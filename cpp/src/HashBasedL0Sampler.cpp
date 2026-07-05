#include "HashBasedL0Sampler.hpp"

#include <algorithm>
#include <vector>
#include <stdexcept>
#include <cstddef>

HashBasedL0Sampler::HashBasedL0Sampler(
    std::size_t num_levels,
    std::uint64_t seed
)
    :seed_(seed) {

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
    std::size_t item_level = trailing_zeros(hash_value);

    std::size_t max_level =
        std::min(item_level, levels_.size() - 1);

    for (std::size_t level = 0; level <= max_level; ++level) {
        levels_[level].update(item_id, delta);
    }
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
    std::uint64_t x = static_cast<std::uint64_t>(item_id);
    return splitmix64(x ^ seed_);
}

std::uint64_t HashBasedL0Sampler::selection_hash(std::int64_t item_id) const{
    std::uint64_t x = static_cast<std::uint64_t>(item_id);
    constexpr std::uint64_t selection_salt = 0xd1b54a32d192ed03ULL;
    return splitmix64(x^seed_^selection_salt);
}

std::uint64_t HashBasedL0Sampler::splitmix64(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

std::size_t HashBasedL0Sampler::trailing_zeros(std::uint64_t x) {
    if (x == 0) {
        return 64;
    }

    std::size_t count = 0;

    while ((x & 1ULL) == 0) {
        ++count;
        x >>= 1;
    }

    return count;
}