// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef BEMAN_INDIRECT_IDENTITY_HPP
#define BEMAN_INDIRECT_IDENTITY_HPP

#include <memory>
#include <type_traits>

namespace beman::indirect {

/// @brief An allocator-aware value-semantic wrapper for dynamically allocated objects.
///
/// The indirect class template manages a dynamically allocated object with value semantics.
/// It supports incomplete types and provides const-correctness and allocator awareness.
/// Objects of type indirect own the dynamically allocated object they manage.
///
/// @tparam T The value type. May be an incomplete type.
/// @tparam Allocator The allocator type used for memory management.
template <class T, class Allocator = std::allocator<T>>
class indirect {
  public:
    using value_type     = T;
    using allocator_type = Allocator;
    using pointer        = typename std::allocator_traits<Allocator>::pointer;
    using const_pointer  = typename std::allocator_traits<Allocator>::const_pointer;

    /// @brief Default constructor.
    ///
    /// Mandates: `is_default_constructible_v<T>` is true.
    ///
    /// Effects: Constructs an `indirect` owning a uses-allocator constructed `T` and stores
    /// the address in `p_`. `allocator_` is default constructed.
    ///
    /// @post `*this` is not valueless.
    ///
    /// @throws Nothing unless allocator allocation or construction throws.
    constexpr indirect()
        requires(std::is_default_constructible_v<T>);

    /// @brief Allocator-aware default constructor.
    ///
    /// Mandates: `is_default_constructible_v<T>` is true.
    ///
    /// Effects: Constructs an `indirect` owning a uses-allocator constructed `T` and stores
    /// the address in `p_`. `allocator_` is constructed from `alloc`.
    ///
    /// @post `*this` is not valueless.
    ///
    /// @throws Nothing unless allocator allocation or construction throws.
    explicit constexpr indirect(std::allocator_arg_t, const Allocator& alloc)
        requires(std::is_default_constructible_v<T>);

    /// @brief Forwarding constructor.
    ///
    /// Mandates: `is_constructible_v<T, U, Us...>` is true.
    ///
    /// Constraints: This constructor does not participate in overload resolution when `U` is
    /// `indirect` or allocator-related types.
    ///
    /// Effects: Constructs an `indirect` owning a uses-allocator constructed `T` from
    /// `std::forward<U>(u), std::forward<Us>(us)...` and stores the address in `p_`.
    /// `allocator_` is default constructed.
    ///
    /// @post `*this` is not valueless.
    ///
    /// @throws Any exception thrown by the constructor of `T` or allocator operations.
    template <class U, class... Us>
    explicit constexpr indirect(U&& u, Us&&... us)
        requires(std::is_constructible_v<T, U, Us...>);

    /// @brief Allocator-aware forwarding constructor.
    ///
    /// Mandates: `is_constructible_v<T, U, Us...>` is true.
    ///
    /// Effects: Constructs an `indirect` owning a uses-allocator constructed `T` from
    /// `std::forward<U>(u), std::forward<Us>(us)...` and stores the address in `p_`.
    /// `allocator_` is constructed from `alloc`.
    ///
    /// @post `*this` is not valueless.
    ///
    /// @throws Any exception thrown by the constructor of `T` or allocator operations.
    template <class U, class... Us>
    explicit constexpr indirect(std::allocator_arg_t, const Allocator& alloc, U&& u, Us&&... us)
        requires(std::is_constructible_v<T, U, Us...>);

    /// @brief Copy constructor.
    ///
    /// Mandates: `is_copy_constructible_v<T>` is true.
    ///
    /// Effects: Constructs an `indirect` owning a copy of the object owned by `other`.
    /// The allocator is obtained by calling
    /// `allocator_traits<allocator_type>::select_on_container_copy_construction(other.get_allocator())`.
    ///
    /// @pre `other` is not valueless.
    ///
    /// @post `*this` is not valueless.
    ///
    /// @throws Any exception thrown by the copy constructor of `T` or allocator operations.
    constexpr indirect(const indirect& other)
        requires(std::is_copy_constructible_v<T>);

    /// @brief Allocator-aware copy constructor.
    ///
    /// Mandates: `is_copy_constructible_v<T>` is true.
    ///
    /// Effects: Constructs an `indirect` owning a copy of the object owned by `other`,
    /// allocated using `alloc`.
    ///
    /// @pre `other` is not valueless.
    ///
    /// @post `*this` is not valueless.
    ///
    /// @throws Any exception thrown by the copy constructor of `T` or allocator operations.
    constexpr indirect(std::allocator_arg_t, const Allocator& alloc, const indirect& other)
        requires(std::is_copy_constructible_v<T>);

    /// @brief Move constructor.
    ///
    /// Effects: Takes ownership of `other`'s owned object and stores the address in `p_`.
    /// `allocator_` is initialized from `other.allocator_`.
    ///
    /// @pre `other` is not valueless.
    ///
    /// @post `other` is valueless.
    constexpr indirect(indirect&& other) /* noexcept(see below) */;

    /// @brief Allocator-aware move constructor.
    ///
    /// Effects: If `alloc == other.get_allocator()`, takes ownership of `other`'s owned object;
    /// otherwise, constructs a copy of the owned object using `alloc`.
    ///
    /// @pre `other` is not valueless.
    ///
    /// @post `*this` is not valueless. If allocators compare equal, `other` is valueless;
    /// otherwise `other` is unchanged.
    ///
    /// @throws Any exception thrown by the move or copy constructor of `T` or allocator operations.
    constexpr indirect(std::allocator_arg_t, const Allocator& alloc, indirect&& other) /* noexcept(see below) */;

    /// @brief Destructor.
    ///
    /// Effects: If not valueless, destroys the owned object using allocator traits and
    /// deallocates storage.
    constexpr ~indirect();

    /// @brief Copy assignment operator.
    ///
    /// Mandates: `is_copy_constructible_v<T>` is true.
    ///
    /// Effects: Assigns the value of `other` to `*this`, handling allocator propagation
    /// according to allocator traits.
    ///
    /// @pre `other` is not valueless.
    ///
    /// @post `*this` is not valueless.
    ///
    /// @returns `*this`.
    ///
    /// @throws Any exception thrown by the copy constructor of `T` or allocator operations.
    constexpr indirect& operator=(const indirect& other)
        requires(std::is_copy_constructible_v<T>);

    /// @brief Move assignment operator.
    ///
    /// Effects: Assigns the value of `other` to `*this`, handling allocator propagation
    /// according to allocator traits. If allocators are equal or propagate on move assignment,
    /// takes ownership of `other`'s object; otherwise copies it.
    ///
    /// @pre `other` is not valueless.
    ///
    /// @post `*this` is not valueless. `other` may be valueless depending on
    /// allocator equality and propagation traits.
    ///
    /// @returns `*this`.
    ///
    /// @throws Any exception thrown by move or copy operations of `T` or allocator operations.
    constexpr indirect& operator=(indirect&& other) /* noexcept(see below) */;

    /// @brief Dereference operator (const lvalue).
    ///
    /// @pre `*this` is not valueless.
    ///
    /// @returns A const lvalue reference to the owned object.
    constexpr const T& operator*() const& noexcept;

    /// @brief Dereference operator (lvalue).
    ///
    /// @pre `*this` is not valueless.
    ///
    /// @returns An lvalue reference to the owned object.
    constexpr T& operator*() & noexcept;

    /// @brief Dereference operator (const rvalue).
    ///
    /// @pre `*this` is not valueless.
    ///
    /// @returns A const rvalue reference to the owned object.
    constexpr const T&& operator*() const&& noexcept;

    /// @brief Dereference operator (rvalue).
    ///
    /// @pre `*this` is not valueless.
    ///
    /// @returns An rvalue reference to the owned object.
    constexpr T&& operator*() && noexcept;

    /// @brief Arrow operator (const).
    ///
    /// @pre `*this` is not valueless.
    ///
    /// @returns A pointer to the owned object.
    constexpr const_pointer operator->() const noexcept;

    /// @brief Arrow operator (non-const).
    ///
    /// @pre `*this` is not valueless.
    ///
    /// @returns A pointer to the owned object.
    constexpr pointer operator->() noexcept;

    /// @brief Check if the object is in a valueless state.
    ///
    /// @returns `true` if `*this` is valueless (has been moved from), `false` otherwise.
    constexpr bool valueless_after_move() const noexcept;

    /// @brief Get the allocator.
    ///
    /// @returns The allocator used by `*this`.
    constexpr allocator_type get_allocator() const noexcept;

    /// @brief Swap the contents with another indirect object.
    ///
    /// Effects: Exchanges the owned objects and allocators according to allocator traits.
    ///
    /// @pre Both `*this` and `other` are not valueless.
    constexpr void swap(indirect& other) /* noexcept(see below) */;

    /// @brief Non-member swap function.
    ///
    /// Effects: Equivalent to `lhs.swap(rhs)`.
    ///
    /// @pre Both `lhs` and `rhs` are not valueless.
    friend constexpr void swap(indirect& lhs, indirect& rhs) /* noexcept(see below) */;

    /// @brief Equality comparison between two indirect objects.
    ///
    /// @pre Neither operand is valueless.
    ///
    /// @returns `*lhs == *rhs`.
    template <class U, class AA>
    friend constexpr auto operator==(const indirect& lhs, const indirect<U, AA>& rhs) /* noexcept(see below) */;

    /// @brief Inequality comparison between two indirect objects.
    ///
    /// @pre Neither operand is valueless.
    ///
    /// @returns `*lhs != *rhs`.
    template <class U, class AA>
    friend constexpr auto operator!=(const indirect& lhs, const indirect<U, AA>& rhs) /* noexcept(see below) */;

    /// @brief Less-than comparison between two indirect objects.
    ///
    /// @pre Neither operand is valueless.
    ///
    /// @returns `*lhs < *rhs`.
    template <class U, class AA>
    friend constexpr auto operator<(const indirect& lhs, const indirect<U, AA>& rhs) /* noexcept(see below) */;

    /// @brief Less-than-or-equal comparison between two indirect objects.
    ///
    /// @pre Neither operand is valueless.
    ///
    /// @returns `*lhs <= *rhs`.
    template <class U, class AA>
    friend constexpr auto operator<=(const indirect& lhs, const indirect<U, AA>& rhs) /* noexcept(see below) */;

    /// @brief Greater-than comparison between two indirect objects.
    ///
    /// @pre Neither operand is valueless.
    ///
    /// @returns `*lhs > *rhs`.
    template <class U, class AA>
    friend constexpr auto operator>(const indirect& lhs, const indirect<U, AA>& rhs) /* noexcept(see below) */;

    /// @brief Greater-than-or-equal comparison between two indirect objects.
    ///
    /// @pre Neither operand is valueless.
    ///
    /// @returns `*lhs >= *rhs`.
    template <class U, class AA>
    friend constexpr auto operator>=(const indirect& lhs, const indirect<U, AA>& rhs) /* noexcept(see below) */;

    /// @brief Three-way comparison between two indirect objects.
    ///
    /// @pre Neither operand is valueless.
    ///
    /// @returns `*lhs <=> *rhs`.
    template <class U, class AA>
    friend constexpr auto operator<=>(const indirect& lhs, const indirect<U, AA>& rhs) /* noexcept(see below) */;

    /// @brief Equality comparison between indirect and value.
    ///
    /// @pre `lhs` is not valueless.
    ///
    /// @returns `*lhs == rhs`.
    template <class U>
    friend constexpr auto operator==(const indirect& lhs, const U& rhs) /* noexcept(see below) */;

    /// @brief Equality comparison between value and indirect.
    ///
    /// @pre `rhs` is not valueless.
    ///
    /// @returns `lhs == *rhs`.
    template <class U>
    friend constexpr auto operator==(const U& lhs, const indirect& rhs) /* noexcept(see below) */;

    /// @brief Inequality comparison between indirect and value.
    ///
    /// @pre `lhs` is not valueless.
    ///
    /// @returns `*lhs != rhs`.
    template <class U>
    friend constexpr auto operator!=(const indirect& lhs, const U& rhs) /* noexcept(see below) */;

    /// @brief Inequality comparison between value and indirect.
    ///
    /// @pre `rhs` is not valueless.
    ///
    /// @returns `lhs != *rhs`.
    template <class U>
    friend constexpr auto operator!=(const U& lhs, const indirect& rhs) /* noexcept(see below) */;

    /// @brief Less-than comparison between indirect and value.
    ///
    /// @pre `lhs` is not valueless.
    ///
    /// @returns `*lhs < rhs`.
    template <class U>
    friend constexpr auto operator<(const indirect& lhs, const U& rhs) /* noexcept(see below) */;

    /// @brief Less-than comparison between value and indirect.
    ///
    /// @pre `rhs` is not valueless.
    ///
    /// @returns `lhs < *rhs`.
    template <class U>
    friend constexpr auto operator<(const U& lhs, const indirect& rhs) /* noexcept(see below) */;

    /// @brief Less-than-or-equal comparison between indirect and value.
    ///
    /// @pre `lhs` is not valueless.
    ///
    /// @returns `*lhs <= rhs`.
    template <class U>
    friend constexpr auto operator<=(const indirect& lhs, const U& rhs) /* noexcept(see below) */;

    /// @brief Less-than-or-equal comparison between value and indirect.
    ///
    /// @pre `rhs` is not valueless.
    ///
    /// @returns `lhs <= *rhs`.
    template <class U>
    friend constexpr auto operator<=(const U& lhs, const indirect& rhs) /* noexcept(see below) */;

    /// @brief Greater-than comparison between indirect and value.
    ///
    /// @pre `lhs` is not valueless.
    ///
    /// @returns `*lhs > rhs`.
    template <class U>
    friend constexpr auto operator>(const indirect& lhs, const U& rhs) /* noexcept(see below) */;

    /// @brief Greater-than comparison between value and indirect.
    ///
    /// @pre `rhs` is not valueless.
    ///
    /// @returns `lhs > *rhs`.
    template <class U>
    friend constexpr auto operator>(const U& lhs, const indirect& rhs) /* noexcept(see below) */;

    /// @brief Greater-than-or-equal comparison between indirect and value.
    ///
    /// @pre `lhs` is not valueless.
    ///
    /// @returns `*lhs >= rhs`.
    template <class U>
    friend constexpr auto operator>=(const indirect& lhs, const U& rhs) /* noexcept(see below) */;

    /// @brief Greater-than-or-equal comparison between value and indirect.
    ///
    /// @pre `rhs` is not valueless.
    ///
    /// @returns `lhs >= *rhs`.
    template <class U>
    friend constexpr auto operator>=(const U& lhs, const indirect& rhs) /* noexcept(see below) */;

    /// @brief Three-way comparison between indirect and value.
    ///
    /// @pre `lhs` is not valueless.
    ///
    /// @returns `*lhs <=> rhs`.
    template <class U>
    friend constexpr auto operator<=>(const indirect& lhs, const U& rhs) /* noexcept(see below) */;

    /// @brief Three-way comparison between value and indirect.
    ///
    /// @pre `rhs` is not valueless.
    ///
    /// @returns `lhs <=> *rhs`.
    template <class U>
    friend constexpr auto operator<=>(const U& lhs, const indirect& rhs) /* noexcept(see below) */;

  private:
    pointer   p_;
    Allocator allocator_;
};

template <typename Value>
indirect(Value) -> indirect<Value>;

template <typename Alloc, typename Value>
indirect(std::allocator_arg_t, Alloc, Value)
    -> indirect<Value, typename std::allocator_traits<Alloc>::template rebind_alloc<Value>>;

} // namespace beman::indirect

#endif // BEMAN_INDIRECT_IDENTITY_HPP
