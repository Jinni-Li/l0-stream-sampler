#pragma once
#include "OneSparseSketch.hpp"
#include "SamplerStatus.hpp"
#include "HashFunction.hpp"
#include "SamplerConfig.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
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

        SSparseSketch(
            std::size_t sparsity,
            std::size_t rows,
            std::size_t buckets,
            std::uint64_t seed,
            RecoveryRandomness recovery_randomness,
            const std::vector<std::int64_t>& item_universe
        );

        void update(std::int64_t item_id, std::int64_t delta);

        SSparseRecoveryResult recover() const;

        std::size_t sparsity() const;
        std::size_t rows() const;
        std::size_t buckets() const;

        std::uint64_t cell_fingerprint_base(std::size_t row, std::size_t bucket) const;

    private:
        std::size_t sparsity_;
        std::size_t rows_;
        std::size_t buckets_;
        std::uint64_t seed_;
        
        std::uint64_t fingerprint_base_;
        std::uint64_t level_fingerprint_;

        std::vector<std::vector<OneSparseSketch>> table_;
        std::vector<std::unique_ptr<HashFunction>> bucket_hashes_;
        
        std::size_t bucket_for(std::size_t row, std::int64_t item_id) const;
};