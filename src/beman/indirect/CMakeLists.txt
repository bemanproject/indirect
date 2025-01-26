# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# add_library(beman.indirect STATIC)
# add_library(beman::indirect ALIAS beman.indirect)

# xyz_add_library(
#     NAME beman.indirect
#     ALIAS beman::indirect
# )
add_library(beman.indirect STATIC)
add_library(beman::indirect ALIAS beman.indirect)

target_sources(beman.indirect PRIVATE indirect.cpp)
#         $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/beman/indirect/indirect.h>
#         $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/beman/indirect/polymorphic.h>
# )

# target_sources(beman.indirect PRIVATE identity.cpp)

target_sources(
    beman.indirect
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}/include
        FILES ${PROJECT_SOURCE_DIR}/include/beman/indirect/indirect.h
              ${PROJECT_SOURCE_DIR}/include/beman/indirect/polymorphic.h
)

set_target_properties(beman.indirect PROPERTIES VERIFY_INTERFACE_HEADER_SETS ON)

install(
    TARGETS beman.indirect
    EXPORT beman.indirect
    DESTINATION
    $<$<CONFIG:Debug>:debug/>${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION $<$<CONFIG:Debug>:debug/>${CMAKE_INSTALL_BINDIR}
    FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
