#pragma once

#include <cstdint>
#include <stdexcept>

class PairwiseHash{
    public:
    PairwiseHash(std::uint64_t seed, std::uint64_t range);
    std::uint64_t operator()(std::int64_t item_id) const;
    std::uint64_t range() const;

    private:
    static constexpr std::int64_t PRIME = 2305843009213693951ULL; // 2^61 - 1

    std::uint64_t a_;
    std::uint64_t b_;
    std::uint64_t range_;

    static std::uint64_t splitmix64(std::uint64_t x);
    static std::uint64_t normalize_item(std::int64_t item_id);

    static std::uint64_t mod_multiply(
        std::uint64_t x,
        std::uint64_t y
    );
};