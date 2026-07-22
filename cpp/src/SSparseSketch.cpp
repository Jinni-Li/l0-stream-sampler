#include "SSparseSketch.hpp"
#include "HashUtils.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace{
    constexpr std::uint64_t FINGERPRINT_PRIME = 4294967291ULL;
    constexpr std::uint64_t FINGERPRINT_SEED_SALT = 0xd6e8feb86659fd93ULL;

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
}

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
fingerprint_base_(derive_fingerprint_base(seed)),
level_fingerprint_(0),
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

    bucket_hashes_.reserve(rows_);

    for (std::size_t row = 0; row < rows_; ++row)
    {
        bucket_hashes_.emplace_back(
            seed_ + 0x9e3779b97f4a7c15ULL * static_cast<std::int64_t>(row+1),
            buckets_
        );
    }
    
}

void SSparseSketch::update(std::int64_t item_id, std::int64_t delta){
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
            RecoveryStatus::RecoveryFailure,
            {}
        };
    }

    if (recovered_items.size() > sparsity_)
    {
        return SSparseRecoveryResult{
            RecoveryStatus::TooDense,
            recovered_items
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
            RecoveryStatus::IncompleteRecovery,
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
    return static_cast<std::size_t>(bucket_hashes_[row](item_id));
}




