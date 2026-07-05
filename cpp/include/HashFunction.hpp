#pragma once

#include <cstdint>

class HashFunction
{
public:
    virtual ~HashFunction() = default;

    virtual std::uint64_t operator()(std::int64_t item_id) const = 0;
};

