#pragma once
#include <cstdint>
#include<optional>
#include "SamplerStatus.hpp"

class OneSparseSketch
{
private:
    __int128_t phi_;
    __int128_t iota_;
    std:: uint64_t fingerprint_;
    bool overflowed_;
    std::uint64_t fingerprint_base_;

    static constexpr std::uint64_t PRIME = 4294967291ULL;
    static constexpr std::uint64_t FINGERPRINT_SEED_SALT= 0xa0761d6478bd642fULL;

    static std::uint64_t derive_fingerprint_base(std::uint64_t seed) noexcept;
    static std::uint64_t mod_pow(std::uint64_t base, std:: uint64_t exponent);
    static std::uint64_t mod_from_int(std::int64_t value);

    static bool fits_in_int64(__int128_t value) noexcept;

public:
    explicit OneSparseSketch(std::uint64_t seed=123);
    
    void update(std::int64_t item_id, std::int64_t delta);

    OneSparseRecoveryResult recover() const;

    bool empty() const;

    std::uint64_t fingerprint_base() const noexcept;
};

