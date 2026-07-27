from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import platform
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


MANIFEST_SCHEMA_VERSION = 1
BASE_SEED = 123

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
ANALYZER_PATH = (REPOSITORY_ROOT / "scripts" / "analyze_runtime_benchmark.py")
CMAKE_CACHE_PATH = (REPOSITORY_ROOT / "cpp" / "build-release" / "CMakeCache.txt")


def resolve_from_repository(path: Path) -> Path:
    if path.is_absolute():
        return path.resolve()

    return (REPOSITORY_ROOT / path).resolve()


def relative_display_path(path: Path) -> str:
    try:
        return path.resolve().relative_to(REPOSITORY_ROOT).as_posix()
    except ValueError:
        return path.resolve().as_posix()


def default_executable_path() -> Path:
    executable_name = (
        "benchmark_runtime.exe"
        if os.name == "nt"
        else "benchmark_runtime"
    )

    candidates = [
        (REPOSITORY_ROOT / "cpp" / "build-release" / executable_name),
        (REPOSITORY_ROOT / "cpp" / "build-release" / "Release" / executable_name),
    ]

    for candidate in candidates:
        if candidate.is_file():
            return candidate

    return candidates[0]


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()

    with path.open("rb") as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(chunk)

    return digest.hexdigest()


def run_git(arguments: list[str]) -> str:
    completed = subprocess.run(["git", *arguments], cwd=REPOSITORY_ROOT,
        capture_output=True, text=True, check=False)

    if completed.returncode != 0:
        raise RuntimeError(
            "Git command failed:\n"
            f"git {' '.join(arguments)}\n"
            f"{completed.stderr.strip()}"
        )

    return completed.stdout.strip()


def require_clean_repository() -> dict[str, str | bool]:
    status = run_git(
        [
            "status",
            "--porcelain",
            "--untracked-files=all",
        ]
    )

    if status:
        raise RuntimeError(
            "The repository must be clean before running "
            "the final benchmark.\n\n"
            f"{status}"
        )

    return {
        "commit": run_git(["rev-parse", "HEAD"]),
        "branch": run_git(
            ["rev-parse", "--abbrev-ref", "HEAD"]
        ),
        "dirty": False,
        "cleanliness_checked_before_run": True,
    }


def read_cmake_cache_value(cache_path: Path, key: str, *, 
                           required: bool = True) -> str:
    prefix = f"{key}:"

    for line in cache_path.read_text(encoding="utf-8", errors="replace",).splitlines():
        if line.startswith(prefix):
            return line.split("=", 1)[1].strip()

    if required:
        raise RuntimeError(
            f"Could not find '{key}' in {cache_path}."
        )

    return "unknown"


def compiler_metadata(cache_path: Path,) -> dict[str, str]:
    compiler_path = read_cmake_cache_value(cache_path, "CMAKE_CXX_COMPILER",)

    build_type = read_cmake_cache_value(cache_path,"CMAKE_BUILD_TYPE",)

    compiler_id = read_cmake_cache_value(
        cache_path,
        "CMAKE_CXX_COMPILER_ID",
        required=False,
    )

    compiler_version_from_cache = read_cmake_cache_value(
        cache_path,
        "CMAKE_CXX_COMPILER_VERSION",
        required=False,
    )

    generator = read_cmake_cache_value(
        cache_path,
        "CMAKE_GENERATOR",
        required=False,
    )

    completed = subprocess.run(
        [compiler_path, "--version"],
        cwd=REPOSITORY_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )

    if completed.returncode == 0:
        version_lines = completed.stdout.strip().splitlines()
        version_output = (version_lines[0]if version_lines else "unknown")
    else:
        version_output = "unknown"

    return {
        "path": compiler_path,
        "id": compiler_id,
        "version": compiler_version_from_cache,
        "version_output": version_output,
        "build_type": build_type,
        "cmake_generator": generator,
    }


def cmake_version() -> str:
    completed = subprocess.run(["cmake", "--version"], cwd=REPOSITORY_ROOT,
        capture_output=True,text=True, check=False,)

    if completed.returncode != 0:
        return "unknown"

    lines = completed.stdout.strip().splitlines()

    return lines[0] if lines else "unknown"


def read_benchmark_configuration(trials_path: Path,) -> dict[str, int]:
    with trials_path.open( "r", newline="", encoding="utf-8-sig",) as file:
        reader = csv.DictReader(file)
        first_row = next(reader, None)

    if first_row is None:
        raise RuntimeError(
            f"Benchmark output is empty: {trials_path}"
        )

    required_columns = {
        "num_updates",
        "support_size",
        "warmup_trials",
        "num_levels",
        "sparsity",
        "recovery_rows",
        "recovery_buckets",
        "hash_independence_k",
        "polynomial_degree",
    }

    if reader.fieldnames is None:
        raise RuntimeError(
            f"Benchmark CSV has no header: {trials_path}"
        )

    missing = required_columns.difference(reader.fieldnames)

    if missing:
        raise RuntimeError(
            "Benchmark CSV is missing configuration "
            f"columns: {sorted(missing)}"
        )

    return {
        "num_updates": int(first_row["num_updates"]),
        "support_size": int(first_row["support_size"]),
        "warmup_trials": int(
            first_row["warmup_trials"]
        ),
        "num_levels": int(first_row["num_levels"]),
        "sparsity": int(first_row["sparsity"]),
        "recovery_rows": int(
            first_row["recovery_rows"]
        ),
        "recovery_buckets": int(
            first_row["recovery_buckets"]
        ),
        "hash_independence_k": int(
            first_row["hash_independence_k"]
        ),
        "polynomial_degree": int(
            first_row["polynomial_degree"]
        ),
    }


def require_output_file(path: Path) -> None:
    if not path.is_file():
        raise RuntimeError(
            f"Expected output was not generated: {path}"
        )


def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Run the C++ runtime benchmark, analyze its "
            "results, and create a provenance manifest."
        )
    )

    parser.add_argument("dataset",type=Path, help="Input stream CSV.")

    parser.add_argument( "--warmup-trials", type=int, default=50,
        help="Number of unrecorded warm-up trial pairs.")

    parser.add_argument( "--measured-trials", type=int, default=1000,
        help="Number of recorded trial pairs.",)

    parser.add_argument("--exe", type=Path, default=default_executable_path(),
        help="Path to benchmark_runtime executable.",)

    parser.add_argument( "--output-dir", type=Path, required=True,
        help="New empty directory for all run outputs.",)

    args = parser.parse_args()

    dataset_path = resolve_from_repository(args.dataset)
    executable_path = resolve_from_repository(args.exe)
    output_directory = resolve_from_repository(args.output_dir)

    if args.warmup_trials < 0:
        raise ValueError(
            "--warmup-trials must be non-negative."
        )

    if args.measured_trials <= 0:
        raise ValueError(
            "--measured-trials must be positive."
        )

    if not dataset_path.is_file():
        raise FileNotFoundError(
            f"Dataset not found: {dataset_path}"
        )

    if not executable_path.is_file():
        raise FileNotFoundError(
            "Benchmark executable not found: "
            f"{executable_path}"
        )

    if not ANALYZER_PATH.is_file():
        raise FileNotFoundError(
            f"Runtime analyzer not found: {ANALYZER_PATH}"
        )

    if not CMAKE_CACHE_PATH.is_file():
        raise FileNotFoundError(
            "Release CMake cache not found: "
            f"{CMAKE_CACHE_PATH}"
        )

    if output_directory.exists():
        if not output_directory.is_dir():
            raise FileExistsError(
                "Output path exists but is not a directory: "
                f"{output_directory}"
            )

        if any(output_directory.iterdir()):
            raise FileExistsError(
                "Runtime output directory is not empty: "
                f"{output_directory}"
            )

    git_metadata = require_clean_repository()
    build_metadata = compiler_metadata(CMAKE_CACHE_PATH)

    input_hashes = {
        "dataset": sha256_file(dataset_path),
        "executable": sha256_file(executable_path),
        "runner": sha256_file(Path(__file__).resolve()),
        "analyzer": sha256_file(ANALYZER_PATH),
    }

    output_directory.mkdir(parents=True, exist_ok=True,)

    trials_path = (output_directory / "runtime_trials.csv")
    summary_path = (output_directory / "runtime_summary.csv")
    status_summary_path = (output_directory / "runtime_status_summary.csv")
    update_plot_path = (output_directory / "update_time_per_item.png")
    recovery_plot_path = (output_directory / "recovery_time.png")
    manifest_path = (output_directory / "run_manifest.json")

    benchmark_command = [
        str(executable_path),
        str(dataset_path),
        str(args.warmup_trials),
        str(args.measured_trials),
        str(trials_path),
    ]

    analyzer_command = [
        sys.executable,
        str(ANALYZER_PATH),
        str(trials_path),
        "--output-dir",
        str(output_directory),
    ]

    started_at = datetime.now(timezone.utc).isoformat()

    wall_start = time.perf_counter()

    print("Running C++ runtime benchmark...")
    subprocess.run(benchmark_command, cwd=REPOSITORY_ROOT, check=True)

    print()
    print("Analyzing runtime benchmark...")
    subprocess.run(analyzer_command, cwd=REPOSITORY_ROOT, check=True)

    wall_time_seconds = (time.perf_counter() - wall_start)

    completed_at = datetime.now(timezone.utc).isoformat()

    generated_outputs = {
        "runtime_trials": trials_path,
        "runtime_summary": summary_path,
        "runtime_status_summary": status_summary_path,
        "update_time_plot": update_plot_path,
        "recovery_time_plot": recovery_plot_path,
    }

    for output_path in generated_outputs.values():
        require_output_file(output_path)

    benchmark_configuration = (
        read_benchmark_configuration(trials_path)
    )

    cpu_name = (
        platform.processor()
        or os.environ.get(
            "PROCESSOR_IDENTIFIER",
            "unknown",
        )
    )

    manifest_outputs = {}

    for name, output_path in generated_outputs.items():
        manifest_outputs[name] = {
            "path": relative_display_path(output_path),
            "sha256": sha256_file(output_path),
        }

    manifest = {
        "schema_version": MANIFEST_SCHEMA_VERSION,
        "experiment": "runtime_benchmark",
        "created_at_utc": started_at,
        "completed_at_utc": completed_at,
        "wall_time_seconds": wall_time_seconds,
        "git": git_metadata,
        "dataset": {
            "path": relative_display_path(dataset_path),
            "sha256": input_hashes["dataset"],
        },
        "executable": {
            "path": relative_display_path(
                executable_path
            ),
            "sha256": input_hashes["executable"],
        },
        "scripts": {
            "runner": {
                "path": relative_display_path(
                    Path(__file__).resolve()
                ),
                "sha256": input_hashes["runner"],
            },
            "analyzer": {
                "path": relative_display_path(
                    ANALYZER_PATH
                ),
                "sha256": input_hashes["analyzer"],
            },
        },
        "build": {
            **build_metadata,
            "cmake_version": cmake_version(),
        },
        "environment": {
            "operating_system": platform.platform(),
            "machine": platform.machine(),
            "processor": cpu_name,
            "python_version": sys.version,
        },
        "configuration": {
            **benchmark_configuration,
            "measured_trials": args.measured_trials,
            "base_seed": BASE_SEED,
            "samplers": [
                "baseline",
                "hash_greedy",
            ],
            "recovery_mode": "greedy",
        },
        "measurement_protocol": {
            "execution_order": (
                "Alternating baseline/hash order "
                "between trial pairs"
            ),
            "sampler_construction_timed": False,
            "dataset_loading_timed": False,
            "csv_writing_timed": False,
            "update_phase": (
                "All update() calls for one stream pass"
            ),
            "recovery_phase": (
                "One sample() call after all updates"
            ),
            "clock": "std::chrono::steady_clock",
            "reported_unit": "nanoseconds",
        },
        "commands": {
            "benchmark": benchmark_command,
            "analyzer": analyzer_command,
        },
        "outputs": manifest_outputs,
    }

    with manifest_path.open(
        "w",
        encoding="utf-8",
    ) as file:
        json.dump(manifest, file, indent=2, sort_keys=True)
        file.write("\n")

    print()
    print("Runtime experiment completed.")
    print(f"Results: {output_directory}")
    print(f"Manifest: {manifest_path}")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(
            f"Error: {error}",
            file=sys.stderr,
        )
        raise SystemExit(1)