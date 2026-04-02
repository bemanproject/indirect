# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

find_package(GTest REQUIRED)

add_executable(beman.indirect.tests.todo)
target_sources(beman.indirect.tests.todo PRIVATE todo.test.cpp)
target_link_libraries(
    beman.indirect.tests.todo
    PRIVATE beman::indirect GTest::gtest GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(beman.indirect.tests.todo)
