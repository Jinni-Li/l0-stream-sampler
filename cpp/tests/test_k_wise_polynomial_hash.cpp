#include "HashFunction.hpp"
#include "KWisePolynomialHash.hpp"

#include <cstdint>
#include <iostream>
#include <memory>

int main() {
    constexpr std::int64_t NUM_TEST_ITEMS = 1000;

    {
        KWisePolynomialHash hash_a(42, 3);
        KWisePolynomialHash hash_b(42, 3);

        for (std::int64_t item = 0; item < NUM_TEST_ITEMS; ++item) {
            if (hash_a(item) != hash_b(item)) {
                std::cerr << "Test 1 failed: same seed should be deterministic\n";
                return 1;
            }
        }
    }

    {
        KWisePolynomialHash hash_a(42, 3);
        KWisePolynomialHash hash_b(123, 3);

        bool found_difference = false;

        for (std::int64_t item = 0; item < NUM_TEST_ITEMS; ++item) {
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
        KWisePolynomialHash hash(42, 3);

        for (std::int64_t item = -100; item <= 100; ++item) {
            std::uint64_t value = hash(item);

            if (value >= KWisePolynomialHash::prime()) {
                std::cerr << "Test 3 failed: hash value out of field range\n";
                return 1;
            }
        }
    }

    {
        std::unique_ptr<HashFunction> hash =
            std::make_unique<KWisePolynomialHash>(42, 3);

        std::uint64_t value = (*hash)(17);

        if (value != KWisePolynomialHash(42, 3)(17)) {
            std::cerr << "Test 4 failed: polymorphic call differs from direct call\n";
            return 1;
        }
    }

    {
        KWisePolynomialHash hash(42, 0);

        if (hash.degree() != 0) {
            std::cerr << "Test 5 failed: expected degree 0\n";
            return 1;
        }

        std::uint64_t value_1 = hash(17);
        std::uint64_t value_2 = hash(44);

        if (value_1 != value_2) {
            std::cerr << "Test 5 failed: degree 0 hash should be constant\n";
            return 1;
        }
    }

    std::cout << "All KWisePolynomialHash tests passed.\n";
    return 0;
}