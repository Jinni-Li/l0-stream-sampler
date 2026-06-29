//include this only once
#pragma once

#include <cstdint>

struct StreamUpdate
{
    std::int64_t item_id;
    std::int64_t delta;
};
