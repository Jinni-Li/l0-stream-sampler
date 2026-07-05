#pragma once
#include "OneSparseSketch.hpp"
#include "SamplerStatus.hpp"
#include "PairwiseHash.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

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
        std::vector<PairwiseHash>bucket_hashes_;
        
        std::size_t bucket_for(std::size_t row, std::int64_t item_id) const;
};