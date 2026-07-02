#pragma once
#include "OneSparseSketch.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

struct SSparseRecoveryResult
{
    bool success;
    bool empty;
    bool too_dense;
    std::vector<std::int64_t> candidates;
};

class SSparseSketch
{
    public:
        SSparseSketch(
            std::size_t sparsity = 4,
            std::size_t rows = 4,
            std::size_t buckets = 8,
            std::uint64_t seed = 123
        );

        void update(std::int64_t item_id, std::int64_t delta);

        SSparseRecoveryResult recover() const;

        std::size_t sparsity() const;
        std::size_t rows() const;
        std::size_t buckets() const;

    private:
        std::size_t sparsity_;
        std::size_t rows_;
        std::size_t buckets_;
        std::uint64_t seed_;

        std::vector<std::vector<OneSparseSketch>> table_;

        std::size_t bucket_for(std::size_t row, std::int64_t item_id) const;
        static std::uint64_t splitmix64(std::uint64_t x);
};