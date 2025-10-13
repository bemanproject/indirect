// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <beman/indirect/indirect.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>

namespace exe = beman::indirect;

TEST(IndirectTests, EnvSetup) { EXPECT_EQ(exe::dummy_function(), 2); }
