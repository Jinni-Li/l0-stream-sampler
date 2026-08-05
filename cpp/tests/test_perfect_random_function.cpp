#include "PerfectRandomFunction.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void test_same_item_returns_same_value() {
    const PerfectRandomFunction random_function({2, 8, 41}, 123, 1000);

    const std::uint64_t first = random_function(8);
    const std::uint64_t second = random_function(8);

    assert(first == second);
}

void test_values_are_inside_requested_range() {
    std::vector<std::int64_t> item_ids;

    for (std::int64_t item_id = 0; item_id < 1000; ++item_id) {
        item_ids.push_back(item_id);
    }

    const PerfectRandomFunction random_function(item_ids, 123, 8);

    for (const std::int64_t item_id : item_ids) {
        assert(random_function(item_id) < 8);
    }
}

void test_assignment_does_not_depend_on_input_order() {
    const PerfectRandomFunction first({41, 2, 8, 2}, 999, 1000);
    const PerfectRandomFunction second({8, 41, 2}, 999, 1000);

    assert(first.size() == 3);
    assert(second.size() == 3);

    assert(first(2) == second(2));
    assert(first(8) == second(8));
    assert(first(41) == second(41));
}

void test_different_seeds_change_the_assignment() {
    std::vector<std::int64_t> item_ids;

    for (std::int64_t item_id = 0; item_id < 100; ++item_id) {
        item_ids.push_back(item_id);
    }

    const PerfectRandomFunction first(item_ids, 123, 1'000'000);
    const PerfectRandomFunction second(item_ids, 456, 1'000'000);

    bool found_difference = false;

    for (const std::int64_t item_id : item_ids) {
        if (first(item_id) != second(item_id)) {
            found_difference = true;
            break;
        }
    }

    assert(found_difference);
}

void test_range_one_always_returns_zero() {
    const PerfectRandomFunction random_function({2, 8, 41}, 123, 1);

    assert(random_function(2) == 0);
    assert(random_function(8) == 0);
    assert(random_function(41) == 0);
}

void test_unknown_item_is_rejected() {
    const PerfectRandomFunction random_function({2, 8, 41}, 123, 1000);

    bool threw = false;

    try {
        static_cast<void>(random_function(99));
    } catch (const std::out_of_range&) {
        threw = true;
    }

    assert(threw);
}

void test_zero_range_is_rejected() {
    bool threw = false;

    try {
        const PerfectRandomFunction random_function({2, 8, 41}, 123, 0);
        static_cast<void>(random_function);
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    assert(threw);
}

void test_negative_item_is_rejected() {
    bool threw = false;

    try {
        const PerfectRandomFunction random_function({2, -1, 41}, 123, 1000);
        static_cast<void>(random_function);
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    assert(threw);
}

} // namespace

int main() {
    test_same_item_returns_same_value();
    test_values_are_inside_requested_range();
    test_assignment_does_not_depend_on_input_order();
    test_different_seeds_change_the_assignment();
    test_range_one_always_returns_zero();
    test_unknown_item_is_rejected();
    test_zero_range_is_rejected();
    test_negative_item_is_rejected();

    std::cout << "All PerfectRandomFunction tests passed.\n";
    return 0;
}
