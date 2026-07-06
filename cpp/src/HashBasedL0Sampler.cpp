#include "HashBasedL0Sampler.hpp"
#include "SplitMix64Hash.hpp"
#include "KWisePolynomialHash.hpp"

#include <algorithm>
#include <vector>
#include <stdexcept>
#include <cstddef>

HashBasedL0Sampler::HashBasedL0Sampler(
    std::size_t num_levels,
    std::uint64_t seed
)
    :seed_(seed),
    paper_hash_(std::make_unique<KWisePolynomialHash>(seed, 3)) { // Degree 3 gives a 4-wise-style polynomial hash family for selection.

        if (num_levels == 0)
        {
            throw std::invalid_argument("num_levels must be greater than zero");
        }
        

        levels_.reserve(num_levels);
        const std::size_t sparsity = 4;
        const std::size_t rows = 4;
        const std::size_t buckets = 8;

        for (std::size_t level = 0; level < num_levels; ++level)
        {
            levels_.emplace_back(
                sparsity,rows, buckets, seed_ + static_cast<std::uint64_t>(level)
            );
        }
        
    }

void HashBasedL0Sampler::update(std::int64_t item_id, std::int64_t delta) {
    if (levels_.empty() || delta == 0) {
        return;
    }

    std::uint64_t hash_value = hash_item(item_id);

    for (std::size_t level = 0; level <= levels_.size(); ++level) {
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

    std::vector<std::int64_t> candidates;

    bool saw_non_empty_level = false;
    bool saw_recovery_problem = false;

    for (std::size_t i = levels_.size(); i > 0; --i) {
        std::size_t level = i - 1;

        auto recovered = levels_[level].recover();

        if (recovered.status == RecoveryStatus::Empty)
        {
            continue;
        }

        saw_non_empty_level = true;

        if (recovered.status != RecoveryStatus::Success)
        {
            saw_recovery_problem = true;
            continue;
        }
        
        
        for (std::int64_t candidate : recovered.candidates)
        {
            if (std::find(candidates.begin(), candidates.end(), candidate) == candidates.end())
            {
                candidates.push_back(candidate);
            }
            
        }
    }
        
    if (candidates.empty())
    {
        if (!saw_non_empty_level)
        {
            return SampleResult{
                SampleStatus::EmptySupport,
                std::nullopt,
                candidates
            };
        }

        if (saw_recovery_problem)
        {
            return SampleResult{
                SampleStatus::RecoveryFailure,
                std::nullopt,
                candidates
            };
        }

        return SampleResult{
            SampleStatus::NoRecoverableLevel,
            std::nullopt,
            candidates
        };
        
    }    

    std::int64_t best_candidate = candidates.front();
    std::uint64_t best_hash = selection_hash(best_candidate);

    for (std::int64_t candidate:candidates)
    {
        std::uint64_t candidate_hash = selection_hash(candidate);

        if (candidate_hash < best_hash)
        {
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

std::size_t HashBasedL0Sampler::num_levels() const {
    return levels_.size();
}

std::uint64_t HashBasedL0Sampler::hash_item(std::int64_t item_id) const {
    return (*paper_hash_)(item_id);
}

std::uint64_t HashBasedL0Sampler::selection_hash(std::int64_t item_id) const{
    return (*paper_hash_)(item_id);
}