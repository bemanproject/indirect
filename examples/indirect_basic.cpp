// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <beman/indirect/indirect.hpp>

#include <iostream>
#include <string>

int main() {
    // indirect owns a heap-allocated value with deep-copy semantics.
    beman::indirect::indirect<std::string> greeting("Hello, world!");
    std::cout << *greeting << "\n";

    // Copy creates an independent deep copy.
    auto copy = greeting;
    *copy = "Hello, copy!";
    std::cout << "Original: " << *greeting << "\n";
    std::cout << "Copy: " << *copy << "\n";

    // Move leaves the source valueless.
    auto moved = std::move(greeting);
    std::cout << "Moved: " << *moved << "\n";
    std::cout << "Source valueless: " << greeting.valueless_after_move() << "\n";

    return 0;
}
