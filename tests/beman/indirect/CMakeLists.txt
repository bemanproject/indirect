# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

include(GoogleTest)

add_executable(beman.indirect.tests.indirect)
target_sources(beman.indirect.tests.indirect PRIVATE indirect_test.cpp)

target_link_libraries(
    beman.indirect.tests.indirect
    PRIVATE beman::indirect GTest::gtest GTest::gtest_main
)

gtest_add_tests(beman.indirect.tests.indirect "" AUTO)

add_executable(beman.indirect.tests.polymorphic)
target_sources(beman.indirect.tests.polymorphic PRIVATE polymorphic_test.cpp)

target_link_libraries(
    beman.indirect.tests.polymorphic
    PRIVATE beman::indirect GTest::gtest GTest::gtest_main
)

gtest_add_tests(beman.indirect.tests.polymorphic "" AUTO)
