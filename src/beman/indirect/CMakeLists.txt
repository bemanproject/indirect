# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_library(beman.indirect)
add_library(beman::indirect ALIAS beman.indirect)

target_sources(beman.indirect PRIVATE indirect.cpp)

target_sources(
    beman.indirect
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../../../include"
        FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../include/beman/indirect/indirect.hpp"
)

set_target_properties(beman.indirect PROPERTIES VERIFY_INTERFACE_HEADER_SETS ON)

find_package(beman-install-library REQUIRED)
beman_install_library(beman.indirect)
