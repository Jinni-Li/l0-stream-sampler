#include "L0Sampler.hpp"

#include <vector>

L0Sampler::L0Sampler(std::uint64_t seed):rng_(seed){}

void L0Sampler::update(const StreamUpdate& update){
    frequencies_[update.item_id] += update.delta;

    if (frequencies_[update.item_id] == 0)
    {
        frequencies_.erase(update.item_id);
    }
    
}


std::optional<std::int64_t> L0Sampler::sample(){
    if (frequencies_.empty()){
        return std::nullopt;
    }
    
    std::vector<std::int64_t> support;
    support.reserve(frequencies_.size());

    for (const auto& [item_id, frequency] : frequencies_)
    {
        if (frequency != 0)
        {
            support.push_back(item_id);
        }
        
    }
    
    std::uniform_int_distribution<std::size_t> distribution(0, support_size() -1);
    std::size_t random_index = distribution(rng_);

    return support[random_index];
}

std::size_t L0Sampler::support_size() const{
    return frequencies_.size();
}