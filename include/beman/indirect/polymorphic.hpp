// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef BEMAN_INDIRECT_POLYMORPHIC_HPP
#define BEMAN_INDIRECT_POLYMORPHIC_HPP

#include <beman/indirect/detail/config.hpp>
#include <beman/indirect/detail/synth_three_way.hpp>

#include <cassert>
#include <initializer_list>
#include <memory>
#include <memory_resource>
#include <type_traits>
#include <utility>

namespace beman::indirect {

namespace detail {

// Check if U is a specialization of in_place_type_t.
template <class>
inline constexpr bool is_in_place_type_v = false;

template <class T>
inline constexpr bool is_in_place_type_v<std::in_place_type_t<T>> = true;

// Abstract control block for type-erased polymorphic storage.
template <class T, class Allocator>
struct control_block {
    T* p_;

    virtual control_block* clone(const Allocator& alloc) const = 0;
    virtual control_block* move_clone(const Allocator& alloc)  = 0;
    virtual void           destroy(Allocator& alloc) noexcept  = 0;

  protected:
    ~control_block() = default;
};

// Concrete control block that stores a value of type U (derived from T) inline.
template <class T, class U, class Allocator>
struct direct_control_block final : control_block<T, Allocator> {
    using cb_alloc  = typename std::allocator_traits<Allocator>::template rebind_alloc<direct_control_block>;
    using cb_traits = std::allocator_traits<cb_alloc>;

    union storage {
        U value;
        storage() {}
        ~storage() {}
    } storage_;

    template <class... Args>
    explicit direct_control_block(Args&&... args) {
        detail::construct_at_impl(std::addressof(storage_.value), std::forward<Args>(args)...);
        this->p_ = std::addressof(storage_.value);
    }

    control_block<T, Allocator>* clone(const Allocator& alloc) const override {
        cb_alloc a(alloc);
        auto*    mem = cb_traits::allocate(a, 1);
        try {
            cb_traits::construct(a, mem, storage_.value);
        } catch (...) {
            cb_traits::deallocate(a, mem, 1);
            throw;
        }
        return mem;
    }

    control_block<T, Allocator>* move_clone(const Allocator& alloc) override {
        cb_alloc a(alloc);
        auto*    mem = cb_traits::allocate(a, 1);
        try {
            cb_traits::construct(a, mem, std::move(storage_.value));
        } catch (...) {
            cb_traits::deallocate(a, mem, 1);
            throw;
        }
        return mem;
    }

    void destroy(Allocator& alloc) noexcept override {
        cb_alloc a(alloc);
        std::destroy_at(std::addressof(storage_.value));
        cb_traits::destroy(a, this);
        cb_traits::deallocate(a, this, 1);
    }
};

} // namespace detail

// [polymorphic] Class template polymorphic
// N5032 §20.4.2
template <class T, class Allocator = std::allocator<T>>
class polymorphic {
    static_assert(std::is_object_v<T>, "T must be an object type");
    static_assert(!std::is_array_v<T>, "T must not be an array type");
    static_assert(!std::is_const_v<T> && !std::is_volatile_v<T>, "T must not be cv-qualified");
    static_assert(std::is_same_v<T, typename std::allocator_traits<Allocator>::value_type>,
                  "Allocator::value_type must be T");

    using alloc_traits = std::allocator_traits<Allocator>;
    using cb_type      = detail::control_block<T, Allocator>;

    template <class U>
    using direct_cb = detail::direct_control_block<T, U, Allocator>;

    template <class U>
    using cb_alloc = typename alloc_traits::template rebind_alloc<direct_cb<U>>;

    template <class U>
    using cb_traits = std::allocator_traits<cb_alloc<U>>;

  public:
    using value_type     = T;
    using allocator_type = Allocator;
    using pointer        = typename alloc_traits::pointer;
    using const_pointer  = typename alloc_traits::const_pointer;

    // [polymorphic.ctor] constructors

#if BEMAN_INDIRECT_USE_CONCEPTS
    constexpr explicit polymorphic()
        requires std::is_default_constructible_v<Allocator>
    {
#else
    template <class Alloc_ = Allocator, std::enable_if_t<std::is_default_constructible_v<Alloc_>, int> = 0>
    constexpr explicit polymorphic() {
#endif
        static_assert(std::is_default_constructible_v<T>);
        static_assert(std::is_copy_constructible_v<T>);
        cb_ = make_cb<T>(alloc_);
    }

    constexpr explicit polymorphic(std::allocator_arg_t, const Allocator& a) : alloc_(a) {
        static_assert(std::is_default_constructible_v<T>);
        static_assert(std::is_copy_constructible_v<T>);
        cb_ = make_cb<T>(alloc_);
    }

    constexpr polymorphic(const polymorphic& other)
        : alloc_(alloc_traits::select_on_container_copy_construction(other.alloc_)) {
        if (!other.valueless_after_move()) {
            cb_ = other.cb_->clone(alloc_);
        }
    }

    constexpr polymorphic(std::allocator_arg_t, const Allocator& a, const polymorphic& other) : alloc_(a) {
        if (!other.valueless_after_move()) {
            cb_ = other.cb_->clone(alloc_);
        }
    }

    constexpr polymorphic(polymorphic&& other) noexcept : alloc_(std::move(other.alloc_)), cb_(other.cb_) {
        other.cb_ = nullptr;
    }

    constexpr polymorphic(std::allocator_arg_t,
                          const Allocator& a,
                          polymorphic&&    other) noexcept(alloc_traits::is_always_equal::value)
        : alloc_(a) {
        if (other.valueless_after_move()) {
            // *this is valueless
        } else if constexpr (alloc_traits::is_always_equal::value) {
            cb_       = other.cb_;
            other.cb_ = nullptr;
        } else {
            if (alloc_ == other.alloc_) {
                cb_       = other.cb_;
                other.cb_ = nullptr;
            } else {
                cb_ = other.cb_->move_clone(alloc_);
                other.reset();
            }
        }
    }

#if BEMAN_INDIRECT_USE_CONCEPTS
    template <class U = T>
        requires(!std::is_same_v<detail::remove_cvref_t<U>, polymorphic> &&
                 detail::derived_from_v<detail::remove_cvref_t<U>, T> &&
                 std::is_constructible_v<detail::remove_cvref_t<U>, U> &&
                 std::is_copy_constructible_v<detail::remove_cvref_t<U>> &&
                 !detail::is_in_place_type_v<detail::remove_cvref_t<U>> && std::is_default_constructible_v<Allocator>)
    constexpr explicit polymorphic(U&& u) {
#else
    template <class U               = T,
              std::enable_if_t<!std::is_same_v<detail::remove_cvref_t<U>, polymorphic> &&
                                   detail::derived_from_v<detail::remove_cvref_t<U>, T> &&
                                   std::is_constructible_v<detail::remove_cvref_t<U>, U> &&
                                   std::is_copy_constructible_v<detail::remove_cvref_t<U>> &&
                                   !detail::is_in_place_type_v<detail::remove_cvref_t<U>> &&
                                   std::is_default_constructible_v<Allocator>,
                               int> = 0>
    constexpr explicit polymorphic(U&& u) {
#endif
        cb_ = make_cb<detail::remove_cvref_t<U>>(alloc_, std::forward<U>(u));
    }

#if BEMAN_INDIRECT_USE_CONCEPTS
    template <class U = T>
        requires(!std::is_same_v<detail::remove_cvref_t<U>, polymorphic> &&
                 detail::derived_from_v<detail::remove_cvref_t<U>, T> &&
                 std::is_constructible_v<detail::remove_cvref_t<U>, U> &&
                 std::is_copy_constructible_v<detail::remove_cvref_t<U>> &&
                 !detail::is_in_place_type_v<detail::remove_cvref_t<U>>)
    constexpr explicit polymorphic(std::allocator_arg_t, const Allocator& a, U&& u) : alloc_(a) {
#else
    template <class U               = T,
              std::enable_if_t<!std::is_same_v<detail::remove_cvref_t<U>, polymorphic> &&
                                   detail::derived_from_v<detail::remove_cvref_t<U>, T> &&
                                   std::is_constructible_v<detail::remove_cvref_t<U>, U> &&
                                   std::is_copy_constructible_v<detail::remove_cvref_t<U>> &&
                                   !detail::is_in_place_type_v<detail::remove_cvref_t<U>>,
                               int> = 0>
    constexpr explicit polymorphic(std::allocator_arg_t, const Allocator& a, U&& u) : alloc_(a) {
#endif
        cb_ = make_cb<detail::remove_cvref_t<U>>(alloc_, std::forward<U>(u));
    }

#if BEMAN_INDIRECT_USE_CONCEPTS
    template <class U, class... Ts>
        requires(std::is_same_v<detail::remove_cvref_t<U>, U> && detail::derived_from_v<U, T> &&
                 std::is_constructible_v<U, Ts...> && std::is_copy_constructible_v<U> &&
                 std::is_default_constructible_v<Allocator>)
    constexpr explicit polymorphic(std::in_place_type_t<U>, Ts&&... ts) {
#else
    template <class U,
              class... Ts,
              std::enable_if_t<std::is_same_v<detail::remove_cvref_t<U>, U> && detail::derived_from_v<U, T> &&
                                   std::is_constructible_v<U, Ts...> && std::is_copy_constructible_v<U> &&
                                   std::is_default_constructible_v<Allocator>,
                               int> = 0>
    constexpr explicit polymorphic(std::in_place_type_t<U>, Ts&&... ts) {
#endif
        cb_ = make_cb<U>(alloc_, std::forward<Ts>(ts)...);
    }

#if BEMAN_INDIRECT_USE_CONCEPTS
    template <class U, class... Ts>
        requires(std::is_same_v<detail::remove_cvref_t<U>, U> && detail::derived_from_v<U, T> &&
                 std::is_constructible_v<U, Ts...> && std::is_copy_constructible_v<U>)
    constexpr explicit polymorphic(std::allocator_arg_t, const Allocator& a, std::in_place_type_t<U>, Ts&&... ts)
        : alloc_(a) {
#else
    template <class U,
              class... Ts,
              std::enable_if_t<std::is_same_v<detail::remove_cvref_t<U>, U> && detail::derived_from_v<U, T> &&
                                   std::is_constructible_v<U, Ts...> && std::is_copy_constructible_v<U>,
                               int> = 0>
    constexpr explicit polymorphic(std::allocator_arg_t, const Allocator& a, std::in_place_type_t<U>, Ts&&... ts)
        : alloc_(a) {
#endif
        cb_ = make_cb<U>(alloc_, std::forward<Ts>(ts)...);
    }

#if BEMAN_INDIRECT_USE_CONCEPTS
    template <class U, class I, class... Us>
        requires(std::is_same_v<detail::remove_cvref_t<U>, U> && detail::derived_from_v<U, T> &&
                 std::is_constructible_v<U, std::initializer_list<I>&, Us...> && std::is_copy_constructible_v<U> &&
                 std::is_default_constructible_v<Allocator>)
    constexpr explicit polymorphic(std::in_place_type_t<U>, std::initializer_list<I> ilist, Us&&... us) {
#else
    template <class U,
              class I,
              class... Us,
              std::enable_if_t<std::is_same_v<detail::remove_cvref_t<U>, U> && detail::derived_from_v<U, T> &&
                                   std::is_constructible_v<U, std::initializer_list<I>&, Us...> &&
                                   std::is_copy_constructible_v<U> && std::is_default_constructible_v<Allocator>,
                               int> = 0>
    constexpr explicit polymorphic(std::in_place_type_t<U>, std::initializer_list<I> ilist, Us&&... us) {
#endif
        cb_ = make_cb<U>(alloc_, ilist, std::forward<Us>(us)...);
    }

#if BEMAN_INDIRECT_USE_CONCEPTS
    template <class U, class I, class... Us>
        requires(std::is_same_v<detail::remove_cvref_t<U>, U> && detail::derived_from_v<U, T> &&
                 std::is_constructible_v<U, std::initializer_list<I>&, Us...> && std::is_copy_constructible_v<U>)
    constexpr explicit polymorphic(
        std::allocator_arg_t, const Allocator& a, std::in_place_type_t<U>, std::initializer_list<I> ilist, Us&&... us)
        : alloc_(a) {
#else
    template <class U,
              class I,
              class... Us,
              std::enable_if_t<std::is_same_v<detail::remove_cvref_t<U>, U> && detail::derived_from_v<U, T> &&
                                   std::is_constructible_v<U, std::initializer_list<I>&, Us...> &&
                                   std::is_copy_constructible_v<U>,
                               int> = 0>
    constexpr explicit polymorphic(
        std::allocator_arg_t, const Allocator& a, std::in_place_type_t<U>, std::initializer_list<I> ilist, Us&&... us)
        : alloc_(a) {
#endif
        cb_ = make_cb<U>(alloc_, ilist, std::forward<Us>(us)...);
    }

    // [polymorphic.dtor] destructor

    BEMAN_INDIRECT_CONSTEXPR_DTOR ~polymorphic() {
        static_assert(detail::is_complete_v<T>);
        reset();
    }

    // [polymorphic.assign] assignment

    constexpr polymorphic& operator=(const polymorphic& other) {
        static_assert(detail::is_complete_v<T>);
        if (std::addressof(other) == this)
            return *this;

        constexpr bool pocca                  = alloc_traits::propagate_on_container_copy_assignment::value;
        Allocator      alloc_for_construction = pocca ? other.alloc_ : alloc_;

        cb_type* new_cb = nullptr;
        if (!other.valueless_after_move()) {
            new_cb = other.cb_->clone(alloc_for_construction);
        }
        reset();
        cb_ = new_cb;

        if constexpr (pocca) {
            alloc_ = other.alloc_;
        }
        return *this;
    }

    constexpr polymorphic&
    operator=(polymorphic&& other) noexcept(alloc_traits::propagate_on_container_move_assignment::value ||
                                            alloc_traits::is_always_equal::value) {
        static_assert(alloc_traits::propagate_on_container_move_assignment::value ||
                          alloc_traits::is_always_equal::value || detail::is_complete_v<T>,
                      "T must be complete when allocators may not be equal");
        if (std::addressof(other) == this)
            return *this;

        constexpr bool pocma = alloc_traits::propagate_on_container_move_assignment::value;

        if (other.valueless_after_move()) {
            reset();
        } else if (pocma || alloc_ == other.alloc_) {
            reset();
            cb_       = other.cb_;
            other.cb_ = nullptr;
        } else {
            cb_type* new_cb = other.cb_->move_clone(alloc_);
            reset();
            cb_ = new_cb;
            other.reset();
        }

        if constexpr (pocma) {
            alloc_ = other.alloc_;
        }
        return *this;
    }

    // [polymorphic.obs] observers

    constexpr const T& operator*() const noexcept {
        assert(!valueless_after_move());
        return *(cb_->p_);
    }

    constexpr T& operator*() noexcept {
        assert(!valueless_after_move());
        return *(cb_->p_);
    }

    constexpr const_pointer operator->() const noexcept {
        assert(!valueless_after_move());
        return std::pointer_traits<const_pointer>::pointer_to(*(cb_->p_));
    }

    constexpr pointer operator->() noexcept {
        assert(!valueless_after_move());
        return std::pointer_traits<pointer>::pointer_to(*(cb_->p_));
    }

    constexpr bool valueless_after_move() const noexcept { return cb_ == nullptr; }

    constexpr allocator_type get_allocator() const noexcept { return alloc_; }

    // [polymorphic.swap] swap

    constexpr void swap(polymorphic& other) noexcept(alloc_traits::propagate_on_container_swap::value ||
                                                     alloc_traits::is_always_equal::value) {
        // Precondition: allocators must be equal when they don't propagate on swap.
        assert(alloc_traits::propagate_on_container_swap::value || alloc_ == other.alloc_);
        using std::swap;
        swap(cb_, other.cb_);
        if constexpr (alloc_traits::propagate_on_container_swap::value) {
            swap(alloc_, other.alloc_);
        }
    }

    friend constexpr void swap(polymorphic& lhs, polymorphic& rhs) noexcept(noexcept(lhs.swap(rhs))) { lhs.swap(rhs); }

  private:
    template <class U, class... Args>
    static cb_type* make_cb(Allocator& alloc, Args&&... args) {
        cb_alloc<U> a(alloc);
        auto*       mem = cb_traits<U>::allocate(a, 1);
        try {
            cb_traits<U>::construct(a, mem, std::forward<Args>(args)...);
        } catch (...) {
            cb_traits<U>::deallocate(a, mem, 1);
            throw;
        }
        return mem;
    }

    constexpr void reset() {
        if (cb_) {
            cb_->destroy(alloc_);
            cb_ = nullptr;
        }
    }

    BEMAN_INDIRECT_NO_UNIQUE_ADDRESS Allocator alloc_ = Allocator();
    cb_type*                                   cb_    = nullptr;
};

} // namespace beman::indirect

namespace beman::indirect::pmr {

template <class T>
using polymorphic = beman::indirect::polymorphic<T, std::pmr::polymorphic_allocator<T>>;

} // namespace beman::indirect::pmr

#endif // BEMAN_INDIRECT_POLYMORPHIC_HPP
