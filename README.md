# Streaming Analytics: L0-Sampling for Dynamic Turnstile Streams

This MSc project implements and evaluates L0-sampling algorithms for dynamic
turnstile data streams. In this model, updates may be positive or negative, and
an L0-sampler should return an item from the final non-zero support.

The repository currently contains a reproducible synthetic-data workflow, an
exact baseline sampler, and a first hash-based C++ prototype built around
1-sparse recovery sketches.

## Project Goal

The goal is to build a reproducible implementation and evaluation workflow for
testing sampler correctness, empirical uniformity, failure probability, runtime,
and memory use.

Controlled synthetic streams with known final support are the primary
validation data. LOBSTER financial order-book data is kept as a later applied
case study.

## Current Status

- Python scripts generate and validate strict-turnstile synthetic streams.
- Included synthetic datasets currently validate successfully:
  `tiny_01.csv`, `multi_tiny.csv`, and `multi_support_100.csv`.
- C++ code includes CSV loading, exact support tracking, an exact baseline
  sampler, a `OneSparseSketch`, and a first `HashBasedL0Sampler`.
- The C++ experiment runner supports `baseline` and `hash` modes and can write
  trial-level CSV output.
- The hash-based prototype returns valid support items when recovery succeeds,
  but it can fail because each level currently uses only one 1-sparse sketch.

The next main algorithmic milestone is replacing the per-level
`OneSparseSketch` with an `s`-sparse recovery structure.

## Repository Structure

- `cpp/include/` - C++ headers for stream updates, readers, trackers, samplers,
  and sketches.
- `cpp/src/` - C++ implementations and the command-line experiment runner.
- `cpp/tests/` - executable C++ regression tests.
- `scripts/` - synthetic-data generation, validation, and LOBSTER sample
  inspection scripts.
- `tests/` - Python `unittest` regression tests for generation and validation.
- `data/synthetic/` - small reproducible synthetic streams and expected support
  files.
- `data/raw/` - local raw datasets such as LOBSTER samples.
- `data/processed/` - local derived datasets and conversions.
- `experiments/` - experiment documentation and future configuration files.
- `results/` - validation summaries and trial-level experiment outputs.
- `src/` - reserved for a future Python package; currently empty.

## Requirements

Python 3.10 or later is recommended. The current Python scripts use only the
standard library.

For the C++ implementation, use CMake 3.16 or later and a C++17 compiler.

## Generate Synthetic Data

Run from the repository root:

```bash
python scripts/generate_synthetic.py \
  --output data/synthetic/example.csv \
  --universe-size 1000 \
  --support-size 50 \
  --cancelled-items 50 \
  --max-positive-updates 5 \
  --max-delta 100 \
  --seed 42
```

The generator writes four files:

- `example.csv` - stream updates with `item_id,delta` columns.
- `example.support.csv` - expected final support.
- `example.frequencies.csv` - expected final frequencies.
- `example.metadata.json` - generation parameters and provenance.

## Validate Synthetic Data

```bash
python scripts/validate_synthetic.py --dir data/synthetic
```

By default, the validation summary is written to
`results/validation_summary.csv`.

Useful options:

```bash
python scripts/validate_synthetic.py --dir data/synthetic --show-support 10
python scripts/validate_synthetic.py --dir data/synthetic --recursive
python scripts/validate_synthetic.py --dir data/synthetic --no-strict
```

## Python Tests

```bash
python -m unittest discover -s tests -v
```

## Build The C++ Prototype

```bash
cmake -S cpp -B cpp/build
cmake --build cpp/build
```

On Windows, the executables are created under `cpp/build/` with `.exe`
extensions. On macOS or Linux, omit `.exe` in the commands below.

## Run C++ Tests

```bash
./cpp/build/test_one_sparse.exe
./cpp/build/test_hash_based.exe
```

## Run Sampling Experiments

The main executable accepts:

```text
l0_sampler <path_to_csv> [trials] [output_csv] [baseline|hash]
```

Run the exact baseline sampler:

```bash
./cpp/build/l0_sampler.exe data/synthetic/tiny_01.csv 1000 results/experiments/tiny_baseline_trials.csv baseline
```

Run the hash-based prototype:

```bash
./cpp/build/l0_sampler.exe data/synthetic/tiny_01.csv 1000 results/experiments/tiny_hash_trials.csv hash
```

The optional output CSV contains one row per trial:

```csv
trial,sample,status
```

The `status` value is `valid`, `invalid`, or `failure`.

## LOBSTER Sample Inspection

The LOBSTER reader currently prints a small message-file preview:

```bash
python scripts/read_lobster_sample.py data/raw/LOBSTER_SampleFile_AMZN_2012-06-21_10/AMZN_2012-06-21_34200000_57600000_message_10.csv --rows 10
```

This is an inspection helper only; the main controlled validation workflow uses
synthetic streams.

## Data And Reference Policy

Raw market data, processed data, downloaded papers, university material, slides,
and other large or copyrighted files should remain local. Put raw datasets in
`data/raw/`, derived datasets in `data/processed/`, and reading material in
`local_references/`.

The repository keeps small synthetic datasets, validation summaries, and compact
experiment outputs when they are useful for reproducibility.

## Known Limitations

- The exact baseline sampler stores the final support explicitly and is used as
  a correctness reference, not as the final streaming-efficient algorithm.
- The current hash-based prototype uses one `OneSparseSketch` per level, so
  recovery only succeeds when a sampled level is exactly 1-sparse.
- The hash prototype uses `SplitMix64` as a practical deterministic mixer; it is
  not yet a full implementation of the theoretical independent hash families.
- The experiment configuration schema and reusable Python package are still
  future work.
