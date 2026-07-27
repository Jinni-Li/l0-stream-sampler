from __future__ import annotations

import argparse
import csv
import statistics
from dataclasses import asdict, dataclass
from pathlib import Path

import matplotlib.pyplot as plt


@dataclass
class RuntimeSummary:
    sampler: str
    measured_trials: int
    successful_trials: int
    failure_trials: int

    update_mean_ns: float
    update_median_ns: float
    update_stddev_ns: float
    update_p95_ns: float

    update_per_item_mean_ns: float
    update_per_item_median_ns: float
    update_per_item_p95_ns: float

    recovery_mean_ns: float
    recovery_median_ns: float
    recovery_stddev_ns: float
    recovery_p95_ns: float

@dataclass
class RuntimeStatusSummary:
    sampler: str
    sample_status: str
    trials: int
    recovery_mean_ns: float
    recovery_median_ns: float
    recovery_p95_ns: float

def percentile(values: list[float], percentile_value: float) -> float:
    if not values:
        raise ValueError("Cannot calculate a percentile of an empty list.")

    ordered = sorted(values)

    position = (len(ordered) - 1) * percentile_value
    lower_index = int(position)
    upper_index = min(lower_index + 1, len(ordered) - 1)
    fraction = position - lower_index

    return (ordered[lower_index] * (1.0 - fraction) + ordered[upper_index] * fraction)


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(
        "r",
        newline="",
        encoding="utf-8-sig",
    ) as file:
        reader = csv.DictReader(file)

        required_columns = {
            "trial",
            "sampler",
            "sample_status",
            "update_time_ns",
            "update_time_per_update_ns",
            "recovery_time_ns",
        }

        if reader.fieldnames is None:
            raise RuntimeError(f"CSV has no header: {path}")

        missing = required_columns.difference(reader.fieldnames)

        if missing:
            raise RuntimeError(
                f"CSV '{path}' is missing columns: {sorted(missing)}"
            )

        rows = list(reader)

    if not rows:
        raise RuntimeError(f"CSV contains no benchmark rows: {path}")

    return rows


def summarize_sampler(sampler: str, rows: list[dict[str, str]]) -> RuntimeSummary:
    selected = [row for row in rows if row["sampler"] == sampler]

    if not selected:
        raise RuntimeError(f"No rows found for sampler: {sampler}")

    update_times = [float(row["update_time_ns"]) for row in selected]

    update_per_item = [float(row["update_time_per_update_ns"]) for row in selected]

    recovery_times = [float(row["recovery_time_ns"]) for row in selected]

    successful_trials = sum(row["sample_status"] == "success" for row in selected)

    return RuntimeSummary(
        sampler=sampler,
        measured_trials=len(selected),
        successful_trials=successful_trials,
        failure_trials=len(selected) - successful_trials,

        update_mean_ns=statistics.fmean(update_times),
        update_median_ns=statistics.median(update_times),
        update_stddev_ns=statistics.stdev(update_times),
        update_p95_ns=percentile(update_times, 0.95),

        update_per_item_mean_ns=statistics.fmean(update_per_item),
        update_per_item_median_ns=statistics.median(update_per_item),
        update_per_item_p95_ns=percentile(update_per_item, 0.95),

        recovery_mean_ns=statistics.fmean(recovery_times),
        recovery_median_ns=statistics.median(recovery_times),
        recovery_stddev_ns=statistics.stdev(recovery_times),
        recovery_p95_ns=percentile(recovery_times, 0.95),
    )

def build_status_summaries(rows: list[dict[str, str]],) -> list[RuntimeStatusSummary]:
    grouped: dict[tuple[str, str],list[float]] = {}

    for row in rows:
        key = (row["sampler"],row["sample_status"])

        grouped.setdefault(key, []).append(float(row["recovery_time_ns"]))

    summaries: list[RuntimeStatusSummary] = []

    for (sampler, sample_status), recovery_times in sorted(grouped.items()):
        summaries.append(
            RuntimeStatusSummary(
                sampler=sampler,
                sample_status=sample_status,
                trials=len(recovery_times),
                recovery_mean_ns=statistics.fmean(recovery_times),
                recovery_median_ns=statistics.median(recovery_times),
                recovery_p95_ns=percentile(recovery_times, 0.95,),
            )
        )

    return summaries

def save_summary(summaries: list[RuntimeSummary], path: Path) -> None:
    rows = [asdict(summary) for summary in summaries]

    with path.open(
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

def save_status_summaries(summaries: list[RuntimeStatusSummary],path: Path,) -> None:
    if not summaries:
        raise ValueError(
            "No runtime status summaries to save."
        )

    rows = [asdict(summary) for summary in summaries]

    with path.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=list(rows[0].keys()),)
        writer.writeheader()
        writer.writerows(rows)

def create_update_plot(
    summaries: list[RuntimeSummary],
    output_path: Path,
) -> None:
    samplers = [summary.sampler for summary in summaries]
    medians = [summary.update_per_item_median_ns for summary in summaries]

    plt.figure(figsize=(7, 5))
    plt.bar(samplers, medians)
    plt.ylabel("Median time per update (ns)")
    plt.title("Update cost by sampler")
    plt.tight_layout()
    plt.savefig(output_path, dpi=200)
    plt.close()


def create_recovery_plot(
    summaries: list[RuntimeSummary],
    output_path: Path,
) -> None:
    samplers = [summary.sampler for summary in summaries]
    medians = [summary.recovery_median_ns for summary in summaries]

    plt.figure(figsize=(7, 5))
    plt.bar(samplers, medians)
    plt.ylabel("Median sample/recovery time (ns)")
    plt.title("Recovery cost by sampler")
    plt.tight_layout()
    plt.savefig(output_path, dpi=200)
    plt.close()


def main() -> None:
    parser = argparse.ArgumentParser(description="Analyze the runtime benchmark CSV.")

    parser.add_argument("input_csv", type=Path,)

    parser.add_argument("--output-dir", type=Path, required=True)

    args = parser.parse_args()

    if not args.input_csv.is_file():
        raise FileNotFoundError(
            f"Benchmark CSV not found: {args.input_csv}"
        )

    args.output_dir.mkdir(parents=True, exist_ok=True,)

    rows = read_rows(args.input_csv)

    samplers = sorted({row["sampler"] for row in rows})

    summaries = [summarize_sampler(sampler, rows)for sampler in samplers]

    summary_path = args.output_dir / "runtime_summary.csv"

    save_summary(summaries,summary_path,)

    status_summaries = build_status_summaries(rows)

    status_summary_path = (args.output_dir / "runtime_status_summary.csv")

    save_status_summaries(status_summaries, status_summary_path)

    create_update_plot(summaries, args.output_dir / "update_time_per_item.png",)

    create_recovery_plot(summaries, args.output_dir / "recovery_time.png",)

    print()
    print(
        f"{'sampler':<14}"
        f"{'trials':<10}"
        f"{'update median/item ns':<24}"
        f"{'recovery median ns':<22}"
        f"{'recovery p95 ns':<18}"
    )
    print("-" * 88)

    for summary in summaries:
        print(
            f"{summary.sampler:<14}"
            f"{summary.measured_trials:<10}"
            f"{summary.update_per_item_median_ns:<24.3f}"
            f"{summary.recovery_median_ns:<22.3f}"
            f"{summary.recovery_p95_ns:<18.3f}"
        )

    print()
    print(f"Summary CSV: {summary_path}")
    print(f"Status summary CSV: {status_summary_path}")


if __name__ == "__main__":
    main()