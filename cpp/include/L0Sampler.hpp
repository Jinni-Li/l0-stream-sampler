#pragma once

#include "StreamUpdate.hpp"

#include <cstdint>
#include <cstddef>
#include <optional>
#include <random>
#include <unordered_map>

class L0Sampler
{
private:
    std::unordered_map<std::int64_t, std::int64_t> frequesncies_;
    std::mt19937_64 rng_;
public:
    explicit L0Sampler(std::uint64_t seed = 42);
    void update(const StreamUpdate& update);
    std::optional<std::int64_t> sample();
    std::size_t support_size() const;
};