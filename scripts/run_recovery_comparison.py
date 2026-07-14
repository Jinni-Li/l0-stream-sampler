from __future__ import annotations

import csv
import re
import subprocess
from collections import Counter
from dataclasses import asdict, dataclass
from pathlib import Path

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
    polynomial_degree: int
    base_seed: int
    trial_csv: str
    log_file: str


EXE = Path("cpp/build/l0_sampler.exe")
DATASET = Path("data/synthetic/multi_support_100.csv")

TRIALS = 1000
BASE_SEED = 123

NUM_LEVELS = 32
SPARSITY = 4
RECOVERY_ROWS = 4
RECOVERY_BUCKETS = 8
POLYNOMIAL_DEGREE = 3

FIXED_LEVELS = list(range(13))

OUTPUT_DIRECTORY = Path("results/experiments/recovery_comparison")
SUMMARY_PATH = OUTPUT_DIRECTORY / "recovery_comparison_summary.csv"
SUCCESS_PLOT_PATH = OUTPUT_DIRECTORY / "fixed_level_success_rate.png"
STATUS_PLOT_PATH = OUTPUT_DIRECTORY / "fixed_level_status_breakdown.png"

# True: reuse the existing trial CSV and log files.
# False: rerun every C++ experiment.
REUSE_EXISTING_RESULTS = True


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
) -> str | None:
    if not REUSE_EXISTING_RESULTS:
        return None

    if not trial_csv.exists() or not log_file.exists():
        return None

    print(f"Reusing existing results: {experiment_name}")
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
        "--degree",
        str(POLYNOMIAL_DEGREE),
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
    )

    if combined_output is None:
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
        polynomial_degree=POLYNOMIAL_DEGREE,
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


def main() -> None:
    if not EXE.exists():
        raise FileNotFoundError(
            f"Executable not found: {EXE}. Build the C++ project first."
        )

    if not DATASET.exists():
        raise FileNotFoundError(f"Dataset not found: {DATASET}.")

    OUTPUT_DIRECTORY.mkdir(parents=True, exist_ok=True)

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
