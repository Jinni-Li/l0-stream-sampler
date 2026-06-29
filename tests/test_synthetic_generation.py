import csv
import tempfile
import unittest
from pathlib import Path

from scripts.generate_synthetic import (
    compute_final_frequencies,
    compute_final_support,
    generate_multi_update_stream,
    write_stream_csv,
    write_support_csv,
)
from scripts.validate_synthetic import read_support_csv


class SyntheticGenerationTests(unittest.TestCase):
    def setUp(self):
        self.stream, self.support, _ = generate_multi_update_stream(
            universe_size=50,
            final_support_size=4,
            cancelled_items=3,
            max_positive_updates=3,
            max_delta=5,
            seed=7,
        )

    def test_stream_csv_has_expected_columns(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parent) as directory:
            stream_path = Path(directory) / "small.csv"
            write_stream_csv(stream_path, self.stream)

            with stream_path.open(newline="") as handle:
                reader = csv.DictReader(handle)
                self.assertEqual(reader.fieldnames, ["item_id", "delta"])

    def test_support_file_matches_generated_stream(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parent) as directory:
            support_path = Path(directory) / "small.support.csv"
            write_support_csv(support_path, self.support)

            computed = compute_final_support(compute_final_frequencies(self.stream))
            self.assertEqual(read_support_csv(support_path), set(computed))


if __name__ == "__main__":
    unittest.main()
