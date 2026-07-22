#include "HashBasedL0Sampler.hpp"
#include "KWisePolynomialHash.hpp"
#include "HashUtils.hpp"

#include <algorithm>
#include <vector>
#include <stdexcept>
#include <cstddef>
#include <memory>

namespace {

SamplerConfig validated_config(SamplerConfig config) {
    config.validate();
    return config;
}

std::vector<std::int64_t> extract_candidate_ids(const std::vector<RecoveredItem>& items){
    std::vector<std::int64_t> candidates;
    candidates.reserve(items.size());

    for (const RecoveredItem& item : items)
    {
        candidates.push_back(item.item_id);
    }

    return candidates;
}

}

HashBasedL0Sampler::HashBasedL0Sampler(
    const SamplerConfig& config
)
    :config_(validated_config(config)),
    paper_hash_(std::make_unique<KWisePolynomialHash>(config_.seed, config_.polynomial_degree)) { 

        levels_.reserve(config_.num_levels);

        for (std::size_t level = 0; level < config_.num_levels; ++level)
        {
            const std::uint64_t levels_seed = hash_utils::splitmix64(config_.seed + 
                0x9e3779b97f4a7c15ULL * static_cast<std::uint64_t>(level + 1));
            
            levels_.emplace_back(config_.sparsity, config_.recovery_rows, 
                config_.recovery_buckets,levels_seed);
        }
        
    }

void HashBasedL0Sampler::update(std::int64_t item_id, std::int64_t delta) {
    if (levels_.empty() || delta == 0) {
        return;
    }

    const std::uint64_t hash_value = hash_item(item_id);

    for (std::size_t level = 0; level < levels_.size(); ++level) {
        if (!included_in_level(hash_value,level))
        {
            break;
        }

        levels_[level].update(item_id, delta);
        
    }
}

bool HashBasedL0Sampler::included_in_level(std::uint64_t hash_value, std::size_t level){
    if (level >= 61)
    {
        return hash_value == 0;
    }

    std::uint64_t threshold = KWisePolynomialHash::prime() >> level;

    return hash_value < threshold;
}

SampleResult HashBasedL0Sampler::sample() const {
    if (config_.recovery_mode == RecoveryMode::FixedLevel) {
        return sample_fixed_level();
    }

    return sample_greedy();
}

std::size_t HashBasedL0Sampler::num_levels() const noexcept {
    return levels_.size();
}

const SamplerConfig& HashBasedL0Sampler::config() const noexcept {
    return config_;
}

std::uint64_t HashBasedL0Sampler::hash_item(std::int64_t item_id) const {
    return (*paper_hash_)(item_id);
}

std::uint64_t HashBasedL0Sampler::selection_hash(std::int64_t item_id) const{
    return (*paper_hash_)(item_id);
}

SampleResult HashBasedL0Sampler::select_candidate(
    const std::vector<std::int64_t>& candidates
) const {
    if (candidates.empty()) {
        return SampleResult{
            SampleStatus::NoRecoverableLevel,
            std::nullopt,
            candidates
        };
    }

    std::int64_t best_candidate = candidates.front();
    std::uint64_t best_hash =
        selection_hash(best_candidate);

    for (const std::int64_t candidate : candidates) {
        const std::uint64_t candidate_hash =
            selection_hash(candidate);

        if (candidate_hash < best_hash) {
            best_candidate = candidate;
            best_hash = candidate_hash;
        }
    }

    return SampleResult{
        SampleStatus::Success,
        best_candidate,
        candidates
    };
}

SampleResult HashBasedL0Sampler::sample_fixed_level() const {
    const auto recovered =
        levels_[config_.fixed_level].recover();

    const auto candidates = extract_candidate_ids(recovered.items);

    if (recovered.status == RecoveryStatus::Empty) {
        return SampleResult{
            SampleStatus::NoRecoverableLevel,
            std::nullopt,
            candidates
        };
    }

    if (recovered.status != RecoveryStatus::Success) {
        return SampleResult{
            SampleStatus::RecoveryFailure,
            std::nullopt,
            candidates
        };
    }

    return select_candidate(candidates);
}

SampleResult HashBasedL0Sampler::sample_greedy() const {
    bool saw_non_empty_level = false;
    bool saw_recovery_problem = false;

    for (std::size_t i = levels_.size(); i > 0; --i) {
        const std::size_t level = i - 1;

        const auto recovered = levels_[level].recover();

        if (recovered.status == RecoveryStatus::Empty) {
            continue;
        }

        saw_non_empty_level = true;

        if (recovered.status != RecoveryStatus::Success) {
            saw_recovery_problem = true;
            continue;
        }

        const auto candidates = extract_candidate_ids(recovered.items);

        return select_candidate(candidates);
    }

    if (!saw_non_empty_level) {
        return SampleResult{
            SampleStatus::EmptySupport,
            std::nullopt,
            {}
        };
    }

    if (saw_recovery_problem) {
        return SampleResult{
            SampleStatus::RecoveryFailure,
            std::nullopt,
            {}
        };
    }

    return SampleResult{
        SampleStatus::NoRecoverableLevel,
        std::nullopt,
        {}
    };
}