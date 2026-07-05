#include "PairwiseHash.hpp"
#include "HashUtils.hpp"

PairwiseHash::PairwiseHash(std::uint64_t seed, std::uint64_t range): range_(range){
    if (range_ == 0)
    {
        throw std::invalid_argument("PairwiseHash range must be greater than zero");
    }

    a_ = hash_utils::splitmix64(seed) % PRIME;
    b_ = hash_utils::splitmix64(seed + 0x9e3779b97f4a7c15ULL) % PRIME;

    if (a_ == 0)
    {
        a_ = 1;
    }
}

std::uint64_t PairwiseHash::operator()(std::int64_t item_id) const{
    std::int64_t x = normalize_item(item_id);

    std::uint64_t ax = mod_multiply(a_,x);
    std::uint64_t value = (ax+b_) %PRIME;

    return value%range_;
}

std::uint64_t PairwiseHash::range() const{
    return range_;
}

std::uint64_t PairwiseHash::normalize_item(std::int64_t item_id){
    if (item_id >= 0)
    {
        return static_cast<std::uint64_t>(item_id) % PRIME;
    }

    std::uint64_t magnitude = static_cast<std::uint64_t>(-(item_id + 1)) + 1;

    return(PRIME - (magnitude % PRIME)) % PRIME;
}

std::uint64_t PairwiseHash::mod_multiply(
    std::uint64_t x,
    std::uint64_t y
) {
    return static_cast<std::uint64_t>(
        (static_cast<__uint128_t>(x) * static_cast<__uint128_t>(y)) % PRIME
    );
}