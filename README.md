# Streaming Analytics: L0-Sampling for Dynamic Turnstile Streams

This MSc project implements and evaluates L0-sampling algorithms for dynamic turnstile data streams. In this model, updates may be positive or negative, and an L0-sampler should return an item from the final non-zero support.

The repository currently contains a reproducible synthetic-data workflow, an exact baseline sampler, and a configurable hash-based C++ implementation following the sampling, sparse-recovery, and selection structure of the Cormode–Firmani framework.

## Project Goal

The goal is to build a reproducible implementation and evaluation workflow for testing sampler correctness, empirical uniformity, failure probability, runtime, and memory use.

Controlled synthetic streams with known final support are the primary validation data. LOBSTER financial order-book data is kept as a later applied case study.

## Current Status

- Python scripts generate and validate strict-turnstile synthetic streams with known final support.
- Included synthetic datasets include `tiny_01.csv`, `multi_tiny.csv`, and `multi_support_100.csv`.
- The C++ implementation includes CSV loading, exact support tracking, an exact baseline sampler, `OneSparseSketch`, `SSparseSketch`, configurable hash families, and `HashBasedL0Sampler`.
- Sampler parameters are managed through `SamplerConfig`, including the number of levels, sparsity, recovery rows and buckets, polynomial degree, seed, and recovery mode.
- The experiment runner supports exact baseline, greedy recovery, and fixed-level recovery, with trial-level CSV output.
- Initial synthetic experiments measure validity, recovery success, fixed-level behaviour, and empirical sampling frequencies.
- Current recovery and uniformity results are preliminary while complete sparse-recovery validation is being audited.

## Repository Structure

- `cpp/include/` - C++ headers for stream updates, readers, trackers, samplers,
  and sketches.
- `cpp/src/` - C++ implementations and the command-line experiment runner.
- `cpp/tests/` - executable C++ regression tests.
- `scripts/` - synthetic-data generation, validation, experiment execution,
  result analysis, and LOBSTER inspection scripts.
- `tests/` - Python `unittest` regression tests for generation and validation.
- `data/synthetic/` - small reproducible synthetic streams and expected support
  files.
- `data/raw/` - local raw datasets such as LOBSTER samples.
- `data/processed/` - local derived datasets and conversions.
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
./cpp/build/test_sampler_config.exe
```

## Run Sampling Experiments

The main executable accepts:

```text
l0_sampler <path_to_csv> [trials] [output_csv] [baseline|hash]
    [--levels N]
    [--sparsity N]
    [--rows N]
    [--buckets N]
    [--degree N]
    [--seed N]
    [--recovery greedy|fixed]
    [--fixed-level N]
```

Run the exact baseline sampler:

```bash
./cpp/build/l0_sampler.exe \
    data/synthetic/multi_support_100.csv \
    1000 \
    results/experiments/support100_baseline.csv \
    baseline \
    --seed 123
```

Run the hash-based sampler with greedy recovery:

```bash
./cpp/build/l0_sampler.exe \
    data/synthetic/multi_support_100.csv \
    1000 \
    results/experiments/support100_greedy.csv \
    hash \
    --recovery greedy \
    --seed 123
```

The optional output CSV contains one row per trial, including the sampled item, status, trial seed, sampler configuration, recovery mode, and fixed level when applicable.

Possible status values include:

- `success`
- `empty_support`
- `no_recoverable_level`
- `recovery_failure`
- `invalid_sample`

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

- The exact baseline stores the full final support and is used only as a correctness reference.
- Complete sparse-recovery validation is currently being audited to ensure that partial candidate recovery cannot be classified as full success.
- Polynomial-hash coefficients and fingerprint parameters use practical seeded pseudorandom construction; the implementation does not claim a formal reproduction of every independence assumption in the paper.
- Current experiments focus on synthetic streams with known support. Runtime, memory, parameter sensitivity, and larger-scale paper-aligned experiments remain in progress.
- LOBSTER order-book data is reserved for a later applied case study.
