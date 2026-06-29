#pragma once
#include <cstdint>
#include<optional>

class OneSparseSketch
{
private:
    std:: int64_t phi_;
    std:: int64_t iota_;
    std:: int64_t fingerprint_;

    static constexpr std::uint64_t PRIME = 4294967291ULL;
    static constexpr std::uint64_t Z = 1000003ULL;

    static std::uint64_t mod_pow(std::uint64_t base, std:: uint64_t exponent);
    static std::uint64_t mod_from_int(std::int64_t value);

public:
    OneSparseSketch();
    
    void update(std::int64_t item_id, std::int64_t delta);

    std::optional<std::int64_t> recover() const;

    bool empty() const;
};

