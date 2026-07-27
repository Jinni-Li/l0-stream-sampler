from __future__ import annotations

import csv
import re
import subprocess
from collections import Counter
from dataclasses import asdict, dataclass
from pathlib import Path
import hashlib
import json
import sys
from datetime import datetime, timezone
import os

import matplotlib.pyplot as plt


@dataclass
class ExperimentResult:
    experiment: str
    dataset: str
    recovery_mode: str
    fixed_level: int | None
    trials: int
    valid_samples: int
    invalid_samples: int
    failures: int
    success_rate: float
    failure_rate: float
    success_count: int
    no_recoverable_level_count: int
    recovery_failure_count: int
    empty_support_count: int
    invalid_sample_count: int
    num_levels: int
    sparsity: int
    recovery_rows: int
    recovery_buckets: int
    hash_independence_k: int
    polynomial_degree: int
    base_seed: int
    trial_csv: str
    log_file: str

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

    # Return the normal CMake/Ninja path so that the
    # eventual error message shows the expected location.
    return candidates[0]

EXE = default_executable_path()
DATASET = Path("data/synthetic/multi_support_100.csv")

TRIALS = 1000
BASE_SEED = 123

NUM_LEVELS = 32
SPARSITY = 4
RECOVERY_ROWS = 4
RECOVERY_BUCKETS = 8
HASH_INDEPENDENCE_K = 4

FIXED_LEVELS = list(range(13))

RUN_LABEL = "final_recovery_support100_v2"

OUTPUT_DIRECTORY = (Path("results/experiments/recovery_comparison") / RUN_LABEL)
SUMMARY_PATH = OUTPUT_DIRECTORY / "recovery_comparison_summary.csv"
SUCCESS_PLOT_PATH = OUTPUT_DIRECTORY / "fixed_level_success_rate.png"
STATUS_PLOT_PATH = OUTPUT_DIRECTORY / "fixed_level_status_breakdown.png"
MANIFEST_SCHEMA_VERSION = 1
MANIFEST_PATH = OUTPUT_DIRECTORY / "run_manifest.json"

# True: reuse results only after validating their configuration.
# False: execute the experiments again.
REUSE_EXISTING_RESULTS = True

# Must be explicitly enabled before replacing existing files.
OVERWRITE_EXISTING_RESULTS = False

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

def extract_count(output: str, label: str) -> int:
    pattern = rf"{re.escape(label)}:\s*(\d+)"
    match = re.search(pattern, output)

    if match is None:
        raise RuntimeError(f"Could not find '{label}' in program output.")

    return int(match.group(1))


def read_status_counts(csv_path: Path) -> Counter[str]:
    if not csv_path.exists():
        raise FileNotFoundError(f"Trial CSV not found: {csv_path}")

    with csv_path.open("r", newline="", encoding="utf-8-sig") as file:
        reader = csv.DictReader(file)

        if reader.fieldnames is None or "status" not in reader.fieldnames:
            raise RuntimeError(
                f"CSV '{csv_path}' does not contain a 'status' column."
            )

        return Counter(
            row["status"].strip().lower()
            for row in reader
            if row.get("status")
        )

def validate_existing_trial_csv(
    csv_path: Path,
    recovery_mode: str,
    fixed_level: int | None,
) -> None:
    required_columns = {
        "trial",
        "status",
        "num_levels",
        "sparsity",
        "recovery_rows",
        "recovery_buckets",
        "hash_independence_k",
        "polynomial_degree",
        "base_seed",
        "recovery_mode",
        "fixed_level",
    }

    with csv_path.open(
        "r",
        newline="",
        encoding="utf-8-sig",
    ) as file:
        reader = csv.DictReader(file)

        if reader.fieldnames is None:
            raise RuntimeError(
                f"CSV '{csv_path}' has no header."
            )

        missing_columns = required_columns.difference(
            reader.fieldnames
        )

        if missing_columns:
            missing_text = ", ".join(
                sorted(missing_columns)
            )

            raise RuntimeError(
                f"CSV '{csv_path}' is incompatible. "
                f"Missing columns: {missing_text}."
            )

        rows = list(reader)

    if len(rows) != TRIALS:
        raise RuntimeError(
            f"CSV '{csv_path}' contains {len(rows)} trials, "
            f"but the current experiment requires {TRIALS}."
        )

    if not rows:
        raise RuntimeError(
            f"CSV '{csv_path}' contains no trial rows."
        )

    first_row = rows[0]

    expected_values = {
        "num_levels": str(NUM_LEVELS),
        "sparsity": str(SPARSITY),
        "recovery_rows": str(RECOVERY_ROWS),
        "recovery_buckets": str(RECOVERY_BUCKETS),
        "hash_independence_k": str(
            HASH_INDEPENDENCE_K
        ),
        "polynomial_degree": str(
            HASH_INDEPENDENCE_K - 1
        ),
        "base_seed": str(BASE_SEED),
        "recovery_mode": recovery_mode,
    }

    if recovery_mode == "fixed":
        expected_values["fixed_level"] = str(fixed_level)

    for column, expected_value in expected_values.items():
        actual_value = first_row[column].strip()

        if actual_value != expected_value:
            raise RuntimeError(
                f"CSV '{csv_path}' is incompatible: "
                f"column '{column}' contains "
                f"'{actual_value}', expected "
                f"'{expected_value}'."
            )

def run_cpp_experiment(
    command: list[str],
    experiment_name: str,
    log_file: Path,
) -> str:
    print()
    print(f"Running {experiment_name}...")
    print("-" * 60)

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
    log_file.write_text(combined_output, encoding="utf-8")

    if completed.returncode != 0:
        raise RuntimeError(
            f"Experiment '{experiment_name}' failed "
            f"with exit code {completed.returncode}."
        )

    return combined_output


def load_existing_output(
    experiment_name: str,
    trial_csv: Path,
    log_file: Path,
    recovery_mode: str,
    fixed_level: int | None,
) -> str | None:
    if not REUSE_EXISTING_RESULTS:
        return None

    if not trial_csv.exists() and not log_file.exists():
        return None

    if not trial_csv.exists() or not log_file.exists():
        raise RuntimeError(
            f"Incomplete existing results for "
            f"'{experiment_name}'. Both the trial CSV "
            f"and log file are required."
        )

    validate_existing_trial_csv(
        trial_csv,
        recovery_mode,
        fixed_level,
    )

    print(
        f"Reusing validated results: {experiment_name}"
    )

    return log_file.read_text(encoding="utf-8")


def run_experiment(
    recovery_mode: str,
    fixed_level: int | None = None,
) -> ExperimentResult:
    if recovery_mode not in {"greedy", "fixed"}:
        raise ValueError("recovery_mode must be 'greedy' or 'fixed'.")

    if recovery_mode == "fixed" and fixed_level is None:
        raise ValueError("fixed_level is required for fixed recovery.")

    experiment_name = (
        "support100_greedy"
        if recovery_mode == "greedy"
        else f"support100_fixed_{fixed_level}"
    )

    trial_csv = OUTPUT_DIRECTORY / f"{experiment_name}.csv"
    log_file = OUTPUT_DIRECTORY / f"{experiment_name}.log"

    command = [
        str(EXE),
        str(DATASET),
        str(TRIALS),
        str(trial_csv),
        "hash",
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
        "--seed",
        str(BASE_SEED),
        "--recovery",
        recovery_mode,
    ]

    if recovery_mode == "fixed":
        command.extend(["--fixed-level", str(fixed_level)])

    combined_output = load_existing_output(
        experiment_name,
        trial_csv,
        log_file,
        recovery_mode,
        fixed_level,
    )

    if combined_output is None:
        existing_paths = [
            path
            for path in (trial_csv, log_file)
            if path.exists()
        ]

        if existing_paths and not OVERWRITE_EXISTING_RESULTS:
            existing_text = ", ".join(
                str(path)
                for path in existing_paths
            )

            raise FileExistsError(
                "Experiment output already exists and "
                "overwriting is disabled: "
                f"{existing_text}. "
                "Enable REUSE_EXISTING_RESULTS or explicitly "
                "set OVERWRITE_EXISTING_RESULTS = True."
            )

        combined_output = run_cpp_experiment(
            command,
            experiment_name,
            log_file,
        )

    valid_samples = extract_count(combined_output, "Valid samples")
    invalid_samples = extract_count(combined_output, "Invalid samples")
    failures = extract_count(combined_output, "Failures")

    status_counts = read_status_counts(trial_csv)

    return ExperimentResult(
        experiment=experiment_name,
        dataset=str(DATASET),
        recovery_mode=recovery_mode,
        fixed_level=fixed_level,
        trials=TRIALS,
        valid_samples=valid_samples,
        invalid_samples=invalid_samples,
        failures=failures,
        success_rate=valid_samples / TRIALS,
        failure_rate=failures / TRIALS,
        success_count=status_counts.get("success", 0),
        no_recoverable_level_count=status_counts.get(
            "no_recoverable_level",
            0,
        ),
        recovery_failure_count=status_counts.get(
            "recovery_failure",
            0,
        ),
        empty_support_count=status_counts.get("empty_support", 0),
        invalid_sample_count=status_counts.get("invalid_sample", 0),
        num_levels=NUM_LEVELS,
        sparsity=SPARSITY,
        recovery_rows=RECOVERY_ROWS,
        recovery_buckets=RECOVERY_BUCKETS,
        hash_independence_k=HASH_INDEPENDENCE_K,
        polynomial_degree=HASH_INDEPENDENCE_K - 1,
        base_seed=BASE_SEED,
        trial_csv=str(trial_csv),
        log_file=str(log_file),
    )


def save_summary(results: list[ExperimentResult]) -> None:
    if not results:
        raise ValueError("No results to save.")

    rows = [asdict(result) for result in results]

    with SUMMARY_PATH.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def fixed_results_only(
    results: list[ExperimentResult],
) -> list[ExperimentResult]:
    return sorted(
        (
            result
            for result in results
            if result.recovery_mode == "fixed"
            and result.fixed_level is not None
        ),
        key=lambda result: result.fixed_level,
    )


def create_success_rate_plot(
    results: list[ExperimentResult],
) -> None:
    fixed_results = fixed_results_only(results)

    levels = [result.fixed_level for result in fixed_results]
    success_rates = [
        result.success_rate * 100
        for result in fixed_results
    ]

    greedy = next(
        result
        for result in results
        if result.recovery_mode == "greedy"
    )

    plt.figure(figsize=(9, 5))
    plt.plot(
        levels,
        success_rates,
        marker="o",
        label="Fixed-level recovery",
    )
    plt.axhline(
        greedy.success_rate * 100,
        linestyle="--",
        label=f"Greedy ({greedy.success_rate * 100:.1f}%)",
    )
    plt.xlabel("Fixed level")
    plt.ylabel("Success rate (%)")
    plt.title("Recovery success rate by fixed level")
    plt.xticks(levels)
    plt.ylim(0, 105)
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()
    plt.savefig(SUCCESS_PLOT_PATH, dpi=200)
    plt.close()


def create_status_breakdown_plot(
    results: list[ExperimentResult],
) -> None:
    fixed_results = fixed_results_only(results)

    levels = [result.fixed_level for result in fixed_results]
    success = [
        result.success_count / result.trials * 100
        for result in fixed_results
    ]
    no_level = [
        result.no_recoverable_level_count / result.trials * 100
        for result in fixed_results
    ]
    recovery_failure = [
        result.recovery_failure_count / result.trials * 100
        for result in fixed_results
    ]
    other = [
        (
            result.empty_support_count
            + result.invalid_sample_count
        )
        / result.trials
        * 100
        for result in fixed_results
    ]

    plt.figure(figsize=(10, 6))
    plt.stackplot(
        levels,
        success,
        no_level,
        recovery_failure,
        other,
        labels=[
            "Success",
            "No recoverable level",
            "Recovery failure",
            "Other",
        ],
    )
    plt.xlabel("Fixed level")
    plt.ylabel("Percentage of trials")
    plt.title("Fixed-level outcome breakdown")
    plt.xticks(levels)
    plt.ylim(0, 100)
    plt.grid(True, alpha=0.3)
    plt.legend(loc="upper right")
    plt.tight_layout()
    plt.savefig(STATUS_PLOT_PATH, dpi=200)
    plt.close()


def print_summary(results: list[ExperimentResult]) -> None:
    print()
    print(
        f"{'mode':<10}"
        f"{'level':<8}"
        f"{'valid':<10}"
        f"{'invalid':<10}"
        f"{'failures':<10}"
        f"{'success':<12}"
        f"{'no_level':<12}"
        f"{'recovery_fail':<15}"
    )
    print("-" * 87)

    for result in results:
        level = "-" if result.fixed_level is None else str(result.fixed_level)

        print(
            f"{result.recovery_mode:<10}"
            f"{level:<8}"
            f"{result.valid_samples:<10}"
            f"{result.invalid_samples:<10}"
            f"{result.failures:<10}"
            f"{result.success_rate:<12.6f}"
            f"{result.no_recoverable_level_count:<12}"
            f"{result.recovery_failure_count:<15}"
        )

def run_git_command(arguments: list[str]) -> str:
    completed = subprocess.run(
        ["git", *arguments],
        capture_output=True,
        text=True,
        check=False,
    )

    if completed.returncode != 0:
        error = completed.stderr.strip()

        raise RuntimeError(
            "Git provenance command failed: "
            f"git {' '.join(arguments)}. "
            f"{error}"
        )

    return completed.stdout.strip()

def build_manifest_identity() -> dict[str, object]:
    git_commit = run_git_command(
        ["rev-parse", "HEAD"]
    )

    git_status = run_git_command(
        ["status", "--porcelain"]
    )

    return {
        "schema_version": MANIFEST_SCHEMA_VERSION,
        "run_label": RUN_LABEL,
        "git": {
            "commit": git_commit,
            "dirty": bool(git_status),
        },
        "dataset": {
            "path": DATASET.as_posix(),
            "sha256": sha256_file(DATASET),
        },
        "executable": {
            "path": EXE.as_posix(),
            "sha256": sha256_file(EXE),
        },
        "environment": {
            "python_version": sys.version,
            "platform": sys.platform,
        },
        "configuration": {
            "trials": TRIALS,
            "base_seed": BASE_SEED,
            "num_levels": NUM_LEVELS,
            "sparsity": SPARSITY,
            "recovery_rows": RECOVERY_ROWS,
            "recovery_buckets": RECOVERY_BUCKETS,
            "hash_independence_k": HASH_INDEPENDENCE_K,
            "polynomial_degree": (
                HASH_INDEPENDENCE_K - 1
            ),
            "recovery_modes": [
                "greedy",
                "fixed",
            ],
            "fixed_levels": FIXED_LEVELS,
        },
    }

def validate_or_create_manifest() -> None:
    expected_identity = build_manifest_identity()

    if MANIFEST_PATH.exists():
        with MANIFEST_PATH.open(
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
                    "current experiment."
                )

        print(
            f"Validated run manifest: {MANIFEST_PATH}"
        )
        return

    existing_outputs = [
        path
        for path in OUTPUT_DIRECTORY.iterdir()
        if path != MANIFEST_PATH
    ]

    if existing_outputs:
        existing_text = ", ".join(
            path.name
            for path in existing_outputs
        )

        raise RuntimeError(
            "Cannot create a new provenance manifest "
            "for a directory that already contains "
            f"experiment outputs: {existing_text}. "
            "Use a new RUN_LABEL or move the existing "
            "results first."
        )

    manifest = {
        "created_at_utc": datetime.now(
            timezone.utc
        ).isoformat(),
        **expected_identity,
    }

    with MANIFEST_PATH.open(
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

    print(f"Created run manifest: {MANIFEST_PATH}")

def main() -> None:
    if not EXE.is_file():
        raise FileNotFoundError(
            f"Executable not found: {EXE}. Build the C++ project first."
        )
    
    if not os.access(EXE, os.X_OK):
        raise PermissionError(
            f"Executable is not marked as executable: {EXE}"
        )

    if not DATASET.exists():
        raise FileNotFoundError(f"Dataset not found: {DATASET}.")

    OUTPUT_DIRECTORY.mkdir(
        parents=True,
        exist_ok=True,
    )

    validate_or_create_manifest()

    results: list[ExperimentResult] = [
        run_experiment(recovery_mode="greedy")
    ]
    
    for fixed_level in FIXED_LEVELS:
        results.append(
            run_experiment(
                recovery_mode="fixed",
                fixed_level=fixed_level,
            )
        )

    save_summary(results)
    create_success_rate_plot(results)
    create_status_breakdown_plot(results)
    print_summary(results)

    print()
    print("Recovery comparison completed.")
    print(f"Summary CSV: {SUMMARY_PATH}")
    print(f"Success-rate plot: {SUCCESS_PLOT_PATH}")
    print(f"Status-breakdown plot: {STATUS_PLOT_PATH}")


if __name__ == "__main__":
    main()
