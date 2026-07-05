#include "HashFunction.hpp"
#include "SplitMix64Hash.hpp"

#include <cstdint>
#include <iostream>
#include <memory>

int main() {
    {
        SplitMix64Hash hash_a(42);
        SplitMix64Hash hash_b(42);

        for (std::int64_t item = 0; item < 1000; ++item) {
            if (hash_a(item) != hash_b(item)) {
                std::cerr << "Test 1 failed: same seed should be deterministic\n";
                return 1;
            }
        }
    }

    {
        SplitMix64Hash hash_a(42);
        SplitMix64Hash hash_b(123);

        bool found_difference = false;

        for (std::int64_t item = 0; item < 1000; ++item) {
            if (hash_a(item) != hash_b(item)) {
                found_difference = true;
                break;
            }
        }

        if (!found_difference) {
            std::cerr << "Test 2 failed: different seeds should differ on some item\n";
            return 1;
        }
    }

    {
        std::unique_ptr<HashFunction> hash =
            std::make_unique<SplitMix64Hash>(42);

        std::uint64_t value = (*hash)(17);

        if (value != SplitMix64Hash(42)(17)) {
            std::cerr << "Test 3 failed: polymorphic call differs from direct call\n";
            return 1;
        }
    }

    std::cout << "All SplitMix64Hash tests passed.\n";
    return 0;
}