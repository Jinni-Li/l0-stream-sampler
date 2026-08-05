#include "PerfectRandomFunction.hpp"
#include "ItemDomain.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

PerfectRandomFunction::PerfectRandomFunction(
    const std::vector<std::int64_t>& item_ids,
    std::uint64_t seed,
    std::uint64_t range
)
    : range_(range) {
    if (range_ == 0) {
        throw std::invalid_argument(
            "PerfectRandomFunction: range must be greater than zero."
        );
    }

    std::vector<std::int64_t> unique_ids = item_ids;

    for (const std::int64_t item_id : unique_ids) {
        item_domain::validate(item_id);
    }

    std::sort(unique_ids.begin(), unique_ids.end());
    unique_ids.erase(
        std::unique(unique_ids.begin(), unique_ids.end()),
        unique_ids.end()
    );

    values_.reserve(unique_ids.size());

    std::mt19937_64 generator(seed);

    for (const std::int64_t item_id : unique_ids) {
        values_.emplace(item_id, draw_uniform(generator, range_));
    }
}

std::uint64_t PerfectRandomFunction::operator()(
    std::int64_t item_id
) const {
    item_domain::validate(item_id);

    const auto iterator = values_.find(item_id);

    if (iterator == values_.end()) {
        throw std::out_of_range(
            "PerfectRandomFunction: item ID is not in the registered universe."
        );
    }

    return iterator->second;
}

std::uint64_t PerfectRandomFunction::range() const noexcept {
    return range_;
}

std::size_t PerfectRandomFunction::size() const noexcept {
    return values_.size();
}

std::uint64_t PerfectRandomFunction::draw_uniform(
    std::mt19937_64& generator,
    std::uint64_t range
) {
    // Rejection sampling avoids modulo bias while remaining deterministic.
    const std::uint64_t rejection_threshold =
        static_cast<std::uint64_t>(-range) % range;

    while (true) {
        const std::uint64_t value = generator();

        if (value >= rejection_threshold) {
            return value % range;
        }
    }
}
