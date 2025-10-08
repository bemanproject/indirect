// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef BEMAN_INDIRECT_IDENTITY_HPP
#define BEMAN_INDIRECT_IDENTITY_HPP

#include <memory>

namespace beman::indirect {

template <class T, class Allocator = std::allocator<T>>
class indirect {
  public:
    using value_type     = T;
    using allocator_type = Allocator;
    using pointer        = typename std::allocator_traits<Allocator>::pointer;
    using const_pointer  = typename std::allocator_traits<Allocator>::const_pointer;

    constexpr indirect();

    explicit constexpr indirect(std::allocator_arg_t, const Allocator& alloc);

    template <class U, class... Us>
    explicit constexpr indirect(U&& u, Us&&... us);

    template <class U, class... Us>
    explicit constexpr indirect(std::allocator_arg_t, const Allocator& alloc, U&& u, Us&&... us);

    constexpr indirect(const indirect& other);

    constexpr indirect(std::allocator_arg_t, const Allocator& alloc, const indirect& other);

    constexpr indirect(indirect&& other) /* noexcept(see below) */;

    constexpr indirect(std::allocator_arg_t, const Allocator& alloc, indirect&& other) /* noexcept(see below) */;

    constexpr ~indirect();

    constexpr indirect& operator=(const indirect& other);

    constexpr indirect& operator=(indirect&& other) /* noexcept(see below) */;

    constexpr const T& operator*() const& noexcept;

    constexpr T& operator*() & noexcept;

    constexpr const T&& operator*() const&& noexcept;

    constexpr T&& operator*() && noexcept;

    constexpr const_pointer operator->() const noexcept;

    constexpr pointer operator->() noexcept;

    constexpr bool valueless_after_move() const noexcept;

    constexpr allocator_type get_allocator() const noexcept;

    constexpr void swap(indirect& other) /* noexcept(see below) */;

    friend constexpr void swap(indirect& lhs, indirect& rhs) /* noexcept(see below) */;

    template <class U, class AA>
    friend constexpr auto operator==(const indirect& lhs, const indirect<U, AA>& rhs) /* noexcept(see below) */;

    template <class U, class AA>
    friend constexpr auto operator!=(const indirect& lhs, const indirect<U, AA>& rhs) /* noexcept(see below) */;

    template <class U, class AA>
    friend constexpr auto operator<(const indirect& lhs, const indirect<U, AA>& rhs) /* noexcept(see below) */;

    template <class U, class AA>
    friend constexpr auto operator<=(const indirect& lhs, const indirect<U, AA>& rhs) /* noexcept(see below) */;

    template <class U, class AA>
    friend constexpr auto operator>(const indirect& lhs, const indirect<U, AA>& rhs) /* noexcept(see below) */;

    template <class U, class AA>
    friend constexpr auto operator>=(const indirect& lhs, const indirect<U, AA>& rhs) /* noexcept(see below) */;

    template <class U, class AA>
    friend constexpr auto operator<=>(const indirect& lhs, const indirect<U, AA>& rhs) /* noexcept(see below) */;

    template <class U>
    friend constexpr auto operator==(const indirect& lhs, const U& rhs) /* noexcept(see below) */;

    template <class U>
    friend constexpr auto operator==(const U& lhs, const indirect& rhs) /* noexcept(see below) */;

    template <class U>
    friend constexpr auto operator!=(const indirect& lhs, const U& rhs) /* noexcept(see below) */;

    template <class U>
    friend constexpr auto operator!=(const U& lhs, const indirect& rhs) /* noexcept(see below) */;

    template <class U>
    friend constexpr auto operator<(const indirect& lhs, const U& rhs) /* noexcept(see below) */;

    template <class U>
    friend constexpr auto operator<(const U& lhs, const indirect& rhs) /* noexcept(see below) */;

    template <class U>
    friend constexpr auto operator<=(const indirect& lhs, const U& rhs) /* noexcept(see below) */;

    template <class U>
    friend constexpr auto operator<=(const U& lhs, const indirect& rhs) /* noexcept(see below) */;

    template <class U>
    friend constexpr auto operator>(const indirect& lhs, const U& rhs) /* noexcept(see below) */;

    template <class U>
    friend constexpr auto operator>(const U& lhs, const indirect& rhs) /* noexcept(see below) */;

    template <class U>
    friend constexpr auto operator>=(const indirect& lhs, const U& rhs) /* noexcept(see below) */;

    template <class U>
    friend constexpr auto operator>=(const U& lhs, const indirect& rhs) /* noexcept(see below) */;

    template <class U>
    friend constexpr auto operator<=>(const indirect& lhs, const U& rhs) /* noexcept(see below) */;

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
