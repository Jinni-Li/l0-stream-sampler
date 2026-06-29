#pragma once

#include "StreamUpdate.hpp"
#include <cstdint>
#include <unordered_map>
#include <vector>

class ExactSupportTracker{
    public:
        void update(const StreamUpdate& update);
        bool contains(std::int64_t item_id) const;
        std::size_t support_size() const;
        std::vector<std::int64_t> final_support() const;

    private:
        std::unordered_map<std::int64_t, std::int64_t> frequencies_;
};

