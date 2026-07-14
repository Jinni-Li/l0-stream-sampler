#include "SamplerConfig.hpp"

#include <iostream>
#include <stdexcept>

namespace {

    bool throws_invalid_argument(const SamplerConfig& config) {
        try {
            config.validate();
        } catch (const std::invalid_argument&) {
            return true;
        }

        return false;
    }

}
int main() {
    {
        SamplerConfig config;

        if (throws_invalid_argument(config)) {
            std::cerr
                << "Test 1 failed: default config should be valid\n";
            return 1;
        }
    }

    {
        SamplerConfig config;
        config.num_levels = 0;

        if (!throws_invalid_argument(config)) {
            std::cerr
                << "Test 2 failed: zero levels should be rejected\n";
            return 1;
        }
    }

    {
        SamplerConfig config;
        config.sparsity = 0;

        if (!throws_invalid_argument(config)) {
            std::cerr
                << "Test 3 failed: zero sparsity should be rejected\n";
            return 1;
        }
    }

    {
        SamplerConfig config;
        config.recovery_rows = 0;

        if (!throws_invalid_argument(config)) {
            std::cerr
                << "Test 4 failed: zero recovery rows should be rejected\n";
            return 1;
        }
    }

    {
        SamplerConfig config;
        config.recovery_buckets = 0;

        if (!throws_invalid_argument(config)) {
            std::cerr
                << "Test 5 failed: zero recovery buckets should be rejected\n";
            return 1;
        }
    }

    {
        SamplerConfig config;
        config.sparsity = 8;
        config.recovery_buckets = 4;

        if (!throws_invalid_argument(config)) {
            std::cerr
                << "Test 6 failed: buckets below sparsity should be rejected\n";
            return 1;
        }
    }

    {
        SamplerConfig config;
        config.polynomial_degree = 0;

        if (!throws_invalid_argument(config)) {
            std::cerr
                << "Test 7 failed: zero degree should be rejected\n";
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

        if (throws_invalid_argument(config)) {
            std::cerr
                << "Test 8 failed: custom valid config was rejected\n";
            return 1;
        }
    }

    {
        SamplerConfig config;
        config.num_levels = 8;
        config.recovery_mode = RecoveryMode::FixedLevel;
        config.fixed_level = 8;

        if (!throws_invalid_argument(config)) {
            std::cerr
                << "Test 9 failed: fixed_level equal to num_levels "
                << "should be rejected\n";
            return 1;
        }
    }

    {
        SamplerConfig config;
        config.num_levels = 8;
        config.recovery_mode = RecoveryMode::FixedLevel;
        config.fixed_level = 7;

        if (throws_invalid_argument(config)) {
            std::cerr
                << "Test 10 failed: fixed_level num_levels - 1 "
                << "should be valid\n";
            return 1;
        }
    }

    std::cout << "All SamplerConfig tests passed.\n";
    return 0;
}