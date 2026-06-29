import argparse
import csv
import json
from collections import defaultdict
from pathlib import Path


SKIP_SUFFIXES = (
    ".support.csv",
    ".frequencies.csv",
    ".metadata.json",
)


def is_stream_csv(path: Path) -> bool:
    """
    Return True if this file is a stream CSV, not a support/frequencies file.
    """
    if path.suffix.lower() != ".csv":
        return False

    name = path.name

    for suffix in SKIP_SUFFIXES:
        if name.endswith(suffix):
            return False

    return True


def format_int(value):
    if value is None:
        return "-"
    return f"{value:,}"


def format_float(value, digits=4):
    if value is None:
        return "-"
    return f"{value:.{digits}f}"


def print_table(title, headers, rows):
    """
    Print a simple console table without external libraries.
    """
    print("\n" + title)
    print("=" * len(title))

    if not rows:
        print("(no rows)")
        return

    string_rows = []
    for row in rows:
        string_rows.append([str(x) for x in row])

    widths = []

    for i, header in enumerate(headers):
        max_width = len(str(header))

        for row in string_rows:
            max_width = max(max_width, len(row[i]))

        widths.append(max_width)

    def line():
        return "+-" + "-+-".join("-" * width for width in widths) + "-+"

    print(line())
    print("| " + " | ".join(str(headers[i]).ljust(widths[i]) for i in range(len(headers))) + " |")
    print(line())

    for row in string_rows:
        print("| " + " | ".join(row[i].ljust(widths[i]) for i in range(len(row))) + " |")

    print(line())


def read_support_csv(path: Path):
    support = set()

    with path.open("r", newline="") as f:
        reader = csv.DictReader(f)

        if "item_id" not in reader.fieldnames:
            raise ValueError(f"{path} does not contain an item_id column")

        for row in reader:
            support.add(int(row["item_id"]))

    return support


def read_frequencies_csv(path: Path):
    frequencies = {}

    with path.open("r", newline="") as f:
        reader = csv.DictReader(f)

        required = {"item_id", "final_frequency"}
        if not required.issubset(set(reader.fieldnames or [])):
            raise ValueError(f"{path} must contain item_id and final_frequency columns")

        for row in reader:
            item_id = int(row["item_id"])
            final_frequency = int(row["final_frequency"])
            frequencies[item_id] = final_frequency

    return frequencies


def read_metadata_json(path: Path):
    if not path.exists():
        return {}

    with path.open("r") as f:
        return json.load(f)


def validate_stream(stream_path: Path, validate_strict: bool = True):
    support_path = stream_path.with_suffix(".support.csv")
    frequencies_path = stream_path.with_suffix(".frequencies.csv")
    metadata_path = stream_path.with_suffix(".metadata.json")

    result = {
        "file": stream_path.name,
        "path": str(stream_path),
        "has_support_file": support_path.exists(),
        "has_frequencies_file": frequencies_path.exists(),
        "has_metadata_file": metadata_path.exists(),
        "stream_length": 0,
        "positive_updates": 0,
        "negative_updates": 0,
        "zero_updates": 0,
        "unique_items_seen": 0,
        "expected_support_size": None,
        "computed_support_size": None,
        "missing_from_computed": 0,
        "extra_in_computed": 0,
        "frequency_mismatches": 0,
        "strict_violations": 0,
        "min_final_frequency_nonzero": None,
        "max_final_frequency_nonzero": None,
        "max_abs_delta": 0,
        "total_positive_mass": 0,
        "total_negative_mass": 0,
        "status": "UNKNOWN",
        "issues": [],
        "metadata": {},
        "computed_frequencies": {},
        "computed_support": set(),
        "expected_support": set(),
    }

    if metadata_path.exists():
        try:
            result["metadata"] = read_metadata_json(metadata_path)
        except Exception as e:
            result["issues"].append(f"Could not read metadata: {e}")

    freq = defaultdict(int)

    try:
        with stream_path.open("r", newline="") as f:
            reader = csv.DictReader(f)

            required = {"item_id", "delta"}
            if not required.issubset(set(reader.fieldnames or [])):
                result["issues"].append("Missing required columns item_id and/or delta")
                result["status"] = "FAIL"
                return result

            for row_number, row in enumerate(reader, start=2):
                try:
                    item_id = int(row["item_id"])
                    delta = int(row["delta"])
                except Exception:
                    result["issues"].append(f"Invalid row at line {row_number}: {row}")
                    continue

                result["stream_length"] += 1
                result["max_abs_delta"] = max(result["max_abs_delta"], abs(delta))

                if delta > 0:
                    result["positive_updates"] += 1
                    result["total_positive_mass"] += delta
                elif delta < 0:
                    result["negative_updates"] += 1
                    result["total_negative_mass"] += abs(delta)
                else:
                    result["zero_updates"] += 1

                freq[item_id] += delta

                if validate_strict and freq[item_id] < 0:
                    result["strict_violations"] += 1

                    if result["strict_violations"] <= 5:
                        result["issues"].append(
                            f"Strict turnstile violation at line {row_number}: "
                            f"item {item_id} has temporary frequency {freq[item_id]}"
                        )

    except FileNotFoundError:
        result["issues"].append("Stream file not found")
        result["status"] = "FAIL"
        return result

    computed_frequencies = dict(freq)
    computed_support = {item for item, value in computed_frequencies.items() if value != 0}

    result["computed_frequencies"] = computed_frequencies
    result["computed_support"] = computed_support
    result["unique_items_seen"] = len(computed_frequencies)
    result["computed_support_size"] = len(computed_support)

    nonzero_values = [value for value in computed_frequencies.values() if value != 0]

    if nonzero_values:
        result["min_final_frequency_nonzero"] = min(nonzero_values)
        result["max_final_frequency_nonzero"] = max(nonzero_values)

    if support_path.exists():
        try:
            expected_support = read_support_csv(support_path)
            result["expected_support"] = expected_support
            result["expected_support_size"] = len(expected_support)

            missing = expected_support - computed_support
            extra = computed_support - expected_support

            result["missing_from_computed"] = len(missing)
            result["extra_in_computed"] = len(extra)

            if missing:
                result["issues"].append(
                    f"{len(missing)} expected support items are missing from computed support"
                )

            if extra:
                result["issues"].append(
                    f"{len(extra)} unexpected items appear in computed support"
                )

        except Exception as e:
            result["issues"].append(f"Could not read support file: {e}")
    else:
        result["issues"].append("Missing .support.csv file")

    if frequencies_path.exists():
        try:
            expected_frequencies = read_frequencies_csv(frequencies_path)

            all_items = set(expected_frequencies) | set(computed_frequencies)
            mismatches = []

            for item in all_items:
                expected = expected_frequencies.get(item, 0)
                computed = computed_frequencies.get(item, 0)

                if expected != computed:
                    mismatches.append((item, expected, computed))

            result["frequency_mismatches"] = len(mismatches)

            if mismatches:
                result["issues"].append(
                    f"{len(mismatches)} final frequency mismatches"
                )

        except Exception as e:
            result["issues"].append(f"Could not read frequencies file: {e}")

    # Final status
    if (
        result["has_support_file"]
        and result["missing_from_computed"] == 0
        and result["extra_in_computed"] == 0
        and result["frequency_mismatches"] == 0
        and result["strict_violations"] == 0
        and not any("Missing required columns" in issue for issue in result["issues"])
    ):
        result["status"] = "PASS"
    else:
        result["status"] = "FAIL"

    return result


def write_summary_csv(output_path: Path, results):
    output_path.parent.mkdir(parents=True, exist_ok=True)

    columns = [
        "file",
        "status",
        "stream_length",
        "unique_items_seen",
        "expected_support_size",
        "computed_support_size",
        "positive_updates",
        "negative_updates",
        "zero_updates",
        "max_abs_delta",
        "total_positive_mass",
        "total_negative_mass",
        "missing_from_computed",
        "extra_in_computed",
        "frequency_mismatches",
        "strict_violations",
    ]

    with output_path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=columns)
        writer.writeheader()

        for result in results:
            writer.writerow({col: result.get(col) for col in columns})


def print_summary(results):
    rows = []

    for result in results:
        rows.append([
            result["file"],
            result["status"],
            format_int(result["stream_length"]),
            format_int(result["unique_items_seen"]),
            format_int(result["expected_support_size"]),
            format_int(result["computed_support_size"]),
            format_int(result["positive_updates"]),
            format_int(result["negative_updates"]),
            format_int(result["strict_violations"]),
        ])

    print_table(
        "Validation Summary",
        [
            "file",
            "status",
            "updates",
            "unique",
            "expected |S|",
            "computed |S|",
            "+ updates",
            "- updates",
            "strict viol.",
        ],
        rows,
    )


def print_detail_table(results):
    rows = []

    for result in results:
        rows.append([
            result["file"],
            format_int(result["max_abs_delta"]),
            format_int(result["total_positive_mass"]),
            format_int(result["total_negative_mass"]),
            format_int(result["min_final_frequency_nonzero"]),
            format_int(result["max_final_frequency_nonzero"]),
            format_int(result["missing_from_computed"]),
            format_int(result["extra_in_computed"]),
            format_int(result["frequency_mismatches"]),
        ])

    print_table(
        "Detailed Statistics",
        [
            "file",
            "max |delta|",
            "positive mass",
            "negative mass",
            "min nonzero f",
            "max nonzero f",
            "missing",
            "extra",
            "freq mism.",
        ],
        rows,
    )


def print_metadata_table(results):
    rows = []

    for result in results:
        metadata = result.get("metadata", {})

        rows.append([
            result["file"],
            metadata.get("stream_type", "-"),
            format_int(metadata.get("universe_size")),
            format_int(metadata.get("final_support_size")),
            format_int(metadata.get("cancelled_items")),
            format_int(metadata.get("max_positive_updates")),
            format_int(metadata.get("max_delta")),
            metadata.get("seed", "-"),
        ])

    print_table(
        "Metadata",
        [
            "file",
            "type",
            "universe",
            "support",
            "cancelled",
            "max + updates",
            "max delta",
            "seed",
        ],
        rows,
    )


def print_issues(results):
    rows = []

    for result in results:
        if result["issues"]:
            rows.append([
                result["file"],
                " | ".join(result["issues"][:3]),
            ])

    print_table(
        "Issues",
        ["file", "issues"],
        rows,
    )


def print_support_preview(results, limit: int):
    if limit <= 0:
        return

    for result in results:
        support_items = sorted(result["computed_support"])[:limit]

        rows = []

        for item in support_items:
            rows.append([
                item,
                result["computed_frequencies"][item],
                "yes" if item in result["expected_support"] else "no",
            ])

        print_table(
            f"Support Preview: {result['file']} first {limit} items",
            ["item_id", "final_frequency", "in_expected_support"],
            rows,
        )


def main():
    parser = argparse.ArgumentParser(
        description="Validate all synthetic L0-sampling CSV streams in a directory."
    )

    parser.add_argument(
        "--dir",
        default="data/synthetic",
        help="Directory containing synthetic CSV files",
    )

    parser.add_argument(
        "--recursive",
        action="store_true",
        help="Search recursively inside the directory",
    )

    parser.add_argument(
        "--no-strict",
        action="store_true",
        help="Disable strict turnstile validation",
    )

    parser.add_argument(
        "--show-support",
        type=int,
        default=0,
        help="Print first N support items for each file",
    )

    parser.add_argument(
        "--summary-output",
        default="results/validation_summary.csv",
        help="Where to save the validation summary CSV",
    )

    args = parser.parse_args()

    root = Path(args.dir)

    if not root.exists():
        raise FileNotFoundError(f"Directory not found: {root}")

    if args.recursive:
        candidates = sorted(root.rglob("*.csv"))
    else:
        candidates = sorted(root.glob("*.csv"))

    stream_files = [path for path in candidates if is_stream_csv(path)]

    if not stream_files:
        print(f"No stream CSV files found in {root}")
        return

    results = []

    for stream_path in stream_files:
        result = validate_stream(
            stream_path=stream_path,
            validate_strict=not args.no_strict,
        )
        results.append(result)

    print_summary(results)
    print_detail_table(results)
    print_metadata_table(results)
    print_issues(results)
    print_support_preview(results, args.show_support)

    summary_output = Path(args.summary_output)
    write_summary_csv(summary_output, results)

    passed = sum(1 for r in results if r["status"] == "PASS")
    failed = sum(1 for r in results if r["status"] == "FAIL")

    print("\nFinal result")
    print("============")
    print(f"Validated files: {len(results)}")
    print(f"Passed:          {passed}")
    print(f"Failed:          {failed}")
    print(f"Summary saved:   {summary_output}")


if __name__ == "__main__":
    main()