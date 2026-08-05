#include "SSparseSketch.hpp"
#include "HashUtils.hpp"
#include "ItemDomain.hpp"
#include "PairwiseHash.hpp"
#include "PerfectRandomFunction.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace{
    constexpr std::uint64_t FINGERPRINT_PRIME = 4294967291ULL;
    constexpr std::uint64_t FINGERPRINT_SEED_SALT = 0xd6e8feb86659fd93ULL;

    constexpr std::uint64_t CELL_SEED_SALT = 0x8cb92baa3f3d8dd7ULL;

    constexpr std::uint64_t CELL_SEED_GAMMA = 0x9e3779b97f4a7c15ULL;

    std::uint64_t mod_from_int(std::int64_t value){
        const auto modulus = static_cast<std::int64_t>(FINGERPRINT_PRIME);

        std::int64_t result = value % modulus;

        if (result < 0){
            result += modulus;
        }

        return static_cast<std::uint64_t>(result);
    }


    std::uint64_t mod_pow(std::uint64_t base, std::uint64_t exponent){
        std::uint64_t result = 1;
        base %= FINGERPRINT_PRIME;

        while (exponent > 0)
        {
            if ((exponent & 1ULL) != 0ULL)
            {
                result = (result * base) % FINGERPRINT_PRIME;
            }
            
            base = (base * base) % FINGERPRINT_PRIME;

            exponent >>= 1U;
        }
        
        return result;
    }

    std::uint64_t derive_fingerprint_base(std::uint64_t seed){
        const std::uint64_t mixed = hash_utils::splitmix64(seed ^ FINGERPRINT_SEED_SALT);

        //Avoid base 0 and 1.
        return 2ULL + mixed % (FINGERPRINT_PRIME - 2ULL);
    }

    std::uint64_t fingerprint_term(
        std::uint64_t base,
        std::int64_t item_id,
        std::int64_t frequency
    ){
        const std::uint64_t item_power = mod_pow(base, static_cast<std::uint64_t>(item_id));

        const std::uint64_t frequency_mod = mod_from_int(frequency);

        return (frequency_mod * item_power) % FINGERPRINT_PRIME;
    }

    std::uint64_t derive_cell_seed(std::uint64_t sketch_seed, std::size_t row, 
        std::size_t bucket, std::size_t buckets) noexcept{
            const std::uint64_t cell_index = static_cast<std::uint64_t>(row) * 
            static_cast<std::uint64_t>(buckets) + 
            static_cast<std::uint64_t>(bucket);

            const std::uint64_t separated_seed = sketch_seed ^ CELL_SEED_SALT ^ 
            (CELL_SEED_GAMMA * (cell_index + 1ULL));

            return hash_utils::splitmix64(separated_seed);
        }
}

SSparseSketch::SSparseSketch(
    std::size_t sparsity,
    std::size_t rows,
    std::size_t buckets,
    std::uint64_t seed
)
    : SSparseSketch(
          sparsity,
          rows,
          buckets,
          seed,
          RecoveryRandomness::PairwiseHash,
          {}
      ) {}

SSparseSketch::SSparseSketch(
    std::size_t sparsity,
    std::size_t rows,
    std::size_t buckets,
    std::uint64_t seed,
    RecoveryRandomness recovery_randomness,
    const std::vector<std::int64_t>& item_universe
)
:sparsity_(sparsity),
rows_(rows),
buckets_(buckets),
seed_(seed),
fingerprint_base_(derive_fingerprint_base(seed)),
level_fingerprint_(0),
table_(){

    if(sparsity_ == 0){
        throw std::invalid_argument("sparsity must be greater than zero");
    }

    if(rows_ == 0){
        throw std::invalid_argument("rows must be greater than zero");
    }

    if(buckets_ == 0){
        throw std::invalid_argument("buckets must be greater than zero");
    }

    table_.reserve(rows_);

    for (std::size_t row = 0; row < rows_; ++row)
    {
        table_.emplace_back();

        std::vector<OneSparseSketch>& row_cells = table_.back();

        row_cells.reserve(buckets_);

        for (std::size_t bucket = 0; bucket < buckets_; ++bucket)
        {
            const std::uint64_t cell_seed = derive_cell_seed(seed_,row, bucket, buckets_);

            row_cells.emplace_back(cell_seed);
        }
        
    }
    

    if (
        recovery_randomness == RecoveryRandomness::PerfectRandom &&
        item_universe.empty()
    ) {
        throw std::invalid_argument(
            "Perfect recovery randomness requires a non-empty item universe."
        );
    }

    bucket_hashes_.reserve(rows_);

    for (std::size_t row = 0; row < rows_; ++row)
    {
        const std::uint64_t row_seed =
            seed_ +
            0x9e3779b97f4a7c15ULL *
                static_cast<std::uint64_t>(row + 1);

        if (recovery_randomness == RecoveryRandomness::PerfectRandom) {
            bucket_hashes_.push_back(
                std::make_unique<PerfectRandomFunction>(
                    item_universe,
                    row_seed,
                    buckets_
                )
            );
        } else {
            bucket_hashes_.push_back(
                std::make_unique<PairwiseHash>(
                    row_seed,
                    buckets_
                )
            );
        }
    }
    
}

void SSparseSketch::update(std::int64_t item_id, std::int64_t delta){

    item_domain::validate(item_id);
    
    if (delta == 0){
        return;
    }

    const std::uint64_t term = fingerprint_term(fingerprint_base_, item_id, delta);

    level_fingerprint_=(level_fingerprint_ + term) % FINGERPRINT_PRIME;

    for (std::size_t row = 0; row < rows_; ++row)
    {
        std::size_t bucket = bucket_for(row, item_id);
        table_[row][bucket].update(item_id,delta);
    }
}

SSparseRecoveryResult SSparseSketch::recover() const{
    std::unordered_map<std::int64_t, std::int64_t> unique_items;
    bool saw_non_empty_cell = false;
    bool saw_moment_overflow = false;

    for (std::size_t row = 0; row < rows_; ++row)
    {
        for (std::size_t bucket = 0; bucket < buckets_; ++bucket)
        {
            const OneSparseSketch& cell = table_[row][bucket];

            auto recovered = cell.recover();
            
            if(recovered.status != RecoveryStatus::Empty){
                saw_non_empty_cell = true;
            }

            if (recovered.status == RecoveryStatus::MomentOverflow)
            {
                saw_moment_overflow = true;
                continue;
            }

            if (recovered.status != RecoveryStatus::Success || !recovered.item.has_value())
            {
                continue;
            }

            const RecoveredItem& item = recovered.item.value();

            const auto [iterator, inserted] = unique_items.emplace(item.item_id, item.frequency);

            // same item recovered in different rows must have same final frequency

            if (!inserted && iterator -> second != item.frequency)
            {
                return SSparseRecoveryResult{
                    RecoveryStatus::RecoveryFailure, {}
                };
            }
        } 
    }

    std::vector<RecoveredItem> recovered_items;
    recovered_items.reserve(unique_items.size());

    for (const auto& [item_id, frequency] : unique_items)
    {
        recovered_items.push_back(RecoveredItem{item_id, frequency});
    }
    

    std::sort(recovered_items.begin(), recovered_items.end(),
    [](const RecoveredItem& left, const RecoveredItem right) {
        return left.item_id < right.item_id;
    });

    if(!saw_non_empty_cell){
        return SSparseRecoveryResult{
            RecoveryStatus::Empty,
            {}
        };
    }

    if (recovered_items.empty())
    {
        return SSparseRecoveryResult{
            saw_moment_overflow
                ? RecoveryStatus::MomentOverflow
                : RecoveryStatus::RecoveryFailure,
            {}
        };
    }

    std::uint64_t recovered_fingerprint = 0;

    for (const RecoveredItem& item : recovered_items)
    {
        const std::uint64_t term = fingerprint_term(fingerprint_base_,item.item_id, item.frequency);

        recovered_fingerprint = (recovered_fingerprint + term) % FINGERPRINT_PRIME;
    }

    if (recovered_fingerprint != level_fingerprint_)
    {
        return SSparseRecoveryResult{
            saw_moment_overflow
                ? RecoveryStatus::MomentOverflow
                : RecoveryStatus::IncompleteRecovery,
            recovered_items
        };
    }
    
    if (recovered_items.size() > sparsity_)
    {
        return SSparseRecoveryResult{
            RecoveryStatus::TooDense,
            recovered_items
        };
    }
    
    return SSparseRecoveryResult{
        RecoveryStatus::Success,
        recovered_items 
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
    return static_cast<std::size_t>((*bucket_hashes_[row])(item_id));
}

std::uint64_t SSparseSketch::cell_fingerprint_base(std::size_t row, std::size_t bucket) const{
    return table_.at(row).at(bucket).fingerprint_base();
}



