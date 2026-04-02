# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set(ALL_EXAMPLES indirect_basic polymorphic_basic recursive_variant)
message("Examples to be built: ${ALL_EXAMPLES}")

foreach(example ${ALL_EXAMPLES})
    add_executable(beman.indirect.examples.${example})
    target_sources(beman.indirect.examples.${example} PRIVATE ${example}.cpp)
    target_link_libraries(
        beman.indirect.examples.${example}
        PRIVATE beman::indirect
    )
endforeach()
