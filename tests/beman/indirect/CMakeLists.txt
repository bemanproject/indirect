# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

include(GoogleTest)

add_executable(beman.indirect.tests.identity)
target_sources(beman.indirect.tests.identity PRIVATE identity.test.cpp)
target_link_libraries(
    beman.indirect.tests.identity
    PRIVATE beman::indirect GTest::gtest GTest::gtest_main
)

gtest_add_tests(beman.indirect.tests.identity "" AUTO)
