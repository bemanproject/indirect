# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

find_package(GTest REQUIRED)

add_executable(beman.indirect.tests.indirect)
target_sources(beman.indirect.tests.indirect PRIVATE indirect.test.cpp)
target_link_libraries(
    beman.indirect.tests.indirect
    PRIVATE beman::indirect GTest::gtest_main
)

add_executable(beman.indirect.tests.polymorphic)
target_sources(beman.indirect.tests.polymorphic PRIVATE polymorphic.test.cpp)
target_link_libraries(
    beman.indirect.tests.polymorphic
    PRIVATE beman::indirect GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(beman.indirect.tests.indirect)
gtest_discover_tests(beman.indirect.tests.polymorphic)
