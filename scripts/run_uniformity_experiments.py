from __future__ import annotations

import argparse
import csv
import subprocess
from collections import Counter, defaultdict
from dataclasses import asdict, dataclass
from pathlib import Path
import hashlib
import json
import sys
from datetime import datetime, timezone
import os
import math
from scipy.stats import chisquare

import matplotlib.pyplot as plt

NUM_LEVELS = 32
SPARSITY = 4
RECOVERY_ROWS = 4
RECOVERY_BUCKETS = 8
HASH_INDEPENDENCE_K = 4
MANIFEST_SCHEMA_VERSION = 1

@dataclass
class UniformityRow:
    sampler: str
    trials: int
    seed: int
    item: int
    count: int
    observed_probability: float
    expected_probability: float
    deviation_from_expected: float
    normalized_deviation: float
    absolute_normalized_deviation: float
    successful_trials: int
    failures: int
    invalid_samples: int
    success_rate: float
    source_csv: str

@dataclass
class UniformityMetrics:
    sampler: str
    trials: int
    seed: int
    support_size: int
    successful_trials: int
    failures: int
    invalid_samples: int
    success_rate: float
    expected_count_per_item: float
    mean_absolute_normalized_deviation: float
    maximum_absolute_normalized_deviation: float
    total_variation_distance: float
    chi_square_statistic: float
    chi_square_degrees_of_freedom: int
    chi_square_p_value: float
    source_csv: str

def parse_int_list(value: str) -> list[int]:
    values = [part.strip() for part in value.split(",") if part.strip()]

    if not values:
        raise argparse.ArgumentTypeError(
            "Provide at least one integer."
        )

    try:
        parsed = [int(part) for part in values]
    except ValueError as exc:
        raise argparse.ArgumentTypeError(
            f"Invalid integer list: {value}"
        ) from exc

    if any(number <= 0 for number in parsed):
        raise argparse.ArgumentTypeError(
            "All values must be positive."
        )

    return parsed

def default_executable_path() -> Path:
    executable_name = (
        "l0_sampler.exe"
        if os.name == "nt"
        else "l0_sampler"
    )

    candidates = [
        Path("cpp/build") / executable_name,
        Path("cpp/build/Release") / executable_name,
        Path("cpp/build/Debug") / executable_name,
    ]

    for candidate in candidates:
        if candidate.is_file():
            return candidate

    return candidates[0]

def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Run and visualize ℓ0-sampler uniformity experiments "
            "for any turnstile-stream CSV dataset."
        )
    )

    parser.add_argument(
        "dataset",
        type=Path,
        help="Path to the input stream CSV.",
    )
    parser.add_argument(
        "--exe",
        type=Path,
        default=default_executable_path(),
        help="Path to the compiled sampler executable. "
        "Automatically detects Windows, Linux, and macOS "
        "build paths by default."
    )
    parser.add_argument(
        "--trials",
        type=parse_int_list,
        default=[10_000, 100_000],
        help="Comma-separated trial counts, e.g. 10000,100000.",
    )
    parser.add_argument(
        "--seeds",
        type=parse_int_list,
        default=[123, 100_123, 200_123, 300_123, 400_123],
        help="Comma-separated base seeds, e.g. 123,456,789.",
    )
    parser.add_argument(
        "--samplers",
        choices=["baseline", "hash", "both"],
        default="both",
        help="Which sampler(s) to run.",
    )
    parser.add_argument(
        "--recovery",
        choices=["greedy", "fixed"],
        default="greedy",
        help="Recovery mode used by the hash sampler.",
    )
    parser.add_argument(
        "--fixed-level",
        type=int,
        default=0,
        help="Fixed level when --recovery fixed is selected.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=None,
        help=(
            "Output directory. Defaults to "
            "results/experiments/uniformity_<dataset_stem>."
        ),
    )
    parser.add_argument(
        "--item-column",
        default="item_id",
        help="Dataset column containing the item identifier.",
    )
    parser.add_argument(
        "--delta-column",
        default="delta",
        help="Dataset column containing the turnstile update.",
    )
    parser.add_argument(
        "--reuse",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Reuse existing trial CSVs and logs.",
    )
    parser.add_argument(
        "--overwrite",
        action=argparse.BooleanOptionalAction,
        default=False,
        help=(
            "Allow existing trial CSVs and logs to be "
            "explicitly replaced."
        ),
    )

    return parser


def compute_exact_support(
    dataset: Path,
    item_column: str,
    delta_column: str,
) -> list[int]:
    frequencies: defaultdict[int, int] = defaultdict(int)

    with dataset.open("r", newline="", encoding="utf-8-sig") as file:
        reader = csv.DictReader(file)

        if reader.fieldnames is None:
            raise RuntimeError(
                f"Dataset has no header: {dataset}"
            )

        missing = {
            item_column,
            delta_column,
        } - set(reader.fieldnames)

        if missing:
            raise RuntimeError(
                f"Dataset '{dataset}' is missing columns "
                f"{sorted(missing)}. Available columns: "
                f"{reader.fieldnames}"
            )

        for row_number, row in enumerate(reader, start=2):
            try:
                item = int(row[item_column])
                delta = int(row[delta_column])
            except (TypeError, ValueError) as exc:
                raise RuntimeError(
                    f"Invalid update at row {row_number} "
                    f"in {dataset}."
                ) from exc

            frequencies[item] += delta

    return sorted(
        item
        for item, frequency in frequencies.items()
        if frequency != 0
    )


def sampler_names(selection: str,recovery: str,fixed_level: int) -> list[str]:
    if recovery == "greedy":
        hash_name = "hash_greedy"
    else:
        hash_name = f"hash_fixed_{fixed_level}"

    if selection == "both":
        return ["baseline", hash_name]

    if selection == "baseline":
        return ["baseline"]

    return [hash_name]


def experiment_stem(
    dataset_stem: str,
    sampler: str,
    trials: int,
    seed: int,
) -> str:
    return (
        f"{dataset_stem}_{sampler}_"
        f"{trials}_seed_{seed}"
    )


def build_command(
    exe: Path,
    dataset: Path,
    trials: int,
    seed: int,
    sampler: str,
    output_csv: Path,
    recovery: str,
    fixed_level: int,
) -> list[str]:
    sampler_type = (
        "baseline"
        if sampler == "baseline"
        else "hash"
    )

    command = [
        str(exe),
        str(dataset),
        str(trials),
        str(output_csv),
        sampler_type,
        "--seed",
        str(seed),
    ]

    if sampler_type == "hash":
        command.extend(
            [
                "--levels",
                str(NUM_LEVELS),
                "--sparsity",
                str(SPARSITY),
                "--rows",
                str(RECOVERY_ROWS),
                "--buckets",
                str(RECOVERY_BUCKETS),
                "--hash-k",
                str(HASH_INDEPENDENCE_K),
                "--recovery",
                recovery,
            ]
        )

        if recovery == "fixed":
            command.extend(
                [
                    "--fixed-level",
                    str(fixed_level),
                ]
            )

    return command


def validate_existing_trial_csv(
    csv_path: Path,
    trials: int,
    seed: int,
    sampler: str,
    recovery: str,
    fixed_level: int,
) -> None:
    required_columns = {
        "trial",
        "sample",
        "status",
        "base_seed",
    }

    if sampler != "baseline":
        required_columns.update(
            {
                "num_levels",
                "sparsity",
                "recovery_rows",
                "recovery_buckets",
                "hash_independence_k",
                "polynomial_degree",
                "recovery_mode",
                "fixed_level",
            }
        )

    with csv_path.open(
        "r",
        newline="",
        encoding="utf-8-sig",
    ) as file:
        reader = csv.DictReader(file)

        if reader.fieldnames is None:
            raise RuntimeError(
                f"Trial CSV has no header: {csv_path}"
            )

        missing_columns = required_columns.difference(
            reader.fieldnames
        )

        if missing_columns:
            raise RuntimeError(
                f"CSV '{csv_path}' is incompatible. "
                f"Missing columns: "
                f"{sorted(missing_columns)}."
            )

        rows = list(reader)

    if len(rows) != trials:
        raise RuntimeError(
            f"CSV '{csv_path}' contains {len(rows)} trials, "
            f"but {trials} are required."
        )

    if not rows:
        raise RuntimeError(
            f"CSV '{csv_path}' contains no rows."
        )

    expected_values = {
        "base_seed": str(seed),
    }

    if sampler != "baseline":
        expected_values.update(
            {
                "num_levels": str(NUM_LEVELS),
                "sparsity": str(SPARSITY),
                "recovery_rows": str(RECOVERY_ROWS),
                "recovery_buckets": str(
                    RECOVERY_BUCKETS
                ),
                "hash_independence_k": str(
                    HASH_INDEPENDENCE_K
                ),
                "polynomial_degree": str(
                    HASH_INDEPENDENCE_K - 1
                ),
                "recovery_mode": recovery,
            }
        )

        if recovery == "fixed":
            expected_values["fixed_level"] = str(
                fixed_level
            )

    for row_number, row in enumerate(rows, start=2):
        for column, expected_value in expected_values.items():
            actual_value = row[column].strip()

            if actual_value != expected_value:
                raise RuntimeError(
                    f"CSV '{csv_path}' is incompatible "
                    f"at row {row_number}: "
                    f"column '{column}' contains "
                    f"'{actual_value}', expected "
                    f"'{expected_value}'."
                )
            
def run_experiment(
    command: list[str],
    output_csv: Path,
    log_path: Path,
    experiment_name: str,
    reuse: bool,
    overwrite: bool,
    trials: int,
    seed: int,
    sampler: str,
    recovery: str,
    fixed_level: int,
) -> None:
    csv_exists = output_csv.exists()
    log_exists = log_path.exists()

    if reuse:
        if csv_exists and log_exists:
            validate_existing_trial_csv(
                output_csv,
                trials,
                seed,
                sampler,
                recovery,
                fixed_level,
            )
            print(f"Reusing validated result: {experiment_name}")
            return

        if csv_exists or log_exists:
            raise RuntimeError(
                f"Incomplete existing result for '{experiment_name}'. "
                "Both CSV and log are required for reuse."
            )

    if (csv_exists or log_exists) and not overwrite:
        raise FileExistsError(
            f"Output already exists for '{experiment_name}'. "
            "Use --reuse for compatible results or "
            "--overwrite to replace them explicitly."
        )

    print()
    print(f"Running {experiment_name}")
    print("-" * 70)

    completed = subprocess.run(
        command,
        capture_output=True,
        text=True,
        check=False,
    )

    combined_output = completed.stdout

    if completed.stderr:
        combined_output += "\n" + completed.stderr

    print(combined_output)

    log_path.write_text(
        combined_output,
        encoding="utf-8",
    )

    if completed.returncode != 0:
        raise RuntimeError(
            f"Experiment '{experiment_name}' failed "
            f"with exit code {completed.returncode}."
        )


def read_trial_csv(
    path: Path,
) -> tuple[Counter[int], int, int, int]:
    counts: Counter[int] = Counter()
    successful_trials = 0
    failures = 0
    invalid_samples = 0

    with path.open(
        "r",
        newline="",
        encoding="utf-8-sig",
    ) as file:
        reader = csv.DictReader(file)

        if reader.fieldnames is None:
            raise RuntimeError(
                f"Trial CSV has no header: {path}"
            )

        required = {"sample", "status"}
        missing = required - set(reader.fieldnames)

        if missing:
            raise RuntimeError(
                f"Trial CSV '{path}' is missing columns "
                f"{sorted(missing)}."
            )

        for row in reader:
            status = row["status"].strip().lower()
            sample = row["sample"].strip()

            if status == "success":
                if sample == "":
                    raise RuntimeError(
                        f"Success row without sample in {path}."
                    )

                counts[int(sample)] += 1
                successful_trials += 1

            elif status == "invalid_sample":
                invalid_samples += 1

            else:
                failures += 1

    return (
        counts,
        successful_trials,
        failures,
        invalid_samples,
    )


def build_summary(
    output_dir: Path,
    dataset_stem: str,
    samplers: list[str],
    trials_values: list[int],
    seeds: list[int],
    support: list[int],
) -> list[UniformityRow]:
    expected_probability = 1 / len(support)
    rows: list[UniformityRow] = []

    for trials in trials_values:
        for seed in seeds:
            for sampler in samplers:
                stem = experiment_stem(
                    dataset_stem,
                    sampler,
                    trials,
                    seed,
                )
                path = output_dir / f"{stem}.csv"

                (
                    counts,
                    successful_trials,
                    failures,
                    invalid_samples,
                ) = read_trial_csv(path)

                for item in support:
                    count = counts.get(item, 0)
                    observed_probability = (
                        count / successful_trials
                        if successful_trials > 0
                        else 0.0
                    )

                    deviation = (
                        observed_probability
                        - expected_probability
                    )

                    normalized_deviation = (
                        deviation / expected_probability
                    )
                    
                    rows.append(

                        UniformityRow(
                            sampler=sampler,
                            trials=trials,
                            seed=seed,
                            item=item,
                            count=count,
                            observed_probability=observed_probability,
                            expected_probability=expected_probability,
                            deviation_from_expected=deviation,
                            normalized_deviation=normalized_deviation,
                            absolute_normalized_deviation=abs(
                                normalized_deviation
                            ),
                            successful_trials=successful_trials,
                            failures=failures,
                            invalid_samples=invalid_samples,
                            success_rate=(
                                successful_trials / trials
                            ),
                            source_csv=str(path),
                        )
                    )



    return rows



def build_uniformity_metrics(
    rows: list[UniformityRow],
) -> list[UniformityMetrics]:
    grouped: dict[
        tuple[str, int, int],
        list[UniformityRow],
    ] = {}

    for row in rows:
        key = (
            row.sampler,
            row.trials,
            row.seed,
        )
        grouped.setdefault(key, []).append(row)

    metrics: list[UniformityMetrics] = []

    for (
        sampler,
        trials,
        seed,
    ), selected_rows in sorted(grouped.items()):
        selected_rows = sorted(
            selected_rows,
            key=lambda row: row.item,
        )

        support_size = len(selected_rows)
        successful_trials = (
            selected_rows[0].successful_trials
        )
        failures = selected_rows[0].failures
        invalid_samples = (
            selected_rows[0].invalid_samples
        )

        observed_probabilities = [
            row.observed_probability
            for row in selected_rows
        ]

        expected_probability = (
            selected_rows[0].expected_probability
        )

        normalized_deviations = [
            row.absolute_normalized_deviation
            for row in selected_rows
        ]

        total_variation_distance = (
            0.5
            * sum(
                abs(
                    probability
                    - expected_probability
                )
                for probability
                in observed_probabilities
            )
        )

        expected_count = (
            successful_trials / support_size
            if support_size > 0
            else 0.0
        )

        if successful_trials > 0 and support_size > 1:
            observed_counts = [
                row.count
                for row in selected_rows
            ]

            expected_counts = [
                expected_count
            ] * support_size

            chi_result = chisquare(
                f_obs=observed_counts,
                f_exp=expected_counts,
            )

            chi_square_statistic = float(
                chi_result.statistic
            )
            chi_square_p_value = float(
                chi_result.pvalue
            )
        elif successful_trials > 0:
            # A one-item support is trivially uniform.
            chi_square_statistic = 0.0
            chi_square_p_value = 1.0
        else:
            chi_square_statistic = math.nan
            chi_square_p_value = math.nan

        metrics.append(
            UniformityMetrics(
                sampler=sampler,
                trials=trials,
                seed=seed,
                support_size=support_size,
                successful_trials=successful_trials,
                failures=failures,
                invalid_samples=invalid_samples,
                success_rate=(
                    successful_trials / trials
                ),
                expected_count_per_item=expected_count,
                mean_absolute_normalized_deviation=(
                    sum(normalized_deviations)
                    / support_size
                ),
                maximum_absolute_normalized_deviation=max(
                    normalized_deviations
                ),
                total_variation_distance=(
                    total_variation_distance
                ),
                chi_square_statistic=(
                    chi_square_statistic
                ),
                chi_square_degrees_of_freedom=(
                    support_size - 1
                ),
                chi_square_p_value=(
                    chi_square_p_value
                ),
                source_csv=(
                    selected_rows[0].source_csv
                ),
            )
        )

    return metrics

def save_summary(
    rows: list[UniformityRow],
    summary_path: Path,
) -> None:
    dictionaries = [asdict(row) for row in rows]

    with summary_path.open(
        "w",
        newline="",
        encoding="utf-8",
    ) as file:
        writer = csv.DictWriter(
            file,
            fieldnames=list(dictionaries[0].keys()),
        )
        writer.writeheader()
        writer.writerows(dictionaries)

def save_metrics(
    metrics: list[UniformityMetrics],
    output_path: Path,
) -> None:
    if not metrics:
        raise ValueError(
            "No uniformity metrics to save."
        )

    rows = [
        asdict(metric)
        for metric in metrics
    ]

    with output_path.open(
        "w",
        newline="",
        encoding="utf-8",
    ) as file:
        writer = csv.DictWriter(
            file,
            fieldnames=list(rows[0].keys()),
        )
        writer.writeheader()
        writer.writerows(rows)

def select_rows(
    rows: list[UniformityRow],
    sampler: str,
    trials: int,
    seed: int,
) -> list[UniformityRow]:
    return sorted(
        (
            row
            for row in rows
            if row.sampler == sampler
            and row.trials == trials
            and row.seed == seed
        ),
        key=lambda row: row.item,
    )


def create_frequency_plot(
    rows: list[UniformityRow],
    output_dir: Path,
    dataset_stem: str,
    sampler: str,
    trials: int,
    seed: int,
) -> None:
    selected = select_rows(
        rows,
        sampler,
        trials,
        seed,
    )

    support_size = len(selected)
    positions = list(range(support_size))
    counts = [row.count for row in selected]
    expected_count = (
        selected[0].successful_trials / support_size
    )

    width = max(8, min(16, support_size / 6))

    plt.figure(figsize=(width, 6))
    plt.bar(positions, counts)
    plt.axhline(
        expected_count,
        linestyle="--",
        label=f"Uniform expectation: {expected_count:.1f}",
    )
    plt.xlabel("Support item index")
    plt.ylabel("Sample count")
    plt.title(
        f"{dataset_stem} — {sampler}\n"
        f"{trials:,} trials, seed {seed}"
    )
    plt.legend()
    plt.tight_layout()
    plt.savefig(
        output_dir
        / (
            f"{dataset_stem}_{sampler}_{trials}_"
            f"seed_{seed}_frequency.png"
        ),
        dpi=200,
    )
    plt.close()


def create_probability_histogram(
    rows: list[UniformityRow],
    output_dir: Path,
    dataset_stem: str,
    sampler: str,
    trials: int,
    seed: int,
) -> None:
    selected = select_rows(
        rows,
        sampler,
        trials,
        seed,
    )

    probabilities = [
        row.observed_probability
        for row in selected
    ]

    expected = selected[0].expected_probability
    bins = min(20, max(5, len(selected) // 5))

    plt.figure(figsize=(8, 5))
    plt.hist(probabilities, bins=bins)
    plt.axvline(
        expected,
        linestyle="--",
        label=f"Expected: {expected:.6f}",
    )
    plt.xlabel("Observed probability per item")
    plt.ylabel("Number of support items")
    plt.title(
        f"{dataset_stem} — {sampler}\n"
        f"Probability distribution, {trials:,} trials"
    )
    plt.legend()
    plt.tight_layout()
    plt.savefig(
        output_dir
        / (
            f"{dataset_stem}_{sampler}_{trials}_"
            f"seed_{seed}_probability_histogram.png"
        ),
        dpi=200,
    )
    plt.close()


def create_deviation_plot(
    rows: list[UniformityRow],
    output_dir: Path,
    dataset_stem: str,
    sampler: str,
    trials: int,
    seed: int,
) -> None:
    selected = select_rows(
        rows,
        sampler,
        trials,
        seed,
    )
    ranked = sorted(
        selected,
        key=lambda row: row.deviation_from_expected,
    )

    positions = list(range(len(ranked)))
    deviations = [
        row.deviation_from_expected
        for row in ranked
    ]

    width = max(8, min(16, len(ranked) / 6))

    plt.figure(figsize=(width, 6))
    plt.bar(positions, deviations)
    plt.axhline(0, linestyle="--")
    plt.xlabel("Support items ranked by deviation")
    plt.ylabel("Observed probability - expected probability")
    plt.title(
        f"{dataset_stem} — {sampler}\n"
        f"Deviation from uniformity, {trials:,} trials"
    )
    plt.tight_layout()
    plt.savefig(
        output_dir
        / (
            f"{dataset_stem}_{sampler}_{trials}_"
            f"seed_{seed}_deviation.png"
        ),
        dpi=200,
    )
    plt.close()


def create_sampler_comparison(
    rows: list[UniformityRow],
    output_dir: Path,
    dataset_stem: str,
    trials: int,
    seed: int,
    hash_sampler: str,
) -> None:
    baseline = select_rows(
        rows,
        "baseline",
        trials,
        seed,
    )
    hash_rows = select_rows(
        rows,
        hash_sampler,
        trials,
        seed,
    )

    if not baseline or not hash_rows:
        return

    baseline_map = {
        row.item: row.observed_probability
        for row in baseline
    }
    hash_map = {
        row.item: row.observed_probability
        for row in hash_rows
    }

    common_items = sorted(
        set(baseline_map) & set(hash_map)
    )

    x_values = [
        baseline_map[item]
        for item in common_items
    ]
    y_values = [
        hash_map[item]
        for item in common_items
    ]

    lower = min(x_values + y_values)
    upper = max(x_values + y_values)

    plt.figure(figsize=(7, 7))
    plt.scatter(x_values, y_values)
    plt.plot(
        [lower, upper],
        [lower, upper],
        linestyle="--",
    )
    plt.xlabel("Baseline observed probability")
    plt.ylabel(f"{hash_sampler} observed probability")
    plt.title(
        f"{dataset_stem}: baseline vs {hash_sampler}\n"
        f"{trials:,} trials, seed {seed}"
    )
    plt.tight_layout()
    plt.savefig(
        output_dir
        / (
            f"{dataset_stem}_comparison_{trials}_"
            f"seed_{seed}.png"
        ),
        dpi=200,
    )
    plt.close()


def print_run_summary(
    rows: list[UniformityRow],
) -> None:
    print()
    print(
        f"{'sampler':<14}"
        f"{'trials':<10}"
        f"{'seed':<8}"
        f"{'successes':<12}"
        f"{'failures':<10}"
        f"{'invalid':<10}"
        f"{'success rate':<14}"
    )
    print("-" * 78)

    seen: set[tuple[str, int, int]] = set()

    for row in rows:
        key = (
            row.sampler,
            row.trials,
            row.seed,
        )

        if key in seen:
            continue

        seen.add(key)

        print(
            f"{row.sampler:<14}"
            f"{row.trials:<10}"
            f"{row.seed:<8}"
            f"{row.successful_trials:<12}"
            f"{row.failures:<10}"
            f"{row.invalid_samples:<10}"
            f"{row.success_rate:<14.6f}"
        )

def sha256_file(path: Path) -> str:
    if not path.exists():
        raise FileNotFoundError(
            f"Cannot calculate SHA-256. File not found: {path}"
        )

    digest = hashlib.sha256()

    with path.open("rb") as file:
        for chunk in iter(
            lambda: file.read(1024 * 1024),
            b"",
        ):
            digest.update(chunk)

    return digest.hexdigest()


def run_git_command(arguments: list[str]) -> str:
    completed = subprocess.run(
        ["git", *arguments],
        capture_output=True,
        text=True,
        check=False,
    )

    if completed.returncode != 0:
        raise RuntimeError(
            "Git provenance command failed: "
            f"git {' '.join(arguments)}. "
            f"{completed.stderr.strip()}"
        )

    return completed.stdout.strip()

def build_manifest_identity(
    args: argparse.Namespace,
    output_dir: Path,
    samplers: list[str],
) -> dict[str, object]:
    git_commit = run_git_command(
        ["rev-parse", "HEAD"]
    )

    git_status = run_git_command(
        ["status", "--porcelain"]
    )

    return {
        "schema_version": MANIFEST_SCHEMA_VERSION,
        "experiment": "uniformity",
        "run_label": output_dir.name,
        "git": {
            "commit": git_commit,
            "dirty": bool(git_status),
        },
        "dataset": {
            "path": args.dataset.as_posix(),
            "sha256": sha256_file(args.dataset),
            "item_column": args.item_column,
            "delta_column": args.delta_column,
        },
        "executable": {
            "path": args.exe.as_posix(),
            "sha256": sha256_file(args.exe),
        },
        "environment": {
            "python_version": sys.version,
            "platform": sys.platform,
        },
        "configuration": {
            "trials": args.trials,
            "seeds": args.seeds,
            "samplers": samplers,
            "recovery_mode": args.recovery,
            "fixed_level": (
                args.fixed_level
                if args.recovery == "fixed"
                else None
            ),
            "num_levels": NUM_LEVELS,
            "sparsity": SPARSITY,
            "recovery_rows": RECOVERY_ROWS,
            "recovery_buckets": RECOVERY_BUCKETS,
            "hash_independence_k": HASH_INDEPENDENCE_K,
            "polynomial_degree": (
                HASH_INDEPENDENCE_K - 1
            ),
        },
    }

def validate_or_create_manifest(
    args: argparse.Namespace,
    output_dir: Path,
    samplers: list[str],
) -> None:
    manifest_path = output_dir / "run_manifest.json"

    expected_identity = build_manifest_identity(
        args,
        output_dir,
        samplers,
    )

    if manifest_path.exists():
        with manifest_path.open(
            "r",
            encoding="utf-8",
        ) as file:
            existing_manifest = json.load(file)

        for key, expected_value in expected_identity.items():
            actual_value = existing_manifest.get(key)

            if actual_value != expected_value:
                raise RuntimeError(
                    "Existing run manifest is incompatible: "
                    f"field '{key}' does not match the "
                    "current uniformity experiment."
                )

        print(
            f"Validated run manifest: {manifest_path}"
        )
        return

    existing_outputs = [
        path
        for path in output_dir.iterdir()
        if path != manifest_path
    ]

    if existing_outputs:
        raise RuntimeError(
            "Cannot create a provenance manifest in a "
            "directory that already contains experiment "
            "outputs. Use a new --output-dir or remove the "
            "smoke-test results first."
        )

    manifest = {
        "created_at_utc": datetime.now(
            timezone.utc
        ).isoformat(),
        **expected_identity,
    }

    with manifest_path.open(
        "w",
        encoding="utf-8",
    ) as file:
        json.dump(
            manifest,
            file,
            indent=2,
            sort_keys=True,
        )
        file.write("\n")

    print(f"Created run manifest: {manifest_path}")

def main() -> None:
    args = build_parser().parse_args()

    if not args.exe.is_file():
        raise FileNotFoundError(
            f"Executable not found: {args.exe}"
        )

    if not os.access(args.exe, os.X_OK):
        raise PermissionError(
            f"Executable is not marked as executable: {args.exe}"
        )

    if not args.dataset.exists():
        raise FileNotFoundError(
            f"Dataset not found: {args.dataset}"
        )

    if args.fixed_level < 0:
        raise ValueError(
            "--fixed-level must be non-negative."
        )

    dataset_stem = args.dataset.stem
    output_dir = (
        args.output_dir
        if args.output_dir is not None
        else Path(
            f"results/experiments/uniformity_{dataset_stem}"
        )
    )

    support = compute_exact_support(
        args.dataset,
        args.item_column,
        args.delta_column,
    )
    if not support:
        raise RuntimeError(
            "The dataset has empty final support."
        )

    output_dir.mkdir(
        parents=True,
        exist_ok=True,
    )

    samplers = sampler_names(
        args.samplers,
        args.recovery,
        args.fixed_level,
    )
    
    validate_or_create_manifest(
        args,
        output_dir,
        samplers,
    )



    print(f"Dataset: {args.dataset}")
    print(f"Final support size: {len(support)}")
    print(f"Output directory: {output_dir}")

    for trials in args.trials:
        for seed in args.seeds:
            for sampler in samplers:
                stem = experiment_stem(
                    dataset_stem,
                    sampler,
                    trials,
                    seed,
                )
                output_csv = output_dir / f"{stem}.csv"
                log_path = output_dir / f"{stem}.log"

                command = build_command(
                    args.exe,
                    args.dataset,
                    trials,
                    seed,
                    sampler,
                    output_csv,
                    args.recovery,
                    args.fixed_level,
                )

                run_experiment(
                    command,
                    output_csv,
                    log_path,
                    stem,
                    args.reuse,
                    args.overwrite,
                    trials,
                    seed,
                    sampler,
                    args.recovery,
                    args.fixed_level,
                )

    rows = build_summary(
        output_dir,
        dataset_stem,
        samplers,
        args.trials,
        args.seeds,
        support,
    )

    summary_path = (
        output_dir
        / f"{dataset_stem}_uniformity_summary.csv"
    )
    save_summary(rows, summary_path)

    metrics = build_uniformity_metrics(rows)

    metrics_path = (
        output_dir
        / f"{dataset_stem}_uniformity_metrics.csv"
    )

    save_metrics(
        metrics,
        metrics_path,
    )

    for trials in args.trials:
        for seed in args.seeds:
            for sampler in samplers:
                create_frequency_plot(
                    rows,
                    output_dir,
                    dataset_stem,
                    sampler,
                    trials,
                    seed,
                )
                create_probability_histogram(
                    rows,
                    output_dir,
                    dataset_stem,
                    sampler,
                    trials,
                    seed,
                )
                create_deviation_plot(
                    rows,
                    output_dir,
                    dataset_stem,
                    sampler,
                    trials,
                    seed,
                )

            hash_samplers = [
                sampler
                for sampler in samplers
                if sampler != "baseline"
            ]

            if "baseline" in samplers and len(hash_samplers) == 1:
                create_sampler_comparison(
                    rows,
                    output_dir,
                    dataset_stem,
                    trials,
                    seed,
                    hash_samplers[0],
                )

    print_run_summary(rows)

    print()
    print("Uniformity experiment and visualization completed.")
    print(f"Summary CSV: {summary_path}")
    print(f"Plots directory: {output_dir}")
    print(f"Metrics CSV: {metrics_path}")

if __name__ == "__main__":
    main()
