#include "HashBasedL0Sampler.hpp"
#include "SamplerConfig.hpp"

#include <cstdint>
#include <iostream>
#include <unordered_set>
#include <stdexcept>
#include <utility>
#include <vector>

int main() {
    {
        SamplerConfig config;
        config.num_levels = 16;
        config.seed = 42;

        HashBasedL0Sampler sampler(config);

        sampler.update(17, 3);

        SampleResult sample = sampler.sample();

        if (sample.status != SampleStatus::Success || !sample.item.has_value() || sample.item.value() != 17) {
            std::cerr << "Test 1 failed: expected to sample 17\n";
            return 1;
        }
    }

    {
        SamplerConfig config;
        config.num_levels = 16;
        config.seed = 42;

        HashBasedL0Sampler sampler(config);

        sampler.update(17, 3);
        sampler.update(17, -3);

        SampleResult sample = sampler.sample();

        if (sample.status != SampleStatus::EmptySupport || sample.item.has_value()) {
            std::cerr << "Test 2 failed: expected failure on empty support\n";
            return 1;
        }

        if (sample.selected_level.has_value()) {
            std::cerr
                << "Test 2 failed: empty support selected a level\n";
            return 1;
        }

        if (
            sample.recovery_diagnostics.size() !=
            config.num_levels
        ) {
            std::cerr
                << "Test 2 failed: expected diagnostics "
                << "for every level\n";
            return 1;
        }

        for (
            const LevelRecoveryDiagnostic& diagnostic :
            sample.recovery_diagnostics
        ) {
            if (
                diagnostic.status != RecoveryStatus::Empty ||
                diagnostic.recovered_item_count != 0
            ) {
                std::cerr
                    << "Test 2 failed: expected every level "
                    << "to be empty\n";
                return 1;
            }
        }
    }

    {
        std::unordered_set<std::int64_t> support = {5, 17, 44};

        int valid = 0;
        int invalid = 0;
        int failures = 0;

        for (std::uint64_t seed = 1; seed <= 100; ++seed) {
            SamplerConfig config;
            config.num_levels = 16;
            config.seed = seed;

            HashBasedL0Sampler sampler(config);

            sampler.update(5, 1);
            sampler.update(10, 1);
            sampler.update(17, 1);
            sampler.update(30, 1);
            sampler.update(44, 1);
            sampler.update(10, -1);
            sampler.update(30, -1);

            auto sample = sampler.sample();

            if (sample.status != SampleStatus::Success || !sample.item.has_value()) {
                ++failures;
            } else if (support.count(sample.item.value()) > 0) {
                ++valid;
            } else {
                ++invalid;
            }
        }

        if (invalid > 0) {
            std::cerr << "Test 3 failed: got invalid samples\n";
            return 1;
        }

        if (valid < 80) {
            std::cerr
                << "Test 3 failed: unexpectedly low recovery rate; "
                << "valid=" << valid
                << ", failures=" << failures
                << "\n";
            return 1;
        }

        std::cout << "Tiny stream hash-based results: "
                  << "valid=" << valid
                  << ", failures=" << failures
                  << ", invalid=" << invalid
                  << "\n";
    }

    {
        bool threw = false;

        try{
            SamplerConfig config;
            config.num_levels = 0;
            config.seed = 42;

            HashBasedL0Sampler sampler(config);
        }catch (const std::invalid_argument&){
            threw = true;
        }

        if(!threw){
            std::cerr << "Test 4 failed: expected invalid_argument for zero levels\n";
            return 1;
        }
    }

    {
        SamplerConfig config;
        config.num_levels = 16;
        config.sparsity = 8;
        config.recovery_rows = 5;
        config.recovery_buckets = 16;
        config.polynomial_degree = 4;
        config.seed = 500;

        HashBasedL0Sampler sampler(config);

        if (sampler.config().num_levels != 16 ||
            sampler.config().sparsity != 8 ||
            sampler.config().recovery_rows != 5 ||
            sampler.config().recovery_buckets != 16 ||
            sampler.config().polynomial_degree != 4 ||
            sampler.config().seed != 500) {

            std::cerr
                << "Test 5 failed: sampler did not retain config\n";
            return 1;
        }
    }

    {
        SamplerConfig config;
        config.seed = 999;

        HashBasedL0Sampler sampler_a(config);
        HashBasedL0Sampler sampler_b(config);

        const std::vector<std::pair<std::int64_t, std::int64_t>>
            updates = {
                {5, 1},
                {17, 2},
                {44, 3},
                {17, -1}
            };

        for (const auto& [item_id, delta] : updates) {
            sampler_a.update(item_id, delta);
            sampler_b.update(item_id, delta);
        }

        const SampleResult result_a = sampler_a.sample();
        const SampleResult result_b = sampler_b.sample();

        if (result_a.status != result_b.status ||
            result_a.item != result_b.item ||
            result_a.candidates != result_b.candidates) {

            std::cerr
                << "Test 6 failed: same config produced "
                << "different results\n";
            return 1;
        }
    }

    {
        SamplerConfig config;
        config.num_levels = 16;
        config.recovery_mode = RecoveryMode::FixedLevel;
        config.fixed_level = 0;
        config.seed = 42;

        HashBasedL0Sampler sampler(config);

        sampler.update(17, 3);

        const SampleResult result = sampler.sample();

        if (
            result.status != SampleStatus::Success ||
            !result.item.has_value() ||
            result.item.value() != 17
        ) {
            std::cerr
                << "Test 7 failed expected item 17 at level 0\n";
            return 1;
        }

        if (
            !result.selected_level.has_value() ||
            result.selected_level.value() != 0
        ) {
            std::cerr
                << "Test 7 failed: expected selected level 0\n";
            return 1;
        }

        if (result.recovery_diagnostics.size() != 1) {
            std::cerr
                << "Test 7 failed: expected one recovery diagnostic\n";
            return 1;
        }

        const LevelRecoveryDiagnostic& diagnostic =
            result.recovery_diagnostics.front();

        if (
            diagnostic.level != 0 ||
            diagnostic.status != RecoveryStatus::Success ||
            diagnostic.recovered_item_count != 1
        ) {
            std::cerr
                << "Test 7 failed: incorrect fixed-level diagnostic\n";
            return 1;
        }
    }

    {
        SamplerConfig config;
        config.num_levels = 16;
        config.recovery_mode = RecoveryMode::Greedy;
        config.seed = 42;

        HashBasedL0Sampler sampler(config);

        sampler.update(5, 1);
        sampler.update(17, 1);
        sampler.update(44, 1);

        const SampleResult result = sampler.sample();

        if (
            result.status != SampleStatus::Success ||
            !result.item.has_value()
        ) {
            std::cerr
                << "Test 8 failed: expected a recovered item\n";
            return 1;
        }

        const std::unordered_set<std::int64_t> support = {
            5, 17, 44
        };

        if (support.count(result.item.value()) == 0) {
            std::cerr
                << "Test 8 failed: recovered item outside support\n";
            return 1;
        }

        if (!result.selected_level.has_value()) {
            std::cerr
                << "Test 8 failed: expected a selected level\n";
            return 1;
        }

        if (result.recovery_diagnostics.empty()) {
            std::cerr
                << "Test 8 failed: expected recovery diagnostics\n";
            return 1;
        }

        const LevelRecoveryDiagnostic& selected_diagnostic =
            result.recovery_diagnostics.back();

        if (
            selected_diagnostic.level !=
                result.selected_level.value() ||
            selected_diagnostic.status !=
                RecoveryStatus::Success ||
            selected_diagnostic.recovered_item_count !=
                result.candidates.size()
        ) {
            std::cerr
                << "Test 8 failed: incorrect selected-level diagnostic\n";
            return 1;
        }

        for (
            std::size_t i = 1;
            i < result.recovery_diagnostics.size();
            ++i
        ) {
            const std::size_t previous_level =
                result.recovery_diagnostics[i - 1].level;

            const std::size_t current_level =
                result.recovery_diagnostics[i].level;

            if (previous_level != current_level + 1) {
                std::cerr
                    << "Test 8 failed: greedy diagnostics "
                    << "are not ordered from high to low levels\n";
                return 1;
            }
        }
    }

    {
        SamplerConfig config;
        config.num_levels = 16;
        config.recovery_mode = RecoveryMode::FixedLevel;
        config.fixed_level = 5;
        config.seed = 42;

        HashBasedL0Sampler sampler(config);

        const SampleResult result = sampler.sample();

        if (
            result.status != SampleStatus::NoRecoverableLevel ||
            result.item.has_value()
        ) {
            std::cerr
                << "Test 9 failed: expected empty fixed level\n";
            return 1;
        }

        if (result.selected_level.has_value()) {
            std::cerr
                << "Test 9 failed: empty level must not be selected\n";
            return 1;
        }

        if (result.recovery_diagnostics.size() != 1) {
            std::cerr
                << "Test 9 failed: expected one recovery diagnostic\n";
            return 1;
        }

        const LevelRecoveryDiagnostic& diagnostic =
            result.recovery_diagnostics.front();

        if (
            diagnostic.level != 5 ||
            diagnostic.status != RecoveryStatus::Empty ||
            diagnostic.recovered_item_count != 0
        ) {
            std::cerr
                << "Test 9 failed: incorrect empty-level diagnostic\n";
            return 1;
        }

    }

    // Negative item IDs must be rejected before any sampling level is modified.
    {
        SamplerConfig config;
        config.num_levels = 16;
        config.seed = 42;

        HashBasedL0Sampler sampler(config);

        bool threw = false;

        try
        {
            sampler.update(-1, 1);
        }
        catch (const std::invalid_argument&)
        {
            threw = true;
        }

        if (!threw)
        {
            std::cerr
                << "Test 10 failed: expected invalid_argument "
                << "for a negative item ID\n";
            return 1;
        }

        const SampleResult result = sampler.sample();

        if (
            result.status != SampleStatus::EmptySupport ||
            result.item.has_value()
        )
        {
            std::cerr
                << "Test 10 failed: invalid update modified "
                << "the sampler state\n";
            return 1;
        }
    }

    std::cout << "All HashBasedL0Sampler tests passed.\n";
    return 0;
}