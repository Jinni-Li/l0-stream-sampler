#pragma once

#include "HashFunction.hpp"

#include <cstddef>
#include <cstdint>
#include <random>
#include <unordered_map>
#include <vector>

class PerfectRandomFunction final : public HashFunction {
public:
    PerfectRandomFunction(
        const std::vector<std::int64_t>& item_ids,
        std::uint64_t seed,
        std::uint64_t range
    );

    std::uint64_t operator()(std::int64_t item_id) const override;

    std::uint64_t range() const noexcept;
    std::size_t size() const noexcept;

private:
    std::uint64_t range_;
    std::unordered_map<std::int64_t, std::uint64_t> values_;

    static std::uint64_t draw_uniform(
        std::mt19937_64& generator,
        std::uint64_t range
    );
};
