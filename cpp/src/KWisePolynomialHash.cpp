#include "KWisePolynomialHash.hpp"
#include "HashUtils.hpp"

#include <stdexcept>

KWisePolynomialHash::KWisePolynomialHash(
    std::uint64_t seed,
    std::size_t degree
): degree_(degree) {
    coefficients_.reserve(degree + 1);

    for (std::size_t i = 0; i <= degree_; ++i)
    {
        std::uint64_t mixed_seed = seed + 0x9e3779b97f4a7c15ULL * static_cast<std::uint64_t>(i+1);

        coefficients_.push_back(hash_utils::splitmix64(mixed_seed) % PRIME);
    }
    
}

std::uint64_t KWisePolynomialHash::operator()(std::int64_t item_id) const {
    std::uint64_t x = normalize_item(item_id);

    std::uint64_t result = 0;

    for (std::size_t i = coefficients_.size(); i > 0; --i) {
        result = mod_multiply(result, x);
        result = mod_add(result, coefficients_[i - 1]);
    }

    return result;
}

std::size_t KWisePolynomialHash::degree() const {
    return degree_;
}

std::uint64_t KWisePolynomialHash::prime() {
    return PRIME;
}

std::uint64_t KWisePolynomialHash::normalize_item(std::int64_t item_id) {
    if (item_id >= 0) {
        return static_cast<std::uint64_t>(item_id) % PRIME;
    }

    std::uint64_t magnitude =
        static_cast<std::uint64_t>(-(item_id + 1)) + 1;

    return (PRIME - (magnitude % PRIME)) % PRIME;
}

std::uint64_t KWisePolynomialHash::mod_add(
    std::uint64_t a,
    std::uint64_t b
) {
    std::uint64_t result = a + b;

    if (result >= PRIME || result < a) {
        result -= PRIME;
    }

    return result;
}

std::uint64_t KWisePolynomialHash::mod_multiply(
    std::uint64_t a,
    std::uint64_t b
) {
    return static_cast<std::uint64_t>(
        (static_cast<__uint128_t>(a) * static_cast<__uint128_t>(b)) % PRIME
    );
}