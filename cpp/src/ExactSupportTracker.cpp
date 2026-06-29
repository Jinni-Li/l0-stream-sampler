#include "ExactSupportTracker.hpp"

void ExactSupportTracker::update(const StreamUpdate& update){
    frequencies_[update.item_id] += update.delta;

    if (frequencies_[update.item_id] == 0){
        frequencies_.erase(update.item_id);
    }
    
}

bool ExactSupportTracker::contains(std::int64_t item_d) const{
    return frequencies_.find(item_d) != frequencies_.end();
}

std:: size_t ExactSupportTracker::support_size() const{
    return frequencies_.size();
}

std:: vector<std::int64_t> ExactSupportTracker::final_support() const{
    std::vector<std::int64_t> support;

    for (const auto& [item_id, frequency] : frequencies_){
        if (frequency != 0)
        {
            support.push_back(item_id);
        }
        
    }

    return support;
}