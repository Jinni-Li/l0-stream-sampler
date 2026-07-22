#include "PairwiseHash.hpp"
#include "SSparseSketch.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

constexpr std::uint64_t ROW_SEED_GAMMA =
    0x9e3779b97f4a7c15ULL;

bool contains_item_with_frequency(
    const std::vector<RecoveredItem>& items,
    std::int64_t target,
    std::int64_t expected_frequency
) {
    return std::any_of(
        items.begin(),
        items.end(),
        [target, expected_frequency](
            const RecoveredItem& item
        ) {
            return
                item.item_id == target &&
                item.frequency == expected_frequency;
        }
    );
}

PairwiseHash make_first_row_hash(
    std::uint64_t sketch_seed,
    std::size_t buckets
) {
    const std::uint64_t row_seed =
        sketch_seed + ROW_SEED_GAMMA;

    return PairwiseHash(row_seed, buckets);
}

} // namespace

int main() {
    // Test 1: a newly created sketch must be empty.
    {
        SSparseSketch sketch(4, 4, 8, 42);

        const auto result = sketch.recover();

        if (
            result.status != RecoveryStatus::Empty ||
            !result.items.empty()
        ) {
            std::cerr
                << "Test 1 failed: expected empty sketch\n";
            return 1;
        }
    }

    // Test 2: recover one item and its final frequency.
    {
        SSparseSketch sketch(4, 4, 8, 42);

        sketch.update(17, 3);

        const auto result = sketch.recover();

        if (result.status != RecoveryStatus::Success) {
            std::cerr
                << "Test 2 failed: expected successful "
                << "recovery, got "
                << to_string(result.status)
                << '\n';
            return 1;
        }

        if (
            !contains_item_with_frequency(
                result.items,
                17,
                3
            )
        ) {
            std::cerr
                << "Test 2 failed: expected item 17 "
                << "with frequency 3\n";
            return 1;
        }
    }

    // Test 3: recover two distinct items.
    {
        SSparseSketch sketch(4, 4, 16, 123);

        sketch.update(17, 1);
        sketch.update(44, 1);

        const auto result = sketch.recover();

        if (result.status != RecoveryStatus::Success) {
            std::cerr
                << "Test 3 failed: expected successful "
                << "recovery for two items, got "
                << to_string(result.status)
                << '\n';
            return 1;
        }

        if (
            !contains_item_with_frequency(
                result.items,
                17,
                1
            ) ||
            !contains_item_with_frequency(
                result.items,
                44,
                1
            )
        ) {
            std::cerr
                << "Test 3 failed: expected items 17 "
                << "and 44 with frequency 1\n";
            return 1;
        }
    }

    // Test 4: partial and complete cancellation.
    {
        SSparseSketch sketch(4, 4, 16, 123);

        sketch.update(1001, 50);
        sketch.update(1001, -20);

        const auto result = sketch.recover();

        if (
            result.status != RecoveryStatus::Success ||
            !contains_item_with_frequency(
                result.items,
                1001,
                30
            )
        ) {
            std::cerr
                << "Test 4 failed: expected item 1001 "
                << "with final frequency 30\n";
            return 1;
        }

        sketch.update(1001, -30);

        const auto empty_result = sketch.recover();

        if (
            empty_result.status !=
            RecoveryStatus::Empty
        ) {
            std::cerr
                << "Test 4 failed: expected empty sketch "
                << "after complete cancellation, got "
                << to_string(empty_result.status)
                << '\n';
            return 1;
        }
    }

    //Test 5: recover more than s items completely.
    {
        constexpr std::uint64_t seed = 999;
        constexpr std::size_t buckets = 8;
        constexpr std::size_t required_items = 3;

        const PairwiseHash row_hash =
            make_first_row_hash(seed, buckets);

        std::vector<std::int64_t> item_for_bucket(
            buckets,
            -1
        );

        std::vector<std::int64_t> isolated_items;
        isolated_items.reserve(required_items);

        for (
            std::int64_t item = 1;
            item < 1000 &&
            isolated_items.size() < required_items;
            ++item
        ) {
            const std::size_t bucket =
                static_cast<std::size_t>(
                    row_hash(item)
                );

            if (item_for_bucket[bucket] != -1) {
                continue;
            }

            item_for_bucket[bucket] = item;
            isolated_items.push_back(item);
        }

        if (isolated_items.size() != required_items) {
            std::cerr
                << "Test 5 failed: could not find three "
                << "items in different buckets\n";
            return 1;
        }

        SSparseSketch sketch(
            2,       // sparsity
            1,       // rows
            buckets,
            seed
        );

        for (const std::int64_t item : isolated_items) {
            sketch.update(item, 1);
        }

        const auto result = sketch.recover();

        if (
            result.status !=
            RecoveryStatus::TooDense
        ) {
            std::cerr
                << "Test 5 failed: expected too_dense, got "
                << to_string(result.status)
                << '\n';
            return 1;
        }

        if (result.items.size() != required_items) {
            std::cerr
                << "Test 5 failed: expected three "
                << "recovered items, got "
                << result.items.size()
                << '\n';
            return 1;
        }
    }

    // Test 6: detect incomplete recovery.
    {
        constexpr std::uint64_t seed = 42;
        constexpr std::size_t buckets = 4;

        const PairwiseHash row_hash =
            make_first_row_hash(seed, buckets);

        std::vector<std::int64_t> first_item_in_bucket(
            buckets,
            -1
        );

        std::int64_t first = -1;
        std::int64_t second = -1;

        for (
            std::int64_t item = 1;
            item < 1000;
            ++item
        ) {
            const std::size_t bucket =
                static_cast<std::size_t>(
                    row_hash(item)
                );

            if (first_item_in_bucket[bucket] != -1) {
                first = first_item_in_bucket[bucket];
                second = item;
                break;
            }

            first_item_in_bucket[bucket] = item;
        }

        std::int64_t isolated = -1;

        if (first != -1) {
            const std::size_t collision_bucket =
                static_cast<std::size_t>(
                    row_hash(first)
                );

            for (
                std::int64_t item = 1;
                item < 1000;
                ++item
            ) {
                const std::size_t bucket =
                    static_cast<std::size_t>(
                        row_hash(item)
                    );

                if (
                    item != first &&
                    item != second &&
                    bucket != collision_bucket
                ) {
                    isolated = item;
                    break;
                }
            }
        }

        if (
            first == -1 ||
            second == -1 ||
            isolated == -1
        ) {
            std::cerr
                << "Test 6 failed: partial-recovery "
                << "setup could not be created\n";
            return 1;
        }

        SSparseSketch sketch(
            2,       // sparsity
            1,       // rows
            buckets,
            seed
        );

        sketch.update(first, 1);
        sketch.update(second, 1);
        sketch.update(isolated, 1);

        const auto result = sketch.recover();

        if (
            result.status !=
            RecoveryStatus::IncompleteRecovery
        ) {
            std::cerr
                << "Test 6 failed: expected "
                << "incomplete_recovery, got "
                << to_string(result.status)
                << '\n';

            std::cerr
                << "Colliding items: "
                << first << ", " << second
                << "; isolated item: "
                << isolated << '\n';

            return 1;
        }

        if (
            !contains_item_with_frequency(
                result.items,
                isolated,
                1
            )
        ) {
            std::cerr
                << "Test 6 failed: expected the isolated "
                << "item to appear in the partial result\n";
            return 1;
        }
    }

    std::cout
        << "All SSparseSketch tests passed.\n";

    return 0;
}