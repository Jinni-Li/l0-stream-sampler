#include "OneSparseSketch.hpp"

#include <iostream>

int main() {
    {
        OneSparseSketch sketch;
        sketch.update(17, 3);

        auto recovered = sketch.recover();

        if (!recovered.has_value() || recovered.value() != 17) {
            std::cerr << "Test 1 failed: expected to recover 17\n";
            return 1;
        }
    }

    {
        OneSparseSketch sketch;
        sketch.update(17, 3);
        sketch.update(17, -3);

        auto recovered = sketch.recover();

        if (recovered.has_value()) {
            std::cerr << "Test 2 failed: expected empty/failure\n";
            return 1;
        }
    }

    {
        OneSparseSketch sketch;
        sketch.update(17, 1);
        sketch.update(44, 1);

        auto recovered = sketch.recover();

        if (recovered.has_value()) {
            std::cerr << "Test 3 failed: expected failure for two active items\n";
            return 1;
        }
    }

    {
        OneSparseSketch sketch;
        sketch.update(1001, 50);
        sketch.update(1001, -20);

        auto recovered = sketch.recover();

        if (!recovered.has_value() || recovered.value() != 1001) {
            std::cerr << "Test 4 failed: expected to recover 1001\n";
            return 1;
        }
    }

    std::cout << "All OneSparseSketch tests passed.\n";
    return 0;
}