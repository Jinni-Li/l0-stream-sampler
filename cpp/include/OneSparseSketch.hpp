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

    static constexpr std::uint64_t PRIME = 4294967291ULL;
    static constexpr std::uint64_t Z = 1000003ULL;

    static std::uint64_t mod_pow(std::uint64_t base, std:: uint64_t exponent);
    static std::uint64_t mod_from_int(std::int64_t value);

    static bool fits_in_int64(__int128_t value) noexcept;

public:
    OneSparseSketch();
    
    void update(std::int64_t item_id, std::int64_t delta);

    OneSparseRecoveryResult recover() const;

    bool empty() const;
};

