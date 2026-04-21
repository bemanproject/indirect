# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.30...4.3)

project(
    beman.indirect # CMake Project Name, which is also the name of the top-level
    # targets (e.g., library, executable, etc.).
    DESCRIPTION "Vocabulary types for composite class design"
    LANGUAGES CXX
    VERSION 0.1.0
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

# for find of beman-install-library
include(infra/cmake/beman-install-library.cmake)
include(cmake/features.cmake)

# Feature detection
beman_indirect_check_concepts(COMPILER_SUPPORTS_CONCEPTS)
beman_indirect_check_three_way_comparison(COMPILER_SUPPORTS_THREE_WAY)
beman_indirect_check_constexpr_destructor(COMPILER_SUPPORTS_CONSTEXPR_DTOR)
beman_indirect_check_constexpr_virtual(COMPILER_SUPPORTS_CONSTEXPR_VIRTUAL)
beman_indirect_check_constexpr_polymorphic_eval(COMPILER_SUPPORTS_CONSTEXPR_POLYMORPHIC_EVAL)
beman_indirect_check_no_unique_address(COMPILER_SUPPORTS_NO_UNIQUE_ADDRESS)

option(
    BEMAN_INDIRECT_USE_CONCEPTS
    "Make use of C++20 concepts. Turn this off for non-conforming compilers."
    ${COMPILER_SUPPORTS_CONCEPTS}
)
option(
    BEMAN_INDIRECT_USE_THREE_WAY_COMPARISON
    "Make use of C++20 three-way comparison (<=>). Turn this off for non-conforming compilers."
    ${COMPILER_SUPPORTS_THREE_WAY}
)
option(
    BEMAN_INDIRECT_USE_CONSTEXPR_DESTRUCTOR
    "Make use of C++20 constexpr destructors. Turn this off for non-conforming compilers."
    ${COMPILER_SUPPORTS_CONSTEXPR_DTOR}
)
option(
    BEMAN_INDIRECT_USE_CONSTEXPR_VIRTUAL
    "Make use of C++20 constexpr virtual functions. Turn this off for non-conforming compilers."
    ${COMPILER_SUPPORTS_CONSTEXPR_VIRTUAL}
)
option(
    BEMAN_INDIRECT_USE_CONSTEXPR_POLYMORPHIC_EVAL
    "Constexpr evaluation of polymorphic types works end-to-end. Turn this off for compilers with known bugs (e.g. GCC 11/12)."
    ${COMPILER_SUPPORTS_CONSTEXPR_POLYMORPHIC_EVAL}
)
option(
    BEMAN_INDIRECT_USE_NO_UNIQUE_ADDRESS
    "Make use of C++20 [[no_unique_address]]. Turn this off for non-conforming compilers."
    ${COMPILER_SUPPORTS_NO_UNIQUE_ADDRESS}
)

configure_file(
    "${PROJECT_SOURCE_DIR}/include/beman/indirect/detail/config.hpp.in"
    "${PROJECT_BINARY_DIR}/include/beman/indirect/detail/config.hpp"
    @ONLY
)

add_library(beman.indirect INTERFACE)
add_library(beman::indirect ALIAS beman.indirect)

target_sources(
    beman.indirect
    PUBLIC
        FILE_SET HEADERS
            BASE_DIRS
                "${CMAKE_CURRENT_SOURCE_DIR}/include"
                "${PROJECT_BINARY_DIR}/include"
)

set_target_properties(
    beman.indirect
    PROPERTIES VERIFY_INTERFACE_HEADER_SETS ${PROJECT_IS_TOP_LEVEL}
)

add_subdirectory(include/beman/indirect)

beman_install_library(beman.indirect TARGETS beman.indirect)

if(BEMAN_INDIRECT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/beman/indirect)
endif()

if(BEMAN_INDIRECT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
