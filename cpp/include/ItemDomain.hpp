#pragma once

#include <cstdint>
#include <stdexcept>

namespace item_domain
{

inline void validate(std::int64_t item_id)
{
    if (item_id < 0)
    {
        throw std::invalid_argument(
            "item_id must be non-negative"
        );
    }
}

} 