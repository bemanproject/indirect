// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef BEMAN_INDIRECT_INDIRECT_HPP
#define BEMAN_INDIRECT_INDIRECT_HPP

#include <beman/indirect/detail/synth_three_way.hpp>

#include <cassert>
#include <compare>
#include <concepts>
#include <functional>
#include <initializer_list>
#include <memory>
#include <memory_resource>
#include <type_traits>
#include <utility>

namespace beman::indirect {

// Forward declaration for is_indirect_v trait.
template <class T, class Allocator>
class indirect;

namespace detail {

// Trait to detect indirect specializations.
// Used to prevent recursive constraint evaluation in comparison-with-T operators.
template <class>
inline constexpr bool is_indirect_v = false;

template <class T, class A>
inline constexpr bool is_indirect_v<indirect<T, A>> = true;

} // namespace detail

// [indirect] Class template indirect
// N5032 §20.4.1
template <class T, class Allocator = std::allocator<T>>
class indirect {
    static_assert(std::is_object_v<T>, "T must be an object type");
    static_assert(!std::is_array_v<T>, "T must not be an array type");
    static_assert(!std::is_const_v<T> && !std::is_volatile_v<T>, "T must not be cv-qualified");
    static_assert(std::is_same_v<T, typename std::allocator_traits<Allocator>::value_type>,
                  "Allocator::value_type must be T");

    using alloc_traits = std::allocator_traits<Allocator>;

  public:
    using value_type     = T;
    using allocator_type = Allocator;
    using pointer        = typename alloc_traits::pointer;
    using const_pointer  = typename alloc_traits::const_pointer;

    // [indirect.ctor] constructors

    constexpr explicit indirect()
        requires std::is_default_constructible_v<Allocator>
    {
        static_assert(std::is_default_constructible_v<T>);
        p_ = construct_from(alloc_);
    }

    constexpr explicit indirect(std::allocator_arg_t, const Allocator& a) : alloc_(a) {
        static_assert(std::is_default_constructible_v<T>);
        p_ = construct_from(alloc_);
    }

    constexpr indirect(const indirect& other)
        : alloc_(alloc_traits::select_on_container_copy_construction(other.alloc_)) {
        static_assert(std::is_copy_constructible_v<T>);
        if (!other.valueless_after_move()) {
            p_ = construct_from(alloc_, *other);
        }
    }

    constexpr indirect(std::allocator_arg_t, const Allocator& a, const indirect& other) : alloc_(a) {
        static_assert(std::is_copy_constructible_v<T>);
        if (!other.valueless_after_move()) {
            p_ = construct_from(alloc_, *other);
        }
    }

    constexpr indirect(indirect&& other) noexcept : alloc_(std::move(other.alloc_)), p_(other.p_) {
        other.p_ = nullptr;
    }

    constexpr indirect(std::allocator_arg_t, const Allocator& a, indirect&& other) noexcept(
        alloc_traits::is_always_equal::value)
        : alloc_(a) {
        static_assert(alloc_traits::is_always_equal::value || detail::is_complete_v<T>);
        if (other.valueless_after_move()) {
            // *this is valueless
        } else if constexpr (alloc_traits::is_always_equal::value) {
            p_ = other.p_;
            other.p_ = nullptr;
        } else {
            if (alloc_ == other.alloc_) {
                p_ = other.p_;
                other.p_ = nullptr;
            } else {
                p_ = construct_from(alloc_, std::move(*other));
                other.reset();
            }
        }
    }

    template <class U = T>
        requires(!std::is_same_v<std::remove_cvref_t<U>, indirect> &&
                 !std::is_same_v<std::remove_cvref_t<U>, std::in_place_t> &&
                 std::is_constructible_v<T, U> && std::is_default_constructible_v<Allocator>)
    constexpr explicit indirect(U&& u) {
        p_ = construct_from(alloc_, std::forward<U>(u));
    }

    template <class U = T>
        requires(!std::is_same_v<std::remove_cvref_t<U>, indirect> &&
                 !std::is_same_v<std::remove_cvref_t<U>, std::in_place_t> && std::is_constructible_v<T, U>)
    constexpr explicit indirect(std::allocator_arg_t, const Allocator& a, U&& u) : alloc_(a) {
        p_ = construct_from(alloc_, std::forward<U>(u));
    }

    template <class... Us>
        requires(std::is_constructible_v<T, Us...> && std::is_default_constructible_v<Allocator>)
    constexpr explicit indirect(std::in_place_t, Us&&... us) {
        p_ = construct_from(alloc_, std::forward<Us>(us)...);
    }

    template <class... Us>
        requires(std::is_constructible_v<T, Us...>)
    constexpr explicit indirect(std::allocator_arg_t, const Allocator& a, std::in_place_t, Us&&... us)
        : alloc_(a) {
        p_ = construct_from(alloc_, std::forward<Us>(us)...);
    }

    template <class I, class... Us>
        requires(std::is_constructible_v<T, std::initializer_list<I>&, Us...> &&
                 std::is_default_constructible_v<Allocator>)
    constexpr explicit indirect(std::in_place_t, std::initializer_list<I> ilist, Us&&... us) {
        p_ = construct_from(alloc_, ilist, std::forward<Us>(us)...);
    }

    template <class I, class... Us>
        requires(std::is_constructible_v<T, std::initializer_list<I>&, Us...>)
    constexpr explicit indirect(std::allocator_arg_t, const Allocator& a, std::in_place_t,
                                std::initializer_list<I> ilist, Us&&... us)
        : alloc_(a) {
        p_ = construct_from(alloc_, ilist, std::forward<Us>(us)...);
    }

    // [indirect.dtor] destructor

    constexpr ~indirect() {
        static_assert(detail::is_complete_v<T>);
        reset();
    }

    // [indirect.assign] assignment

    constexpr indirect& operator=(const indirect& other) {
        static_assert(std::is_copy_assignable_v<T>);
        static_assert(std::is_copy_constructible_v<T>);
        if (std::addressof(other) == this)
            return *this;

        constexpr bool pocca = alloc_traits::propagate_on_container_copy_assignment::value;
        Allocator alloc_for_construction = pocca ? other.alloc_ : alloc_;

        if (other.valueless_after_move()) {
            reset();
        } else if (!valueless_after_move() && alloc_ == alloc_for_construction) {
            **this = *other;
        } else {
            // Construct new first for strong exception guarantee
            pointer new_p = construct_from(alloc_for_construction, *other);
            reset();
            p_ = new_p;
        }

        if constexpr (pocca) {
            alloc_ = other.alloc_;
        }
        return *this;
    }

    constexpr indirect& operator=(indirect&& other) noexcept(
        alloc_traits::propagate_on_container_move_assignment::value ||
        alloc_traits::is_always_equal::value) {
        static_assert(alloc_traits::propagate_on_container_move_assignment::value ||
                          alloc_traits::is_always_equal::value || std::is_move_constructible_v<T>,
                      "T must be move constructible when allocators may not be equal");
        if (std::addressof(other) == this)
            return *this;

        constexpr bool pocma = alloc_traits::propagate_on_container_move_assignment::value;

        if (other.valueless_after_move()) {
            reset();
        } else if (pocma || alloc_ == other.alloc_) {
            reset();
            p_ = other.p_;
            other.p_ = nullptr;
        } else {
            // Allocators differ and don't propagate: must move-construct
            pointer new_p = construct_from(alloc_, std::move(*other));
            reset();
            p_ = new_p;
            other.reset();
        }

        if constexpr (pocma) {
            alloc_ = other.alloc_;
        }
        return *this;
    }

    template <class U = T>
        requires(!std::is_same_v<std::remove_cvref_t<U>, indirect> && std::is_constructible_v<T, U> &&
                 std::is_assignable_v<T&, U>)
    constexpr indirect& operator=(U&& u) {
        if (valueless_after_move()) {
            p_ = construct_from(alloc_, std::forward<U>(u));
        } else {
            **this = std::forward<U>(u);
        }
        return *this;
    }

    // [indirect.obs] observers

    constexpr const T& operator*() const& noexcept {
        assert(!valueless_after_move());
        return *p_;
    }

    constexpr T& operator*() & noexcept {
        assert(!valueless_after_move());
        return *p_;
    }

    constexpr const T&& operator*() const&& noexcept {
        assert(!valueless_after_move());
        return std::move(*p_);
    }

    constexpr T&& operator*() && noexcept {
        assert(!valueless_after_move());
        return std::move(*p_);
    }

    constexpr const_pointer operator->() const noexcept {
        assert(!valueless_after_move());
        return p_;
    }

    constexpr pointer operator->() noexcept {
        assert(!valueless_after_move());
        return p_;
    }

    constexpr bool valueless_after_move() const noexcept { return p_ == nullptr; }

    constexpr allocator_type get_allocator() const noexcept { return alloc_; }

    // [indirect.swap] swap

    constexpr void swap(indirect& other) noexcept(alloc_traits::propagate_on_container_swap::value ||
                                                   alloc_traits::is_always_equal::value) {
        // Precondition: allocators must be equal when they don't propagate on swap.
        assert(alloc_traits::propagate_on_container_swap::value ||
               alloc_ == other.alloc_);
        using std::swap;
        swap(p_, other.p_);
        if constexpr (alloc_traits::propagate_on_container_swap::value) {
            swap(alloc_, other.alloc_);
        }
    }

    friend constexpr void swap(indirect& lhs, indirect& rhs) noexcept(noexcept(lhs.swap(rhs))) {
        lhs.swap(rhs);
    }

    // [indirect.relops] relational operators

    template <class U, class AA>
    friend constexpr bool operator==(const indirect& lhs,
                                     const indirect<U, AA>& rhs) noexcept(noexcept(*lhs == *rhs)) {
        if (lhs.valueless_after_move() || rhs.valueless_after_move())
            return lhs.valueless_after_move() == rhs.valueless_after_move();
        return *lhs == *rhs;
    }

    template <class U, class AA>
    friend constexpr auto operator<=>(const indirect& lhs, const indirect<U, AA>& rhs)
        -> detail::synth_three_way_result<T, U> {
        if (lhs.valueless_after_move() || rhs.valueless_after_move())
            return !lhs.valueless_after_move() <=> !rhs.valueless_after_move();
        return detail::synth_three_way(*lhs, *rhs);
    }

    // [indirect.comp.with.t] comparison with T

    template <class U>
        requires(!detail::is_indirect_v<U>)
    friend constexpr bool operator==(const indirect& lhs,
                                     const U& rhs) noexcept(noexcept(*lhs == rhs)) {
        if (lhs.valueless_after_move())
            return false;
        return *lhs == rhs;
    }

    template <class U>
        requires(!detail::is_indirect_v<U>)
    friend constexpr auto operator<=>(const indirect& lhs, const U& rhs) {
        if (lhs.valueless_after_move())
            return detail::synth_three_way_result<T, U>(std::strong_ordering::less);
        return detail::synth_three_way(*lhs, rhs);
    }

  private:
    template <class... Args>
    static constexpr pointer construct_from(Allocator& a, Args&&... args) {
        pointer p = alloc_traits::allocate(a, 1);
        try {
            alloc_traits::construct(a, std::to_address(p), std::forward<Args>(args)...);
        } catch (...) {
            alloc_traits::deallocate(a, p, 1);
            throw;
        }
        return p;
    }

    static constexpr void destroy_with(Allocator& a, pointer p) {
        alloc_traits::destroy(a, std::to_address(p));
        alloc_traits::deallocate(a, p, 1);
    }

    constexpr void reset() {
        if (p_) {
            destroy_with(alloc_, p_);
            p_ = nullptr;
        }
    }

    pointer                            p_     = pointer();
    [[no_unique_address]] Allocator alloc_ = Allocator();
};

// Deduction guides
template <class Value>
indirect(Value) -> indirect<Value>;

template <class Allocator, class Value>
indirect(std::allocator_arg_t, Allocator, Value)
    -> indirect<Value, typename std::allocator_traits<Allocator>::template rebind_alloc<Value>>;

} // namespace beman::indirect

// [indirect.hash] Hash support
template <class T, class Allocator>
    requires std::is_default_constructible_v<std::hash<T>>
struct std::hash<beman::indirect::indirect<T, Allocator>> {
    constexpr std::size_t operator()(const beman::indirect::indirect<T, Allocator>& i) const
        noexcept(noexcept(std::hash<T>{}(*i))) {
        if (i.valueless_after_move())
            return static_cast<std::size_t>(-1);
        return std::hash<T>{}(*i);
    }
};

namespace beman::indirect::pmr {

template <class T>
using indirect = beman::indirect::indirect<T, std::pmr::polymorphic_allocator<T>>;

} // namespace beman::indirect::pmr

#endif // BEMAN_INDIRECT_INDIRECT_HPP
