import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from scripts.generate_synthetic import (
    write_frequencies_csv,
    write_stream_csv,
    write_support_csv,
)
from scripts.validate_synthetic import validate_stream


class ValidationTests(unittest.TestCase):
    def _write_small_dataset(self, directory: str) -> Path:
        stream_path = Path(directory) / "small.csv"
        stream = [(1, 2), (2, 1), (1, -2), (3, 4)]
        write_stream_csv(stream_path, stream)
        write_support_csv(stream_path.with_suffix(".support.csv"), [2, 3])
        write_frequencies_csv(
            stream_path.with_suffix(".frequencies.csv"),
            {1: 0, 2: 1, 3: 4},
        )
        return stream_path

    def test_small_dataset_passes_validation(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parent) as directory:
            result = validate_stream(self._write_small_dataset(directory))
            self.assertEqual(result["status"], "PASS")
            self.assertEqual(result["computed_support"], {2, 3})

    def test_validation_cli_runs_on_small_dataset(self):
        repository_root = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parent) as directory:
            self._write_small_dataset(directory)
            summary_path = Path(directory) / "summary.csv"
            completed = subprocess.run(
                [
                    sys.executable,
                    str(repository_root / "scripts" / "validate_synthetic.py"),
                    "--dir",
                    directory,
                    "--summary-output",
                    str(summary_path),
                ],
                cwd=repository_root,
                capture_output=True,
                text=True,
                check=False,
            )

            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertTrue(summary_path.exists())
            self.assertIn("Passed:          1", completed.stdout)


if __name__ == "__main__":
    unittest.main()
