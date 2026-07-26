#include "PairwiseHash.hpp"
#include "SSparseSketch.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>
#include <limits>
#include <set>

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

        if (result.status != RecoveryStatus::Empty ||!result.items.empty()) {
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

        if (!contains_item_with_frequency(result.items,17,3)) {
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

        if (!contains_item_with_frequency(result.items,17,1) ||
            !contains_item_with_frequency(result.items,44,1)) {
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

        if (result.status != RecoveryStatus::Success || 
            !contains_item_with_frequency(result.items,1001,30)) {
            std::cerr
                << "Test 4 failed: expected item 1001 "
                << "with final frequency 30\n";
            return 1;
        }

        sketch.update(1001, -30);

        const auto empty_result = sketch.recover();

        if (empty_result.status != RecoveryStatus::Empty
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

        const PairwiseHash row_hash = make_first_row_hash(seed, buckets);

        std::vector<std::int64_t> item_for_bucket(buckets, -1);

        std::vector<std::int64_t> isolated_items;
        isolated_items.reserve(required_items);

        for (std::int64_t item = 1; 
            item < 1000 && isolated_items.size() < required_items; 
            ++item) {
            const std::size_t bucket = static_cast<std::size_t>(row_hash(item));

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

        if (result.status != RecoveryStatus::TooDense) {
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

        const PairwiseHash row_hash = make_first_row_hash(seed, buckets);

        std::vector<std::int64_t> first_item_in_bucket(buckets,-1);

        std::int64_t first = -1;
        std::int64_t second = -1;

        for (std::int64_t item = 1;item < 1000;++item) {
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

            for (std::int64_t item = 1;item < 1000;++item) {
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

        if (first == -1 || second == -1 || isolated == -1
        ) {
            std::cerr
                << "Test 6 failed: partial-recovery "
                << "setup could not be created\n";
            return 1;
        }

        SSparseSketch sketch(
            2, // sparsity
            1, // rows
            buckets,
            seed
        );

        sketch.update(first, 1);
        sketch.update(second, 1);
        sketch.update(isolated, 1);

        const auto result = sketch.recover();

        if (result.status != RecoveryStatus::IncompleteRecovery
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

        if (!contains_item_with_frequency(result.items, isolated, 1)) {
            std::cerr
                << "Test 6 failed: expected the isolated "
                << "item to appear in the partial result\n";
            return 1;
        }
    }


    // Test 7: recovering more than s item s does not imply complete recovery

    {
        constexpr std::uint64_t seed = 2026;
        constexpr std::size_t buckets = 8;
        constexpr std::size_t sparsity = 2;
        constexpr std::size_t required_isolated_items = 3;

        const PairwiseHash row_hash = make_first_row_hash(seed, buckets);
        std::vector<std::int64_t> first_even(buckets, -1);

        std::vector<std:: int64_t> first_odd(buckets, -1);

        std::int64_t collision_first = -1;
        std::int64_t collision_second = -1;
        std::size_t collision_bucket = buckets;

        // Find two items with opposite parity in the same bucket.
        // Their total frequency is 2, but their summed item IDs are odd, so the cell cannot be interpreted as 1-sparse.

        for (std::int64_t item = 1;item < 10000;++item) {
            const std::size_t bucket = static_cast<std::size_t>( row_hash(item));

            if ((item % 2) == 0) {
                if (first_odd[bucket] != -1) {
                    collision_first = first_odd[bucket];
                    collision_second = item;
                    collision_bucket = bucket;
                    break;
                }

                if (first_even[bucket] == -1) {
                    first_even[bucket] = item;
                }
            } else {
                if (first_even[bucket] != -1) {
                    collision_first = first_even[bucket];
                    collision_second = item;
                    collision_bucket = bucket;
                    break;
                }

                if (first_odd[bucket] == -1) {
                    first_odd[bucket] = item;
                }
            }
        }

        std::vector<bool> used_buckets(
            buckets,
            false
        );

        if (collision_bucket < buckets) {
            used_buckets[collision_bucket] = true;
        }

        std::vector<std::int64_t> isolated_items;
        isolated_items.reserve(required_isolated_items);

        // Find three singleton items in three other buckets.
        for (std::int64_t item = 1; 
            item < 10000 && isolated_items.size() < required_isolated_items;
            ++item) {
            if (item == collision_first || item == collision_second) {
                continue;
            }

            const std::size_t bucket = static_cast<std::size_t>(row_hash(item));

            if (used_buckets[bucket]) {
                continue;
            }

            used_buckets[bucket] = true;
            isolated_items.push_back(item);
        }

        if (
            collision_first == -1 ||
            collision_second == -1 ||
            collision_bucket == buckets ||
            isolated_items.size() !=
                required_isolated_items
        ) {
            std::cerr
                << "Test 7 failed: could not construct "
                << "the incomplete dense recovery setup\n";
            return 1;
        }

        SSparseSketch sketch(sparsity,1,buckets,seed);

        sketch.update(collision_first, 1);
        sketch.update(collision_second, 1);

        for (const std::int64_t item :isolated_items) {
            sketch.update(item, 1);
        }

        const auto result = sketch.recover();

        if (result.status !=RecoveryStatus::IncompleteRecovery) {
            std::cerr
                << "Test 7 failed: expected "
                << "incomplete_recovery, got "
                << to_string(result.status)
                << '\n';
            return 1;
        }

        if (result.items.size() != required_isolated_items) {
            std::cerr
                << "Test 7 failed: expected three "
                << "recovered singleton items, got "
                << result.items.size()
                << '\n';
            return 1;
        }

        for (const std::int64_t item :isolated_items) {
            if (!contains_item_with_frequency(result.items,item,1)) {
                std::cerr
                    << "Test 7 failed: missing isolated "
                    << "item "
                    << item
                    << '\n';
                return 1;
            }
        }
    }
    

    // A cell-level moment overflow must be surfaced by the sparse-recovery result when complete recovery is no longer possible.
    {
        SSparseSketch sketch(1,1,1,123);

        const std::int64_t maximum =
            std::numeric_limits<std::int64_t>::max();

        sketch.update(maximum, maximum);
        sketch.update(maximum, maximum);
        sketch.update(maximum, maximum);

        const SSparseRecoveryResult result =
            sketch.recover();

        if (
            result.status != RecoveryStatus::MomentOverflow ||
            !result.items.empty()
        ) {
            std::cerr
                << "SSparseSketch moment overflow test failed: "
                << "expected moment_overflow, got "
                << to_string(result.status)
                << '\n';
            return 1;
        }
    }

    // The same SSparseSketch seed must reproduce the same fingerprint base for every cell.
    {
        constexpr std::size_t rows = 3;
        constexpr std::size_t buckets = 5;

        SSparseSketch first(4,rows,buckets,777);
        SSparseSketch second(4,rows,buckets,777);

        for (std::size_t row = 0; row < rows; ++row)
        {
            for (std::size_t bucket = 0;bucket < buckets;++bucket) {
                if (first.cell_fingerprint_base(row,bucket) != second.cell_fingerprint_base(row,bucket)) {
                    std::cerr
                        << "Fingerprint seed reproducibility "
                        << "test failed at row "
                        << row
                        << ", bucket "
                        << bucket
                        << '\n';

                    return 1;
                }
            }
        }
    }


    // Different cells should use different fingerprint bases within the same SSparseSketch.
    {
        constexpr std::size_t rows = 3;
        constexpr std::size_t buckets = 5;

        SSparseSketch sketch(4,rows,buckets,777);

        std::set<std::uint64_t> bases;

        for (std::size_t row = 0; row < rows; ++row)
        {
            for (std::size_t bucket = 0;bucket < buckets;++bucket) {
                const std::uint64_t base =sketch.cell_fingerprint_base(row,bucket);

                const bool inserted = bases.insert(base).second;

                if (!inserted)
                {
                    std::cerr
                        << "Independent fingerprint seed "
                        << "test failed: duplicate base at "
                        << "row "
                        << row
                        << ", bucket "
                        << bucket
                        << '\n';

                    return 1;
                }
            }
        }
    }

    // Changing the SSparseSketch seed should change the derived cell fingerprint bases.
    {
        SSparseSketch first(4, 2, 4, 777);
        SSparseSketch second(4, 2, 4, 778);

        bool found_difference = false;

        for (std::size_t row = 0; row < 2; ++row)
        {
            for (std::size_t bucket = 0; bucket < 4; ++bucket)
            {
                if (first.cell_fingerprint_base(row,bucket) !=
                    second.cell_fingerprint_base(row,bucket)) {
                    found_difference = true;
                }
            }
        }

        if (!found_difference)
        {
            std::cerr
                << "Different sketch seeds produced "
                << "identical cell fingerprint bases\n";
            return 1;
        }
    }


    std::cout
        << "All SSparseSketch tests passed.\n";

    return 0;
}