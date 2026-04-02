// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef BEMAN_INDIRECT_DETAIL_SYNTH_THREE_WAY_HPP
#define BEMAN_INDIRECT_DETAIL_SYNTH_THREE_WAY_HPP

#include <beman/indirect/detail/config.hpp>

#include <type_traits>

namespace beman::indirect::detail {

// is_complete_v: check if T is a complete type at the point of instantiation.
template <class T, class = void>
inline constexpr bool is_complete_v = false;

template <class T>
inline constexpr bool is_complete_v<T, std::void_t<decltype(sizeof(T))>> = true;

} // namespace beman::indirect::detail

#if BEMAN_INDIRECT_USE_THREE_WAY_COMPARISON

namespace beman::indirect::detail {

// synth-three-way per [expos.only.entity]
struct synth_three_way_fn {
    template <class T, class U>
    constexpr auto operator()(const T& t, const U& u) const {
        if constexpr (std::three_way_comparable_with<T, U>) {
            return t <=> u;
        } else {
            if (t < u)
                return std::weak_ordering::less;
            if (u < t)
                return std::weak_ordering::greater;
            return std::weak_ordering::equivalent;
        }
    }
};

inline constexpr synth_three_way_fn synth_three_way{};

// synth-three-way-result
template <class T, class U = T>
using synth_three_way_result = decltype(synth_three_way(std::declval<const T&>(), std::declval<const U&>()));

} // namespace beman::indirect::detail

#endif // BEMAN_INDIRECT_USE_THREE_WAY_COMPARISON

#endif // BEMAN_INDIRECT_DETAIL_SYNTH_THREE_WAY_HPP
