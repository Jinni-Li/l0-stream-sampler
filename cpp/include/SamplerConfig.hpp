#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>

enum class RecoveryMode {
    Greedy,
    FixedLevel
};

struct SamplerConfig
{
    // Number of geometric sampling levels
    std::size_t num_levels = 32;

    // Maximum target sparsity recoverable at each level
    std::size_t sparsity = 4;

    // Number of independent rows in each SSparseSketch
    std::size_t recovery_rows = 4;

    //Number of buckets per recovery row
    std::size_t recovery_buckets = 8;

    // Degree of the polynomial hash used for sampling and selection
    std::size_t polynomial_degree = 3;

    // Base seed from which the sampler derives its internal randomness.
    std::uint64_t seed = 123;

    RecoveryMode recovery_mode = RecoveryMode::Greedy;

    std::size_t fixed_level = 0;

    void validate() const{
        if (num_levels == 0) {
            throw std::invalid_argument(
                "SamplerConfig: num_levels must be greater than 0."
            );
        }

        if (sparsity == 0) {
            throw std::invalid_argument(
                "SamplerConfig: sparsity must be greater than 0."
            );
        }

        if (recovery_rows == 0) {
            throw std::invalid_argument(
                "SamplerConfig: recovery_rows must be greater than 0."
            );
        }

        if (recovery_buckets == 0) {
            throw std::invalid_argument(
                "SamplerConfig: recovery_buckets must be greater than 0."
            );
        }

        if (recovery_buckets < sparsity) {
            throw std::invalid_argument(
                "SamplerConfig: recovery_buckets must be at least sparsity."
            );
        }

        if (polynomial_degree == 0) {
            throw std::invalid_argument(
                "SamplerConfig: polynomial_degree must be greater than 0."
            );
        }

        if (
            recovery_mode == RecoveryMode::FixedLevel &&
            fixed_level >= num_levels
        ) {
            throw std::invalid_argument(
                "SamplerConfig: fixed_level must be smaller than num_levels."
            );
        }
        
    }
};
