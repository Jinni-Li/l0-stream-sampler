#include "HashBasedL0Sampler.hpp"

#include <algorithm>

HashBasedL0Sampler::HashBasedL0Sampler(
    std::size_t num_levels,
    std::uint64_t seed
)
    :seed_(seed) {
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

std::optional<std::int64_t> HashBasedL0Sampler::sample() const {
    for (std::size_t i = levels_.size(); i > 0; --i) {
        std::size_t level = i - 1;

        auto recovered = levels_[level].recover();

        if (recovered.success && !recovered.candidates.empty()) {
            return recovered.candidates.front();
        }
    }

    return std::nullopt;
}

std::size_t HashBasedL0Sampler::num_levels() const {
    return levels_.size();
}

std::uint64_t HashBasedL0Sampler::hash_item(std::int64_t item_id) const {
    std::uint64_t x = static_cast<std::uint64_t>(item_id);
    return splitmix64(x ^ seed_);
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