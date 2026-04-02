# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

target_sources(
    beman.indirect
    PUBLIC
        FILE_SET HEADERS
            FILES indirect.hpp polymorphic.hpp detail/synth_three_way.hpp
)
