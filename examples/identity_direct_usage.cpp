// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <beman/indirect/identity.hpp>

#include <iostream>

namespace exe = beman::indirect;

int main() {
    std::cout << exe::identity()(2024) << '\n';
    return 0;
}
