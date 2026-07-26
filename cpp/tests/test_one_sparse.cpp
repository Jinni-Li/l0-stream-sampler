#include "OneSparseSketch.hpp"

#include <iostream>
#include <limits>

int main() {
    {
        OneSparseSketch sketch;
        sketch.update(17, 3);

        auto recovered = sketch.recover();

        if (recovered.status != RecoveryStatus::Success || !recovered.item.has_value() || recovered.item -> item_id != 17) {
            std::cerr << "Test 1 failed: expected to recover 17\n";
            return 1;
        }

        if (recovered.item -> item_id != 17 || recovered.item->frequency != 3)
        {
            std::cerr << "Test 1 failed: expected item 17 with frequency 3\n";
            return 1;
        }
        
    }

    {
        OneSparseSketch sketch;
        sketch.update(17, 3);
        sketch.update(17, -3);

        auto recovered = sketch.recover();

        if (recovered.status != RecoveryStatus::Empty||recovered.item.has_value()) {
            std::cerr << "Test 2 failed: expected empty/failure\n";
            return 1;
        }
    }

    {
        OneSparseSketch sketch;
        sketch.update(17, 1);
        sketch.update(44, 1);

        auto recovered = sketch.recover();

        if (recovered.status == RecoveryStatus::Success||recovered.item.has_value()) {
            std::cerr << "Test 3 failed: expected failure for two active items\n";
            return 1;
        }
    }

    {
        OneSparseSketch sketch;
        sketch.update(1001, 50);
        sketch.update(1001, -20);

        auto recovered = sketch.recover();

        if (recovered.status != RecoveryStatus::Success ||!recovered.item.has_value() || recovered.item -> item_id != 1001) {
            std::cerr << "Test 4 failed: expected to recover 1001\n";
            return 1;
        }
    }

    // Large item-frequency product must be computed without overflowing int64_t.
    {
        OneSparseSketch sketch;

        const std::int64_t large_item =
            std::numeric_limits<std::int64_t>::max();

        sketch.update(large_item, 2);

        const OneSparseRecoveryResult result =
            sketch.recover();

        if (
            result.status != RecoveryStatus::Success ||
            !result.item.has_value() ||
            result.item->item_id != large_item ||
            result.item->frequency != 2
        )
        {
            std::cerr
                << "Test 5 failed: Large product recovery test failed\n";
            return 1;
        }
    }

    // A recovered frequency outside int64_t must be rejected instead of being narrowed.
    {
        OneSparseSketch sketch;

        sketch.update(
            1,
            std::numeric_limits<std::int64_t>::max()
        );

        sketch.update(1, 1);

        const OneSparseRecoveryResult result =
            sketch.recover();

        if (
            result.status !=
            RecoveryStatus::InvalidCandidate ||
            result.item.has_value()
        )
        {
            std::cerr
                << "Test 6 failed: Out-of-range frequency test failed\n";
            return 1;
        }
    }

    {
        OneSparseSketch sketch;

        const std::int64_t maximum =
            std::numeric_limits<std::int64_t>::max();

        sketch.update(maximum, maximum);
        sketch.update(maximum, maximum);
        sketch.update(maximum, maximum);

        const OneSparseRecoveryResult result =
            sketch.recover();

        if (
            result.status != RecoveryStatus::MomentOverflow ||
            result.item.has_value()
        ) {
            std::cerr
                << "Test 7 failed: 128-bit moment overflow test failed\n";
            return 1;
        }
    }
    std::cout << "All OneSparseSketch tests passed.\n";
    return 0;
}