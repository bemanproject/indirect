// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef BEMAN_INDIRECT_IDENTITY_HPP
#define BEMAN_INDIRECT_IDENTITY_HPP

#include <initializer_list>
#include <memory>
#include <type_traits>
#include <utility>
#include <functional>

namespace beman::indirect {
template <class T, class Allocator = std::allocator<T>>
class indirect {
  public:
    using value_type     = T;
    using allocator_type = Allocator;
    using pointer        = typename std::allocator_traits<Allocator>::pointer;
    using const_pointer  = typename std::allocator_traits<Allocator>::const_pointer;

    /**
     * Constraints: is_default_constructible_v<Allocator> is true.
     *
     * Mandates: is_default_constructible_v<T> is true.
     *
     * Effects: Constructs an owned object of type T with an empty argument list, using the allocator alloc.
     */
    explicit constexpr indirect() : alloc(Allocator{}), p(allocate_and_construct()) {}

    /**
     * Mandates: is_default_constructible_v<T> is true.
     *
     * Effects: alloc is direct-non-list-initialized with a.
     * Constructs an owned object of type T with an empty argument list, using the allocator alloc.
     */
    explicit constexpr indirect(std::allocator_arg_t, const Allocator& a) : alloc(a), p(allocate_and_construct()) {}

    /**
     * Mandates: is_copy_constructible_v<T> is true.
     *
     * Effects: alloc is direct-non-list-initialized with
     * allocator_traits<Allocator>::select_on_container_copy_construction(other.alloc).
     * If other is valueless, *this is valueless.
     * Otherwise, constructs an owned object of type T with *other, using the allocator alloc.
     */
    constexpr indirect(const indirect& other)
        : alloc(Allocator{}), p(other.valueless_after_move() ? nullptr : allocate_and_construct(*other)) {}

    /**
     * Mandates: is_copy_constructible_v<T> is true.
     *
     * Effects: alloc is direct-non-list-initialized with a. If other is valueless, *this is valueless.
     * Otherwise, constructs an owned object of type T with *other, using the allocator alloc.
     */
    constexpr indirect(std::allocator_arg_t, const Allocator& a, const indirect& other)
        : alloc(a), p(other.valueless_after_move() ? nullptr : allocate_and_construct(*other)) {}

    /**
     * Effects: alloc is direct-non-list-initialized from std::move(other.alloc).
     * If other is valueless, *this is valueless.
     * Otherwise *this takes ownership of the owned object of other.
     *
     * Postconditions: other is valueless.
     */
    constexpr indirect(indirect&& other) noexcept : alloc(std::move(other.alloc)), p(other.p) { other.p = nullptr; }

    /**
     * Mandates: If allocator_traits<Allocator>::is_always_equal::value is false then T is a complete type.
     *
     * Effects: alloc is direct-non-list-initialized with a. If other is valueless, *this is valueless.
     * Otherwise, if alloc == other.alloc is true,
     * constructs an object of type indirect that takes ownership of the owned object of other.
     * Otherwise, constructs an owned object of type T with *std::move(other), using the allocator alloc.
     *
     * Postconditions: other is valueless.
     */
    constexpr indirect(std::allocator_arg_t,
                       const Allocator& a,
                       indirect&&       other) noexcept(std::allocator_traits<Allocator>::is_always_equal::value)
        : alloc(a) {
        // Shortcut 1: Taking over ownership directly
        if (this->alloc == other.alloc) {
            this->p = other.p;
            other.p = nullptr;
            return;
        }

        // Shortcut 2: other is valueless
        if (other.valueless_after_move()) {
            this->p = nullptr;
            return;
        }

        // Move constructing
        finally post_condition([&other]() {
            // Post condition: other must be set to valueless regardless of exception state
            // Here destroy or deallocate could throw.
            finally set_valueless([&other]() { other.p = nullptr; });
            // valueless check done already
            other.unchecked_destroy_and_deallocate();
            set_valueless.invoke();
        });

        this->p = allocate_ptr();

        // We have to rewind allocation if there's exception here.
        try {
            this->construct(std::move(*other));
        } catch (...) {
            this->deallocate();
        }

        // satisfy post condition
        post_condition.invoke();
    }

    /**
     * Constraints:
     * • is_same_v<remove_cvref_t<U>, indirect> is false,
     * • is_same_v<remove_cvref_t<U>, in_place_t> is false,
     * • is_constructible_v<T, U> is true, and
     * • is_default_constructible_v<Allocator> is true.
     *
     * Effects: Constructs an owned object of type T with std::forward<U>(u), using the allocator alloc.
     */
    template <class U = T>
    explicit constexpr indirect(U&& u)
        requires((!std::is_same_v<std::remove_cvref_t<U>, indirect>) &&        //
                 (!std::is_same_v<std::remove_cvref_t<U>, std::in_place_t>) && //
                 std::is_constructible_v<T, U> &&                              //
                 std::is_default_constructible_v<Allocator>)
        : alloc(Allocator{}), p(allocate_and_construct(std::forward<U>(u))) {}

    /**
     * Constraints:
     * • is_same_v<remove_cvref_t<U>, indirect> is false,
     * • is_same_v<remove_cvref_t<U>, in_place_t> is false, and
     * • is_constructible_v<T, U> is true.
     *
     * Effects: alloc is direct-non-list-initialized with a.
     * Constructs an owned object of type T with std::forward<U>(u), using the allocator alloc.
     */
    template <class U = T>
    explicit constexpr indirect(std::allocator_arg_t, const Allocator& a, U&& u)
        requires((!std::is_same_v<std::remove_cvref_t<U>, indirect>) &&        //
                 (!std::is_same_v<std::remove_cvref_t<U>, std::in_place_t>) && //
                 std::is_constructible_v<T, U>)
        : alloc(a), p(allocate_and_construct(std::forward<U>(u))) {}

    /**
     * Constraints:
     * • is_constructible_v<T, Us...> is true, and
     * • is_default_constructible_v<Allocator> is true.
     *
     * Effects: Constructs an owned object of type T with std::forward<Us>(us)..., using the allocator alloc.
     */
    template <class... Us>
    explicit constexpr indirect(std::in_place_t, Us&&... us)
        requires(std::is_constructible_v<T, Us...> && std::is_default_constructible_v<Allocator>)
        : alloc(Allocator{}), p(allocate_and_construct(std::forward<Us>(us)...)) {}

    /**
     * Constraints: is_constructible_v<T, Us...> is true.
     *
     * Effects: alloc is direct-non-list-initialized with a.
     * Constructs an owned object of type T with std::forward<Us>(us)..., using the allocator alloc.
     */
    template <class... Us>
    explicit constexpr indirect(std::allocator_arg_t, const Allocator& a, std::in_place_t, Us&&... us)
        requires(std::is_constructible_v<T, Us...>)
        : alloc(a), p(allocate_and_construct(std::forward<Us>(us)...)) {}

    /**
     * Constraints:
     * • is_constructible_v<T, initializer_list<I>&, Us...> is true, and
     * • is_default_constructible_v<Allocator> is true.
     *
     * Effects: Constructs an owned object of type T with the arguments ilist, std::forward<Us>(us)...,
     * using the allocator alloc.
     */
    template <class I, class... Us>
    explicit constexpr indirect(std::in_place_t, std::initializer_list<I> ilist, Us&&... us)
        requires(std::is_constructible_v<T, std::initializer_list<I>&, Us...> &&
                 std::is_default_constructible_v<Allocator>)
        : alloc(Allocator{}), p(allocate_and_construct(std::move(ilist), std::forward<Us>(us)...)) {}

    /**
     * Constraints: is_constructible_v<T, initializer_list<I>&, Us...> is true.
     *
     * Effects: alloc is direct-non-list-initialized with a. Constructs an owned object of type T with the arguments
     * ilist, std::forward<Us>(us)..., using the allocator alloc.
     */
    template <class I, class... Us>
    explicit constexpr indirect(
        std::allocator_arg_t, const Allocator& a, std::in_place_t, std::initializer_list<I> ilist, Us&&... us)
        requires(std::is_constructible_v<T, std::initializer_list<I>&, Us...>)
        : alloc(a), p(allocate_and_construct(std::move(ilist), std::forward<Us>(us)...)) {}

    /**
     * Mandates: T is a complete type.
     *
     * Effects: If *this is not valueless, destroys the owned object
     * using allocator_traits<Allocator>::destroy and then the storage is deallocated.
     */
    constexpr ~indirect() { checked_destroy_and_deallocate(); }

    /**
     * Mandates:
     * • is_copy_assignable_v<T> is true, and
     * • is_copy_constructible_v<T> is true.
     *
     * Effects: If addressof(other) == this is true, there are no effects.
     * Otherwise:
     * 1. The allocator needs updating if
     *    allocator_traits<Allocator>::propagate_on_container_copy_assignment::value is true.
     * 2. If other is valueless, *this becomes valueless and the owned object in *this, if any,
     *    is destroyed using allocator_traits<Allocator>::destroy and then the storage is deallocated.
     * 3. Otherwise, if alloc == other.alloc is true and *this is not valueless, equivalent to **this = *other.
     * 4. Otherwise a new owned object is constructed in *this using allocator_traits<Allocator>::construct
     *    with the owned object from other as the argument,
     *    using either the allocator in *this or the allocator in other if the allocator needs updating.
     * 5. The previously owned object in *this, if any,
     *    is destroyed using allocator_traits<Allocator>::destroy and then the storage is deallocated.
     * 6. If the allocator needs updating, the allocator in *this is replaced with a copy of the allocator in other.
     *
     * Returns: A reference to *this.
     *
     * Remarks:
     * - If any exception is thrown, the result of the expression this->valueless_after_move() remains unchanged.
     * - If an exception is thrown during the call to T's selected copy constructor, no effect.
     * - If an exception is thrown during the call to T's copy assignment, the state of its contained value is as
     *   defined by the exception safety guarantee of T's copy assignment.
     */
    constexpr indirect& operator=(const indirect& other) {
        if (this == std::addressof(other))
            return *this;

        //   1. The allocator needs updating if
        //      allocator_traits<Allocator>::propagate_on_container_copy_assignment::value is true.
        constexpr auto alloc_need_update =
            std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value;
        //   2. If other is valueless, *this becomes valueless and the owned object in *this, if any,
        //      is destroyed using allocator_traits<Allocator>::destroy and then the storage is deallocated.
        if (other.valueless_after_move()) {
            this->checked_destroy_and_deallocate();
            if (alloc_need_update)
                this->alloc = other.alloc;
            this->p = nullptr;
            return *this;
        }
        //   3. Otherwise, if alloc == other.alloc is true and *this is not valueless, equivalent to **this =
        //   *other.
        if (this->alloc == other.alloc && !this->valueless_after_move()) {
            **this = *other;
            return *this;
        }
        //   4. Otherwise a new owned object is constructed in *this using allocator_traits<Allocator>::construct
        //      with the owned object from other as the argument,
        //      using either the allocator in *this or the allocator in other if the allocator needs updating.
        //   5. The previously owned object in *this, if any,
        //      is destroyed using allocator_traits<Allocator>::destroy and then the storage is deallocated.

        // Updating allocator come after construction. Needed to fulfill exception guarantee: If an exception is
        // thrown during the call to T's selected copy constructor, no effect.
        auto new_ptr = std::invoke([&]() {
            if (!alloc_need_update)
                return this->allocate_and_construct(*other);

            // We need to construct a new storage using other's allocator
            // Allocator is copied twice here as other is a const&
            auto new_alloc = other.alloc;
            auto ptr       = std::allocator_traits<Allocator>::allocate(new_alloc, 1);
            try {
                std::allocator_traits<Allocator>::construct(new_alloc, ptr, *other);
            } catch (...) {
                std::allocator_traits<Allocator>::deallocate(new_alloc, ptr, 1);
                throw;
            }
            return ptr;
        });

        this->checked_destroy_and_deallocate();
        // 6. If the allocator needs updating, the allocator in *this is replaced with a copy of the allocator
        // in other.
        if (alloc_need_update) {
            this->alloc = other.alloc;
        }
        this->p = new_ptr;
        return *this;
    }

    /**
     * Mandates: is_copy_constructible_t<T> is true.
     *
     * Effects: If addressof(other) == this is true, there are no effects. Otherwise:
     * 1. The allocator needs updating if
     *    allocator_traits<Allocator>::propagate_on_container_move_assignment::value is true.
     * 2. If other is valueless, *this becomes valueless and the owned object in *this, if any,
     *    is destroyed using allocator_traits<Allocator>::destroy and then the storage is deallocated.
     * 3. Otherwise, if alloc == other.alloc is true, swaps the owned objects in *this and other;
     *    the owned object in other, if any,
     *    is then destroyed using allocator_traits<Allocator>::destroy and then the storage is deallocated.
     * 4. Otherwise constructs a new owned object with the owned object of other as the argument as an rvalue,
     *    using either the allocator in *this or the allocator in other if the allocator needs updating.
     * 5. The previously owned object in *this, if any,
     *    is destroyed using allocator_traits<Allocator>::destroy and then the storage is deallocated.
     * 6. If the allocator needs updating, the allocator in *this is replaced with a copy of the allocator in other.
     *
     * Postconditions: other is valueless.
     *
     * Returns: A reference to *this.
     *
     * Remarks: If any exception is thrown, there are no effects on *this or other.
     */
    constexpr indirect& operator=(indirect&& other) noexcept(
        std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value ||
        std::allocator_traits<Allocator>::is_always_equal::value) {
        if (this == std::addressof(other))
            return *this;

        // 1. The allocator needs updating if
        //    allocator_traits<Allocator>::propagate_on_container_move_assignment::value is true.
        constexpr auto need_update = std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value;
        // 2. If other is valueless, *this becomes valueless and the owned object in *this, if any,
        //    is destroyed using allocator_traits<Allocator>::destroy and then the storage is deallocated.
        if (other.valueless_after_move()) {
            this->checked_destroy_and_deallocate();
            this->p = nullptr;
            if (need_update)
                this->alloc = other.alloc;
            return *this;
        }
        // 3. Otherwise, if alloc == other.alloc is true, swaps the owned objects in *this and other;
        //    the owned object in other, if any,
        //    is then destroyed using allocator_traits<Allocator>::destroy and then the storage is deallocated.
        if (this->alloc == other.alloc) {
            std::swap(this->p, other.p);
            other.unchecked_destroy_and_deallocate();
            other.p = nullptr;
            return *this;
        }

        // 4. Otherwise constructs a new owned object with the owned object of other as the argument as an rvalue,
        //    using either the allocator in *this or the allocator in other if the allocator needs updating.
        // 5. The previously owned object in *this, if any,
        //    is destroyed using allocator_traits<Allocator>::destroy and then the storage is deallocated.

        // An exception could be thrown here, not swapping allocator for now.
        auto new_p = need_update ? other.allocate_and_construct(std::move(*other))
                                 : this->allocate_and_construct(std::move(*other));
        other.unchecked_destroy_and_deallocate();
        this->checked_destroy_and_deallocate();

        // All potentially throwing functions are done, commit state change.
        // alloc_update();
        if (need_update)
            this->alloc = other.alloc;
        other.p = nullptr;
        this->p = new_p;
        return *this;
    }

    /**
     * Constraints:
     * • is_same_v<remove_cvref_t<U>, indirect> is false,
     * • is_constructible_v<T, U> is true, and
     * • is_assignable_v<T&, U> is true.
     *
     * Effects: If *this is valueless then constructs an owned object of type T with std::forward<U>(u)
     * using the allocator alloc.
     * Otherwise, equivalent to **this = std::forward<U>(u).
     *
     * Returns: A reference to *this.
     */
    template <class U = T>
        requires(!std::is_same_v<std::remove_cvref_t<U>, indirect> && //
                 std::is_constructible_v<T, U> &&                     //
                 std::is_assignable_v<T&, U>)
    constexpr indirect& operator=(U&& u) {
        if (this->valueless_after_move()) {
            this->p = this->allocate_and_construct(std::forward<U>(u));
        } else {
            **this = std::forward<U>(u);
        }
        return *this;
    }

    /**
     * Preconditions: *this is not valueless.
     *
     * Returns: *p.
     */
    constexpr const T& operator*() const& noexcept { return *this->p; }

    /**
     * Preconditions: *this is not valueless.
     *
     * Returns: *p.
     */
    constexpr T& operator*() & noexcept { return *this->p; }

    /**
     * Preconditions: *this is not valueless.
     *
     * Returns: std::move(*p).
     */
    constexpr const T&& operator*() const&& noexcept { return std::move(*this->p); }

    /**
     * Preconditions: *this is not valueless.
     *
     * Returns: std::move(*p).
     */
    constexpr T&& operator*() && noexcept { return std::move(*this->p); }

    /**
     * Preconditions: *this is not valueless.
     *
     * Returns: p.
     */
    constexpr const_pointer operator->() const noexcept { return this->p; }

    /**
     * Preconditions: *this is not valueless.
     *
     * Returns: p.
     */
    constexpr pointer operator->() noexcept { return this->p; }

    /**
     * Returns: true if *this is valueless, otherwise false.
     */
    constexpr bool valueless_after_move() const noexcept { return this->p == nullptr; }

    /**
     * Returns: alloc.
     */
    constexpr allocator_type get_allocator() const noexcept { return this->alloc; }

    /**
     * Preconditions: If allocator_traits<Allocator>::propagate_on_container_swap::value is true,
     * then Allocator meets the Cpp17Swappable requirements.
     * Otherwise get_allocator() == other.get_allocator() is true.
     *
     * Effects: Swaps the states of *this and other, exchanging owned objects or valueless states.
     * If allocator_traits<Allocator>::propagate_on_container_swap::value is true,
     * then the allocators of *this and other are exchanged by calling swap as described in [swappable.requirements].
     * Otherwise, the allocators are not swapped.
     * [Note: Does not call swap on the owned objects directly. –end note]
     */
    constexpr void
    swap(indirect& other) noexcept(std::allocator_traits<Allocator>::propagate_on_container_swap::value ||
                                   std::allocator_traits<Allocator>::is_always_equal::value) {
        if constexpr (std::allocator_traits<Allocator>::propagate_on_container_swap::value) {
            std::swap(this->alloc, other.alloc);
        }
        std::swap(this->p, other.p);
    }

    /**
     * Effects: Equivalent to lhs.swap(rhs).
     */
    friend constexpr void swap(indirect& lhs, indirect& rhs) noexcept(noexcept(lhs.swap(rhs))) { lhs.swap(rhs); }

    /**
     * Mandates: The expression *lhs == *rhs is well-formed and its result is convertible to bool.
     *
     * Returns: If lhs is valueless or rhs is valueless,
     * lhs.valueless_after_move() == rhs.valueless_after_move(); otherwise *lhs == *rhs.
     */
    template <class U, class AA>
    friend constexpr bool operator==(const indirect&        lhs,
                                     const indirect<U, AA>& rhs) noexcept(noexcept(*lhs == *rhs)) {
        if (lhs.valueless_after_move() || rhs.valueless_after_move())
            return lhs.valueless_after_move() == rhs.valueless_after_move();
        return *lhs == *rhs;
    }

    /**
     * Mandates: The expression *lhs == rhs is well-formed and its result is convertible to bool.
     *
     * Returns: If lhs is valueless, false; otherwise *lhs == rhs.
     */
    template <class U>
    friend constexpr bool operator==(const indirect& lhs, const U& rhs) noexcept(noexcept(*lhs == rhs)) {
        if (lhs.valueless_after_move())
            return false;
        return *lhs == rhs;
    }

    /**
     * Returns: If lhs is valueless or rhs is valueless,
     * !lhs.valueless_after_move() <=> !rhs.valueless_after_move();
     * otherwise synth-three-way(*lhs, *rhs).
     */
    template <class U, class AA>
    friend constexpr auto operator<=>(const indirect&        lhs,
                                      const indirect<U, AA>& rhs) /* ->synth-three-way-result<T, U> */;

    /**
     * Returns: If lhs is valueless, strong_ordering::less; otherwise synth-three-way(*lhs, rhs).
     */
    template <class U>
    friend constexpr auto operator<=>(const indirect& lhs, const U& rhs) /* ->synth-three-way-result<T, U> */;

  private:
    // Shorthand functions to allocator_traits:

    // Allocate a pointer to a storage using this->alloc
    [[nodiscard]] inline constexpr pointer allocate_ptr() {
        return std::allocator_traits<Allocator>::allocate(this->alloc, 1);
    }

    // Construct object at this->p using this->alloc as allocator and args as arguments
    template <class... Args>
    inline constexpr void construct(Args&&... args) {
        std::allocator_traits<Allocator>::construct(this->alloc, this->p, std::forward<Args>(args)...);
    }

    // Destories this->p using this->alloc
    inline constexpr void destroy() { std::allocator_traits<Allocator>::destroy(this->alloc, this->p); }
    // Deallocates this->p using this->alloc
    inline constexpr void deallocate() { std::allocator_traits<Allocator>::deallocate(this->alloc, this->p, 1); }

    // Exception safe utils and destroy+deallocate/ allocate+construct helpers:

    // This is equivalent to a scope_exit
    // Used as a "finally" block in java equivalent to enforce execution on exception
    template <typename lambda>
    struct finally {
        constexpr explicit finally(lambda&& f_) : f(std::forward<lambda>(f_)) {}
        constexpr ~finally() { invoke(); }
        constexpr void invoke() {
            if (!released)
                f();
            released = true;
        }

      private:
        lambda f;
        bool   released = false;
    };

    // Disengage ownership if exist, same postcondition as the unchecked counterpart
    constexpr void checked_destroy_and_deallocate() {
        if (this->p != nullptr)
            unchecked_destroy_and_deallocate();
    }

    // Disengage ownership
    //
    // precondition: is not in valueless state
    // postcondition: owned object is deallocated
    //
    // throws when contained object's destritor or deallocate throws
    // postcondition is guaranteed even when destroy throws
    constexpr void unchecked_destroy_and_deallocate() {
        // must deallocate whatever when we exit this function
        finally must_deallocate([this]() { this->deallocate(); });
        // destroy may throw
        destroy();
        must_deallocate.invoke();
    }

    // Return a pointer to a new object constructed using this->alloc Allocator and args...
    //
    // Note: pointer this->p is not updated.
    // Exception guarantee: no memory leak if construct throws
    template <class... Args>
    [[nodiscard]] constexpr pointer allocate_and_construct(Args&&... args) {
        auto target = std::allocator_traits<Allocator>::allocate(this->alloc, 1);
        try {
            std::allocator_traits<Allocator>::construct(this->alloc, target, std::forward<Args>(args)...);
        } catch (...) {
            std::allocator_traits<Allocator>::deallocate(this->alloc, target, 1);
            throw;
        }
        return target;
    }

    Allocator alloc;
    pointer   p;
};

template <class Value>
indirect(Value) -> indirect<Value>;

template <class Allocator, class Value>
indirect(std::allocator_arg_t, Allocator, Value)
    -> indirect<Value, typename std::allocator_traits<Allocator>::template rebind_alloc<Value>>;

} // namespace beman::indirect

#endif // BEMAN_INDIRECT_IDENTITY_HPP
