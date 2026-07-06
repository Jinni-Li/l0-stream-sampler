#pragma once

#include "HashFunction.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

class KWisePolynomialHash : public HashFunction
{
private:
    static constexpr std::uint64_t PRIME = 2305843009213693951ULL; // 2^61 - 1

    std::size_t degree_;
    std::vector<std::uint64_t> coefficients_;

    static std::uint64_t normalize_item(std::int64_t item_id);
    static std::uint64_t mod_add(std::uint64_t a, std::uint64_t b);
    static std::uint64_t mod_multiply(std::uint64_t a, std::uint64_t b);
public:
    KWisePolynomialHash(std::uint64_t seed, std::size_t degree);

    std::uint64_t operator()(std::int64_t item_id) const override;
    std::size_t degree() const;
    static std::uint64_t prime();
};
