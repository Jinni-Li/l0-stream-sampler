#pragma once

#include "HashFunction.hpp"

#include <cstdint>

class SplitMix64Hash : public HashFunction
{
private:
    std::uint64_t seed_;
public:
    explicit SplitMix64Hash(std::uint64_t seed);
    std::uint64_t operator()(std::int64_t item_id) const override;
};

