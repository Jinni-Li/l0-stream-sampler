#pragma once

#include "SSparseSketch.hpp"
#include "SamplerStatus.hpp"
#include "HashFunction.hpp"
#include "SamplerConfig.hpp"

#include <memory>
#include <cstdint>
#include <cstddef>
#include <vector>

class HashBasedL0Sampler {
public:
    explicit HashBasedL0Sampler(const SamplerConfig& config);

    void update(std::int64_t item_id, std::int64_t delta);

    SampleResult sample() const;

    std::size_t num_levels() const noexcept;
    const SamplerConfig& config() const noexcept;

    

private:
    SamplerConfig config_;
    std::vector<SSparseSketch> levels_;
    std::unique_ptr<HashFunction> paper_hash_;

    SampleResult sample_greedy() const;
    SampleResult sample_fixed_level() const;
    SampleResult select_candidate( const std::vector<std::int64_t>& candidates) const;

    std::uint64_t hash_item(std::int64_t item_id) const;
    std::uint64_t selection_hash(std::int64_t item_id) const;

    static bool included_in_level(std::uint64_t hash_value, std::size_t level);
};