from __future__ import annotations

import argparse
import csv
import subprocess
from collections import Counter, defaultdict
from dataclasses import asdict, dataclass
from pathlib import Path

import matplotlib.pyplot as plt


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
    successful_trials: int
    failures: int
    invalid_samples: int
    success_rate: float
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
        default=Path("cpp/build/l0_sampler.exe"),
        help="Path to the compiled sampler executable.",
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


def sampler_names(selection: str) -> list[str]:
    if selection == "both":
        return ["baseline", "hash_greedy"]
    if selection == "baseline":
        return ["baseline"]
    return ["hash_greedy"]


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


def run_experiment(
    command: list[str],
    output_csv: Path,
    log_path: Path,
    experiment_name: str,
    reuse: bool,
) -> None:
    if reuse and output_csv.exists() and log_path.exists():
        print(f"Reusing existing result: {experiment_name}")
        return

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

                    rows.append(
                        UniformityRow(
                            sampler=sampler,
                            trials=trials,
                            seed=seed,
                            item=item,
                            count=count,
                            observed_probability=observed_probability,
                            expected_probability=expected_probability,
                            deviation_from_expected=(
                                observed_probability
                                - expected_probability
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
) -> None:
    baseline = select_rows(
        rows,
        "baseline",
        trials,
        seed,
    )
    hash_rows = select_rows(
        rows,
        "hash_greedy",
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
    plt.ylabel("Hash greedy observed probability")
    plt.title(
        f"{dataset_stem}: baseline vs hash greedy\n"
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


def main() -> None:
    args = build_parser().parse_args()

    if not args.exe.exists():
        raise FileNotFoundError(
            f"Executable not found: {args.exe}"
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
    output_dir.mkdir(parents=True, exist_ok=True)

    support = compute_exact_support(
        args.dataset,
        args.item_column,
        args.delta_column,
    )

    if not support:
        raise RuntimeError(
            "The dataset has empty final support."
        )

    samplers = sampler_names(args.samplers)

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

            if set(samplers) == {
                "baseline",
                "hash_greedy",
            }:
                create_sampler_comparison(
                    rows,
                    output_dir,
                    dataset_stem,
                    trials,
                    seed,
                )

    print_run_summary(rows)

    print()
    print("Uniformity experiment and visualization completed.")
    print(f"Summary CSV: {summary_path}")
    print(f"Plots directory: {output_dir}")


if __name__ == "__main__":
    main()
