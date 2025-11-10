// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef BEMAN_INDIRECT_IDENTITY_HPP
#define BEMAN_INDIRECT_IDENTITY_HPP

#include <initializer_list>
#include <memory>
#include <utility>

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
    explicit constexpr indirect() : indirect(std::allocator_arg, Allocator{}) {}

    /**
     * Mandates: is_default_constructible_v<T> is true.
     *
     * Effects: alloc is direct-non-list-initialized with a.
     * Constructs an owned object of type T with an empty argument list, using the allocator alloc.
     */
    explicit constexpr indirect(std::allocator_arg_t, const Allocator& a) {
        this->alloc = a;
        this->p     = this->alloc.allocate(1);
        new (this->p) value_type{};
    }

    /**
     * Mandates: is_copy_constructible_v<T> is true.
     *
     * Effects: alloc is direct-non-list-initialized with
     * allocator_traits<Allocator>::select_on_container_copy_construction(other.alloc).
     * If other is valueless, *this is valueless.
     * Otherwise, constructs an owned object of type T with *other, using the allocator alloc.
     */
    constexpr indirect(const indirect& other) : indirect(std::allocator_arg, Allocator{}, other) {}

    /**
     * Mandates: is_copy_constructible_v<T> is true.
     *
     * Effects: alloc is direct-non-list-initialized with a. If other is valueless, *this is valueless.
     * Otherwise, constructs an owned object of type T with *other, using the allocator alloc.
     */
    constexpr indirect(std::allocator_arg_t, const Allocator& a, const indirect& other) {
        this->alloc = a;
        if (other.p == nullptr) {
            this->p = nullptr;
            return;
        }

        this->p = this->alloc.allocate(1);
        new (this->p) value_type(*other.p);
    }

    /**
     * Effects: alloc is direct-non-list-initialized from std::move(other.alloc).
     * If other is valueless, *this is valueless.
     * Otherwise *this takes ownership of the owned object of other.
     *
     * Postconditions: other is valueless.
     */
    constexpr indirect(indirect&& other) noexcept {
        this->alloc = std::move(other.alloc);
        // takes ownership directly
        this->p     = other.p;
        // Mark other as valueless
        other.p = nullptr;
    }

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
                       indirect&&       other) noexcept(std::allocator_traits<Allocator>::is_always_equal::value) {
        this->alloc = a;
        // Taking over ownership
        if (this->alloc == other.alloc) {
            this->p = other.p;
            other.p = nullptr;
            return;
        }

        // other is valueless
        if (other.p == nullptr) {
            this->p = nullptr;
            return;
        }

        // Move constructing
        this->p = this->alloc.allocate(1);
        new (this->p) value_type(std::move(*other.p));
        other.alloc.deallocate(other.p, 1);
        other.p = nullptr;
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
    explicit constexpr indirect(U&& u) : indirect(std::allocator_arg, Allocator{}, std::forward(u)) {}

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
        : indirect(std::allocator_arg, Allocator{}, std::in_place, std::forward(u)) {}

    /**
     * Constraints:
     * • is_constructible_v<T, Us...> is true, and
     * • is_default_constructible_v<Allocator> is true.
     *
     * Effects: Constructs an owned object of type T with std::forward<Us>(us)..., using the allocator alloc.
     */
    template <class... Us>
    explicit constexpr indirect(std::in_place_t, Us&&... us)
        : indirect(std::allocator_arg, Allocator{}, std::in_place, std::forward(us)...) {}

    /**
     * Constraints: is_constructible_v<T, Us...> is true.
     *
     * Effects: alloc is direct-non-list-initialized with a.
     * Constructs an owned object of type T with std::forward<Us>(us)..., using the allocator alloc.
     */
    template <class... Us>
    explicit constexpr indirect(std::allocator_arg_t, const Allocator& a, std::in_place_t, Us&&... us) {
        this->alloc = a;
        this->p     = this->alloc.allocate(1);
        new (this->p) value_type(std::forward(us)...);
    }

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
        : indirect(std::allocator_arg, Allocator{}, std::in_place, std::move(ilist), std::forward(us)...) {}

    /**
     * Constraints: is_constructible_v<T, initializer_list<I>&, Us...> is true.
     *
     * Effects: alloc is direct-non-list-initialized with a. Constructs an owned object of type T with the arguments
     * ilist, std::forward<Us>(us)..., using the allocator alloc.
     */
    template <class I, class... Us>
    explicit constexpr indirect(
        std::allocator_arg_t, const Allocator& a, std::in_place_t, std::initializer_list<I> ilist, Us&&... us) {
        this->alloc = a;
        this->p     = this->alloc.allocate(1);
        new (this->p) value_type(std::move(ilist), std::forward(us)...);
    }

    /**
     * Mandates: T is a complete type.
     *
     * Effects: If *this is not valueless, destroys the owned object
     * using allocator_traits<Allocator>::destroy and then the storage is deallocated.
     */
    constexpr ~indirect() {
        if (this->p != nullptr)
            this->alloc.deallocate(this->p, 1);
    }

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
     * Remarks: If any exception is thrown, the result of the expression this->valueless_after_move() remains unchanged.
     * If an exception is thrown during the call to T's selected copy constructor, no effect.
     * If an exception is thrown during the call to T's copy assignment,
     * the state of its contained value is as defined by the exception safety guarantee of T's copy assignment.
     */
    constexpr indirect& operator=(const indirect& other);

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
        std::allocator_traits<Allocator>::is_always_equal::value);

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
    constexpr indirect& operator=(U&& u);

    /**
     * Preconditions: *this is not valueless.
     *
     * Returns: *p.
     */
    constexpr const T& operator*() const& noexcept;

    /**
     * Preconditions: *this is not valueless.
     *
     * Returns: *p.
     */
    constexpr T&        operator*() & noexcept;

    /**
     * Preconditions: *this is not valueless.
     *
     * Returns: std::move(*p).
     */
    constexpr const T&& operator*() const&& noexcept;

    /**
     * Preconditions: *this is not valueless.
     *
     * Returns: std::move(*p).
     */
    constexpr T&&       operator*() && noexcept;

    /**
     * Preconditions: *this is not valueless.
     *
     * Returns: p.
     */
    constexpr const_pointer operator->() const noexcept;

    /**
     * Preconditions: *this is not valueless.
     *
     * Returns: p.
     */
    constexpr pointer       operator->() noexcept;

    /**
     * Returns: true if *this is valueless, otherwise false.
     */
    constexpr bool valueless_after_move() const noexcept;

    /**
     * Returns: alloc.
     */
    constexpr allocator_type get_allocator() const noexcept;

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
    constexpr void swap(indirect& other) noexcept(std::allocator_traits<Allocator>::propagate_on_container_swap::value ||
                                                   std::allocator_traits<Allocator>::is_always_equal::value);

    /**
     * Effects: Equivalent to lhs.swap(rhs).
     */
    friend constexpr void swap(indirect& lhs, indirect& rhs) noexcept(noexcept(lhs.swap(rhs)));

    /**
     * Mandates: The expression *lhs == *rhs is well-formed and its result is convertible to bool.
     *
     * Returns: If lhs is valueless or rhs is valueless,
     * lhs.valueless_after_move() == rhs.valueless_after_move(); otherwise *lhs == *rhs.
     */
    template <class U, class AA>
    friend constexpr bool operator==(const indirect& lhs, const indirect<U, AA>& rhs) noexcept(noexcept(*lhs == *rhs));

    /**
     * Mandates: The expression *lhs == rhs is well-formed and its result is convertible to bool.
     *
     * Returns: If lhs is valueless, false; otherwise *lhs == rhs.
     */
    template <class U>
    friend constexpr bool operator==(const indirect& lhs, const U& rhs) noexcept(noexcept(*lhs == rhs));

    /**
     * Returns: If lhs is valueless or rhs is valueless,
     * !lhs.valueless_after_move() <=> !rhs.valueless_after_move();
     * otherwise synth-three-way(*lhs, *rhs).
     */
    template <class U, class AA>
    friend constexpr auto operator<=>(const indirect& lhs, const indirect<U, AA>& rhs) /* ->synth-three-way-result<T, U> */;

    /**
     * Returns: If lhs is valueless, strong_ordering::less; otherwise synth-three-way(*lhs, rhs).
     */
    template <class U>
    friend constexpr auto operator<=>(const indirect& lhs, const U& rhs) /* ->synth-three-way-result<T, U> */;

  private:
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
