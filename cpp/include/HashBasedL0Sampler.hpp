#pragma once

#include "SSparseSketch.hpp"
#include "SamplerStatus.hpp"
#include "HashFunction.hpp"

#include <memory>
#include <cstdint>
#include <optional>
#include <vector>

class HashBasedL0Sampler {
public:
    HashBasedL0Sampler(std::size_t num_levels = 32, std::uint64_t seed = 42);

    void update(std::int64_t item_id, std::int64_t delta);

    SampleResult sample() const;

    std::size_t num_levels() const;

private:
    std::vector<SSparseSketch> levels_;
    std::uint64_t seed_;

    std::unique_ptr<HashFunction> sampling_hash_;
    std::unique_ptr<HashFunction> selection_hash_;

    std::uint64_t hash_item(std::int64_t item_id) const;
    std::uint64_t selection_hash(std::int64_t item_id) const;

    static std::size_t trailing_zeros(std::uint64_t x);

};