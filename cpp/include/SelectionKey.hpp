#pragma once

#include <cstdint>

struct SelectionKey {
    std::uint64_t hash;
    std::int64_t item_id;
};

constexpr bool operator<(const SelectionKey& left,const SelectionKey& right) noexcept {
    return left.hash < right.hash ||
        (left.hash == right.hash &&left.item_id < right.item_id);
}