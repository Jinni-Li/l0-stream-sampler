#include "SSparseSketch.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>

SSparseSketch::SSparseSketch(
    std::size_t sparsity,
    std::size_t rows,
    std::size_t buckets,
    std::uint64_t seed
)
:sparsity_(sparsity),
rows_(rows),
buckets_(buckets),
seed_(seed),
table_(rows,std::vector<OneSparseSketch>(buckets)){

    if(sparsity_ == 0){
        throw std::invalid_argument("sparsity must be greater than zero");
    }

    if(rows_ == 0){
        throw std::invalid_argument("rows must be greater than zero");
    }

    if(buckets_ == 0){
        throw std::invalid_argument("buckets must be greater than zero");
    }

}

void SSparseSketch::update(std::int64_t item_id, std::int64_t delta){
    if (delta == 0){
        return;
    }

    for (std::size_t row = 0; row < rows_; ++row)
    {
        std::size_t bucket = bucket_for(row, item_id);
        table_[row][bucket].update(item_id,delta);
    }
}

SSparseRecoveryResult SSparseSketch::recover() const{
    std::unordered_set<std::int64_t> unique_candidates;
    bool saw_non_empty_cell = false;

    for (std::size_t row = 0; row < rows_; ++row)
    {
        for (std::size_t bucket = 0; bucket < buckets_; ++bucket)
        {
            const OneSparseSketch& cell = table_[row][bucket];

            if (!cell.empty())
            {
                saw_non_empty_cell = true;
            }

            auto recovered = cell.recover();
            
            if(recovered.status != RecoveryStatus::Empty){
                saw_non_empty_cell = true;
            }

            if (recovered.status == RecoveryStatus::Success && recovered.item.has_value())
            {
                unique_candidates.insert(recovered.item.value());
            }
        } 
    }

    std::vector<std::int64_t>candidates(unique_candidates.begin(), 
    unique_candidates.end());

    std::sort(candidates.begin(), candidates.end());

    if(!saw_non_empty_cell){
        return SSparseRecoveryResult{
            RecoveryStatus::Empty,
            candidates
        };
    }

    if (candidates.empty())
    {
        return SSparseRecoveryResult{
            RecoveryStatus::RecoveryFailure,
            candidates
        };
    }

    if (candidates.size() > sparsity_)
    {
        return SSparseRecoveryResult{
            RecoveryStatus::TooDense,
            candidates
        };
    }

    return SSparseRecoveryResult{
        RecoveryStatus::Success,
            candidates
    };
    
}

std::size_t SSparseSketch::sparsity() const{
    return sparsity_;
}

std::size_t SSparseSketch::rows() const{
    return rows_;
}

std::size_t SSparseSketch::buckets() const{
    return buckets_;
}

std::size_t SSparseSketch::bucket_for(std::size_t row, std::int64_t item_id) const{
    std::uint64_t x = static_cast<std::uint64_t>(item_id);

    std::uint64_t row_salt = 0x9e3779b97f4a7c15ULL * static_cast<std::uint64_t>(row + 1);

    std::uint64_t mixed = splitmix64(x^seed_^row_salt);

    return static_cast<std::size_t>(mixed % buckets_);
}

std::uint64_t SSparseSketch::splitmix64(std::uint64_t x){
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}



