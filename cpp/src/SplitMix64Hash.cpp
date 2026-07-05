#include "SplitMix64Hash.hpp"
#include "HashUtils.hpp"


SplitMix64Hash::SplitMix64Hash(std::uint64_t seed): seed_(seed){}

std::uint64_t SplitMix64Hash::operator()(std::int64_t item_id) const{
    std::uint64_t x = static_cast<std::uint64_t>(item_id);
    return hash_utils::splitmix64(x^seed_);
}