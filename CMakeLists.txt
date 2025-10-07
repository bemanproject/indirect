# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.25)

project(
    beman.indirect # CMake Project Name, which is also the name of the top-level
    # targets (e.g., library, executable, etc.).
    DESCRIPTION "Vocabulary Types for Composite Class Design"
    LANGUAGES CXX
    VERSION 2.2.1
)

# [CMAKE.SKIP_TESTS]
option(
    BEMAN_INDIRECT_BUILD_TESTS
    "Enable building tests and test infrastructure. Default: ${PROJECT_IS_TOP_LEVEL}. Values: { ON, OFF }."
    ${PROJECT_IS_TOP_LEVEL}
)

# [CMAKE.SKIP_EXAMPLES]
option(
    BEMAN_INDIRECT_BUILD_EXAMPLES
    "Enable building examples. Default: ${PROJECT_IS_TOP_LEVEL}. Values: { ON, OFF }."
    ${PROJECT_IS_TOP_LEVEL}
)

include(CTest)

add_subdirectory(src/beman/indirect)

if(BEMAN_INDIRECT_BUILD_TESTS)
    add_subdirectory(tests/beman/indirect)
endif()

if(BEMAN_INDIRECT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
