import argparse
import csv
from pathlib import Path


LOBSTER_MESSAGE_COLUMNS = [
    "time",
    "event_type",
    "order_id",
    "size",
    "price",
    "direction",
]


def read_lobster_message_sample(path: Path, rows: int) -> None:
    if not path.exists():
        raise FileNotFoundError(f"File not found: {path}")

    with path.open("r", newline="") as file:
        reader = csv.reader(file)

        print("LOBSTER message file sample")
        print(f"File: {path}")
        print()

        print(",".join(LOBSTER_MESSAGE_COLUMNS))

        for row_number, row in enumerate(reader, start=1):
            if row_number > rows:
                break

            if len(row) != 6:
                print(f"Skipping malformed row {row_number}: {row}")
                continue

            event = dict(zip(LOBSTER_MESSAGE_COLUMNS, row))

            print(
                f"{event['time']},"
                f"{event['event_type']},"
                f"{event['order_id']},"
                f"{event['size']},"
                f"{event['price']},"
                f"{event['direction']}"
            )


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Read a small sample from a LOBSTER message file."
    )

    parser.add_argument(
        "path",
        type=Path,
        help="Path to the LOBSTER message CSV file.",
    )

    parser.add_argument(
        "--rows",
        type=int,
        default=10,
        help="Number of rows to print.",
    )

    args = parser.parse_args()

    read_lobster_message_sample(args.path, args.rows)


if __name__ == "__main__":
    main()