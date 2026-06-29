import argparse
import csv
import json
import random
from collections import defaultdict, deque
from pathlib import Path


def sample_unique_items(universe_size: int, count: int, rng: random.Random):
    """
    Sample 'count' unique item IDs from [1, universe_size].
    This works efficiently even for large ranges.
    """
    if count > universe_size:
        raise ValueError("count cannot be larger than universe_size")

    return rng.sample(range(1, universe_size + 1), count)


def split_into_bounded_chunks(total: int, max_delta: int, rng: random.Random):
    """
    Split a positive integer 'total' into random positive chunks,
    each at most max_delta.
    """
    if total < 0:
        raise ValueError("total must be non-negative")

    if max_delta <= 0:
        raise ValueError("max_delta must be positive")

    chunks = []
    remaining = total

    while remaining > 0:
        amount = rng.randint(1, min(max_delta, remaining))
        chunks.append(amount)
        remaining -= amount

    return chunks


def generate_surviving_item_sequence(
    item_id: int,
    max_positive_updates: int,
    max_delta: int,
    rng: random.Random,
):
    """
    Generate multiple updates for an item that should remain
    in the final support.

    The item receives several positive updates and then possibly
    some negative updates, but its final frequency remains non-zero.
    """
    positive_update_count = rng.randint(1, max_positive_updates)

    positive_updates = [
        rng.randint(1, max_delta)
        for _ in range(positive_update_count)
    ]

    total_positive = sum(positive_updates)

    # Choose a final frequency between 1 and total_positive.
    # This guarantees the item remains in the support.
    final_frequency = rng.randint(1, total_positive)

    total_negative = total_positive - final_frequency

    negative_updates = split_into_bounded_chunks(
        total=total_negative,
        max_delta=max_delta,
        rng=rng,
    )

    sequence = []

    for amount in positive_updates:
        sequence.append((item_id, amount))

    for amount in negative_updates:
        sequence.append((item_id, -amount))

    return sequence, final_frequency


def generate_cancelled_item_sequence(
    item_id: int,
    max_positive_updates: int,
    max_delta: int,
    rng: random.Random,
):
    """
    Generate multiple updates for an item that should disappear
    from the final support.

    The item receives several positive updates and then matching
    negative updates so that the final frequency is exactly zero.
    """
    positive_update_count = rng.randint(1, max_positive_updates)

    positive_updates = [
        rng.randint(1, max_delta)
        for _ in range(positive_update_count)
    ]

    total_positive = sum(positive_updates)

    negative_updates = split_into_bounded_chunks(
        total=total_positive,
        max_delta=max_delta,
        rng=rng,
    )

    sequence = []

    for amount in positive_updates:
        sequence.append((item_id, amount))

    for amount in negative_updates:
        sequence.append((item_id, -amount))

    final_frequency = 0

    return sequence, final_frequency


def random_interleave_sequences(sequences, rng: random.Random):
    """
    Randomly interleave item sequences while preserving the order
    of updates within each item.
    """
    queues = [deque(seq) for seq in sequences if len(seq) > 0]

    stream = []

    while queues:
        index = rng.randrange(len(queues))
        chosen_queue = queues[index]

        stream.append(chosen_queue.popleft())

        if not chosen_queue:
            queues.pop(index)

    return stream


def compute_final_frequencies(stream):
    """
    Reference implementation storing the full frequency vector.
    """
    freq = defaultdict(int)

    for item_id, delta in stream:
        freq[item_id] += delta

    return dict(freq)


def compute_final_support(freq):
    """
    Return sorted items with non-zero final frequency.
    """
    return sorted(item for item, value in freq.items() if value != 0)


def write_stream_csv(path: Path, stream):
    path.parent.mkdir(parents=True, exist_ok=True)

    with path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["item_id", "delta"])
        writer.writerows(stream)


def write_support_csv(path: Path, support):
    path.parent.mkdir(parents=True, exist_ok=True)

    with path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["item_id"])
        for item in support:
            writer.writerow([item])


def write_frequencies_csv(path: Path, frequencies):
    path.parent.mkdir(parents=True, exist_ok=True)

    with path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["item_id", "final_frequency"])
        for item_id in sorted(frequencies):
            writer.writerow([item_id, frequencies[item_id]])


def write_metadata_json(path: Path, metadata):
    path.parent.mkdir(parents=True, exist_ok=True)

    with path.open("w") as f:
        json.dump(metadata, f, indent=2)


def generate_multi_update_stream(
    universe_size: int,
    final_support_size: int,
    cancelled_items: int,
    max_positive_updates: int,
    max_delta: int,
    seed: int,
):
    """
    Generate a strict turnstile stream with multiple updates per item.

    Some items survive with non-zero final frequency.
    Other items are cancelled and have final frequency zero.
    """
    rng = random.Random(seed)

    total_unique_items = final_support_size + cancelled_items

    if total_unique_items > universe_size:
        raise ValueError(
            "final_support_size + cancelled_items cannot exceed universe_size"
        )

    all_items = sample_unique_items(
        universe_size=universe_size,
        count=total_unique_items,
        rng=rng,
    )

    surviving_items = sorted(all_items[:final_support_size])
    cancelled_item_ids = sorted(all_items[final_support_size:])

    expected_frequencies = {}
    sequences = []

    for item_id in surviving_items:
        sequence, final_frequency = generate_surviving_item_sequence(
            item_id=item_id,
            max_positive_updates=max_positive_updates,
            max_delta=max_delta,
            rng=rng,
        )

        sequences.append(sequence)
        expected_frequencies[item_id] = final_frequency

    for item_id in cancelled_item_ids:
        sequence, final_frequency = generate_cancelled_item_sequence(
            item_id=item_id,
            max_positive_updates=max_positive_updates,
            max_delta=max_delta,
            rng=rng,
        )

        sequences.append(sequence)
        expected_frequencies[item_id] = final_frequency

    stream = random_interleave_sequences(sequences, rng)

    computed_frequencies = compute_final_frequencies(stream)
    computed_support = compute_final_support(computed_frequencies)

    expected_support = sorted(surviving_items)

    if computed_support != expected_support:
        raise RuntimeError("Computed support does not match expected support")

    for item_id, expected_value in expected_frequencies.items():
        computed_value = computed_frequencies.get(item_id, 0)

        if computed_value != expected_value:
            raise RuntimeError(
                f"Frequency mismatch for item {item_id}: "
                f"expected {expected_value}, got {computed_value}"
            )

    return stream, expected_support, computed_frequencies


def main():
    parser = argparse.ArgumentParser(
        description="Generate multi-update synthetic turnstile streams for L0-sampling."
    )

    parser.add_argument("--output", required=True)
    parser.add_argument("--universe-size", type=int, required=True)
    parser.add_argument("--support-size", type=int, required=True)
    parser.add_argument("--cancelled-items", type=int, required=True)
    parser.add_argument("--max-positive-updates", type=int, default=5)
    parser.add_argument("--max-delta", type=int, default=100)
    parser.add_argument("--seed", type=int, default=42)

    args = parser.parse_args()

    output_path = Path(args.output)
    support_path = output_path.with_suffix(".support.csv")
    frequencies_path = output_path.with_suffix(".frequencies.csv")
    metadata_path = output_path.with_suffix(".metadata.json")

    stream, support, frequencies = generate_multi_update_stream(
        universe_size=args.universe_size,
        final_support_size=args.support_size,
        cancelled_items=args.cancelled_items,
        max_positive_updates=args.max_positive_updates,
        max_delta=args.max_delta,
        seed=args.seed,
    )

    write_stream_csv(output_path, stream)
    write_support_csv(support_path, support)
    write_frequencies_csv(frequencies_path, frequencies)

    metadata = {
        "stream_type": "multi_update_strict_turnstile",
        "universe_size": args.universe_size,
        "final_support_size": args.support_size,
        "cancelled_items": args.cancelled_items,
        "max_positive_updates": args.max_positive_updates,
        "max_delta": args.max_delta,
        "stream_length": len(stream),
        "seed": args.seed,
        "output": str(output_path),
        "support_file": str(support_path),
        "frequencies_file": str(frequencies_path),
    }

    write_metadata_json(metadata_path, metadata)

    print("Synthetic multi-update stream generated successfully.")
    print(f"Stream file:      {output_path}")
    print(f"Support file:     {support_path}")
    print(f"Frequencies file: {frequencies_path}")
    print(f"Metadata file:    {metadata_path}")
    print(f"Stream length:    {len(stream)}")
    print(f"Support size:     {len(support)}")
    print(f"Cancelled items:  {args.cancelled_items}")


if __name__ == "__main__":
    main()