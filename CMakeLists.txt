# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.22.4 FATAL_ERROR)
cmake_policy(SET CMP0127 NEW)
cmake_policy(SET CMP0135 NEW)

project(
    beman.indirect # CMake Project Name, which is also the name of the top-level
    DESCRIPTION "Beman implementation of indirect and polymorphic (P3019)"
    LANGUAGES CXX
    # VERSION ${XYZ_VALUE_TYPES_VERSION} # TODO(lboehm): version??
)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# [CMAKE.SKIP_TESTS]
option(
    BEMAN_INDIRECT_BUILD_TESTS
    "Enable building tests and test infrastructure. Default: ON. Values: { ON, OFF }."
    ${PROJECT_IS_TOP_LEVEL}
)

# [CMAKE.SKIP_EXAMPLES]
option(
    BEMAN_INDIRECT_BUILD_EXAMPLES
    "Enable building examples. Default: ON. Values: { ON, OFF }."
    ${PROJECT_IS_TOP_LEVEL}
)

include(CTest) # TODO(lboehm): check why I needed to do this for it to find the tests?
include(FetchContent)
include(GNUInstallDirs)

if(BEMAN_INDIRECT_BUILD_TESTS)
    # Fetch GoogleTest
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG
            f8d7d77c06936315286eb55f8de22cd23c188571 # release-1.14.0
        EXCLUDE_FROM_ALL
    )
    set(INSTALL_GTEST OFF) # Disable GoogleTest installation
    FetchContent_MakeAvailable(googletest)
endif()
#
add_subdirectory(src/beman/indirect)
#
if(BEMAN_INDIRECT_BUILD_TESTS)
    add_subdirectory(tests/beman/indirect)
endif()
