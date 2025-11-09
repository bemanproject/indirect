// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <beman/indirect/indirect.hpp>

#include <cstddef>
#include <functional>
#include <gtest/gtest.h>
#include <memory>

template <typename T>
struct CountingAllocator {
    using value_type      = T;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;
    using Id_type         = int;

    constexpr CountingAllocator() = default;
    constexpr explicit CountingAllocator(Id_type _id) : id(_id) {}
    constexpr ~CountingAllocator() = default;

    constexpr T* allocate(std::size_t n) {
        ++num_allocated;
        return _backing.allocate(n);
    }

    void deallocate(T* p, std::size_t n) {
        ++num_deallocated;
        _backing.deallocate(p, n);
    }

    std::allocator<T> _backing{};

    size_type num_allocated   = 0;
    size_type num_deallocated = 0;
    Id_type   id              = -1;
};

// making sure the allocators are consteval-able
static_assert(std::invoke([]() {
    CountingAllocator<int> _default;
    (void)_default;
    return true;
}));

#define ASSERT_NO_LEAKS(alloc) EXPECT_EQ(alloc.num_allocated, alloc.num_deallocated)

template <typename T>
using indirect = beman::indirect::indirect<T, CountingAllocator<T>>;

// TODO: Move these to a header file and add factory methods so they can be in another TU to test for incomplete type.
struct Composite {
    int a, b, c;

    Composite() : a(0), b(0), c(0) {}
    Composite(int _a, int _b, int _c) : a(_a), b(_b), c(_c) {}

    bool operator==(const Composite& other) const { return a == other.a && b == other.b && c == other.c; }
};

struct SimpleType {
    int value;
    explicit SimpleType(int v) : value(v) {}
    bool operator==(const SimpleType& other) const { return value == other.value; }
};

struct MoveOnlyType {
    int value;
    explicit MoveOnlyType(int v) : value(v) {}
    MoveOnlyType(const MoveOnlyType&) = delete;
    MoveOnlyType(MoveOnlyType&& other) noexcept : value(other.value) { other.value = -1; }
    MoveOnlyType& operator=(const MoveOnlyType&) = delete;
    MoveOnlyType& operator=(MoveOnlyType&&)      = delete;
    bool          operator==(const MoveOnlyType& other) const { return value == other.value; }
};

struct DefaultConstructible {
    int  value = 55;
    bool operator==(const DefaultConstructible& other) const { return value == other.value; }
};

struct VectorWrapper {
    std::vector<int> data;
    explicit VectorWrapper(std::initializer_list<int> ilist) : data(ilist) {}
    bool operator==(const VectorWrapper& other) const { return data == other.data; }
};

struct VectorWithInt {
    std::vector<int> data;
    int              multiplier;
    explicit VectorWithInt(std::initializer_list<int> ilist, int mult) : data(ilist), multiplier(mult) {}
    bool operator==(const VectorWithInt& other) const { return data == other.data && multiplier == other.multiplier; }
};

// ========================================
// Incomplete Type Tests
// ========================================

TEST(IncompleteTests, CanHoldIncompleteType) {
    // Passes if it compiles, this is statuary

    struct Incomplete {
        using Self = Incomplete;

        indirect<Self> ind;
        int            _ignore;
    };
}

// ========================================
// Default Constructor Tests
// ========================================

/**
 * explicit constexpr indirect();
 *
 * Constraints: is_default_constructible_v<Allocator> is true.
 *
 * Mandates: is_default_constructible_v<T> is true.
 *
 * Effects: Constructs an owned object of type T with an empty argument list, using the allocator alloc.
 */

TEST(IndirectTest, DefaultConstructor) {
    using T = DefaultConstructible;
    indirect<T> instance;

    EXPECT_FALSE(instance.valueless_after_move());
}

/**
 * explicit constexpr indirect(std::allocator_arg_t, const Allocator& a);
 *
 * Mandates: is_default_constructible_v<T> is true.
 *
 * Effects: alloc is direct-non-list-initialized with a.
 * Constructs an owned object of type T with an empty argument list, using the allocator alloc.
 */

TEST(IndirectTest, DefaultConstructorWithAllocator) {
    using T = DefaultConstructible;
    CountingAllocator<T> alloc;

    {
        indirect<T> instance(std::allocator_arg, alloc);

        EXPECT_FALSE(instance.valueless_after_move());
    }

    ASSERT_NO_LEAKS(alloc);
    EXPECT_EQ(alloc.num_allocated, 1);
}

// ========================================
// in_place Constructor Tests
// ========================================

/**
 * template <class... Us>
 * explicit constexpr indirect(std::in_place_t, Us&&... us);
 *
 * Constraints:
 * • is_constructible_v<T, Us...> is true, and
 * • is_default_constructible_v<Allocator> is true.
 *
 * Effects: Constructs an owned object of type T with std::forward<Us>(us)..., using the allocator alloc.
 */

TEST(IndirectTest, InPlaceConstructorBasic) {
    using T = Composite;
    indirect<T> instance(std::in_place, 1, 2, 3);

    EXPECT_EQ(*instance, T(1, 2, 3));
}

/**
 * template <class... Us>
 * explicit constexpr indirect(std::allocator_arg_t, const Allocator& a, std::in_place_t, Us&&... us);
 *
 * Constraints: is_constructible_v<T, Us...> is true.
 *
 * Effects: alloc is direct-non-list-initialized with a.
 * Constructs an owned object of type T with std::forward<Us>(us)..., using the allocator alloc.
 */

TEST(IndirectTest, InPlaceConstructorBasicWithAllocator) {
    using T = Composite;
    CountingAllocator<T> alloc;

    {
        indirect<T> instance(std::allocator_arg, alloc, std::in_place, 1, 2, 3);

        EXPECT_EQ(*instance, T(1, 2, 3));
    }

    ASSERT_NO_LEAKS(alloc);
    EXPECT_EQ(alloc.num_allocated, 1);
}

/**
 * template <class... Us>
 * explicit constexpr indirect(std::in_place_t, Us&&... us);
 *
 * Constraints:
 * • is_constructible_v<T, Us...> is true, and
 * • is_default_constructible_v<Allocator> is true.
 *
 * Effects: Constructs an owned object of type T with std::forward<Us>(us)..., using the allocator alloc.
 */

TEST(IndirectTest, InPlaceConstructorNoArgs) {
    using T = DefaultConstructible;
    indirect<T> instance(std::in_place);

    EXPECT_EQ(*instance, T());
}

/**
 * template <class... Us>
 * explicit constexpr indirect(std::in_place_t, Us&&... us);
 *
 * Constraints:
 * • is_constructible_v<T, Us...> is true, and
 * • is_default_constructible_v<Allocator> is true.
 *
 * Effects: Constructs an owned object of type T with std::forward<Us>(us)..., using the allocator alloc.
 */

TEST(IndirectTest, InPlaceConstructorWithArgs) {
    using T = Composite;
    indirect<T> instance(std::in_place, 5, 10, 15);

    EXPECT_EQ(*instance, T(5, 10, 15));
}

/**
 * template <class... Us>
 * explicit constexpr indirect(std::allocator_arg_t, const Allocator& a, std::in_place_t, Us&&... us);
 *
 * Constraints: is_constructible_v<T, Us...> is true.
 *
 * Effects: alloc is direct-non-list-initialized with a.
 * Constructs an owned object of type T with std::forward<Us>(us)..., using the allocator alloc.
 */

TEST(IndirectTest, InPlaceConstructorWithAllocator) {
    using T = Composite;
    CountingAllocator<T> alloc;

    {
        indirect<T> instance(std::allocator_arg, alloc, std::in_place, 7, 8, 9);

        EXPECT_EQ(*instance, T(7, 8, 9));
    }

    ASSERT_NO_LEAKS(alloc);
}

/**
 * template <class... Us>
 * explicit constexpr indirect(std::allocator_arg_t, const Allocator& a, std::in_place_t, Us&&... us);
 *
 * Constraints: is_constructible_v<T, Us...> is true.
 *
 * Effects: alloc is direct-non-list-initialized with a.
 * Constructs an owned object of type T with std::forward<Us>(us)..., using the allocator alloc.
 */

TEST(IncompleteTests, InPlaceConstructorWithIncompleteType) {
    struct T;
    CountingAllocator<T> alloc;

    struct T {
        int         a, b, c;
        indirect<T> ind{std::allocator_arg, alloc, std::in_place, 1, 2, 3};
    };

    indirect<T> instance;

    ASSERT_NO_LEAKS(alloc);
    EXPECT_EQ(alloc.num_allocated, 1);
}

// ========================================
// Copy Constructor Tests
// ========================================

/**
 * constexpr indirect(const indirect& other);
 *
 * Mandates: is_copy_constructible_v<T> is true.
 *
 * Effects: alloc is direct-non-list-initialized with
 * allocator_traits<Allocator>::select_on_container_copy_construction(other.alloc).
 * If other is valueless, *this is valueless.
 * Otherwise, constructs an owned object of type T with *other, using the allocator alloc.
 */

TEST(IndirectTest, CopyConstructor) {
    using T = Composite;
    indirect<T> original(std::in_place, 10, 20, 30);

    indirect<T> copy(original);

    EXPECT_EQ(*copy, T(10, 20, 30));
    EXPECT_EQ(*original, T(10, 20, 30));

    // Verify copy is independent - modify copy and check original unchanged
    copy->a = 999;
    EXPECT_EQ(*copy, T(999, 20, 30));
    EXPECT_EQ(*original, T(10, 20, 30));
}

/**
 * constexpr indirect(std::allocator_arg_t, const Allocator& a, const indirect& other);
 *
 * Mandates: is_copy_constructible_v<T> is true.
 *
 * Effects: alloc is direct-non-list-initialized with a. If other is valueless, *this is valueless.
 * Otherwise, constructs an owned object of type T with *other, using the allocator alloc.
 */

TEST(IndirectTest, CopyConstructorWithAllocator) {
    using T = Composite;
    CountingAllocator<T> alloc1(100);
    CountingAllocator<T> alloc2(200);

    {
        indirect<T> original(std::allocator_arg, alloc1, std::in_place, 10, 20, 30);

        indirect<T> copy(std::allocator_arg, alloc2, original);

        EXPECT_EQ(*copy, T(10, 20, 30));

        // Verify copy is independent
        copy->b = 888;
        EXPECT_EQ(*copy, T(10, 888, 30));
        EXPECT_EQ(*original, T(10, 20, 30));
    }

    ASSERT_NO_LEAKS(alloc1);
    ASSERT_NO_LEAKS(alloc2);
}

// ========================================
// Move Constructor Tests
// ========================================

/**
 * constexpr indirect(indirect&& other) noexcept;
 *
 * Effects: alloc is direct-non-list-initialized from std::move(other.alloc).
 * If other is valueless, *this is valueless.
 * Otherwise *this takes ownership of the owned object of other.
 *
 * Postconditions: other is valueless.
 */

TEST(IndirectTest, MoveConstructor) {
    using T = Composite;
    indirect<T> original(std::in_place, 10, 20, 30);

    EXPECT_FALSE(original.valueless_after_move());

    indirect<T> moved(std::move(original));

    EXPECT_EQ(*moved, T(10, 20, 30));
    EXPECT_TRUE(original.valueless_after_move());
}

/**
 * constexpr indirect(std::allocator_arg_t,
 *                    const Allocator& a,
 *                    indirect&&       other) noexcept(std::allocator_traits<Allocator>::is_always_equal::value);
 *
 * Mandates: If allocator_traits<Allocator>::is_always_equal::value is false then T is a complete type.
 *
 * Effects: alloc is direct-non-list-initialized with a. If other is valueless, *this is valueless.
 * Otherwise, if alloc == other.alloc is true,
 * constructs an object of type indirect that takes ownership of the owned object of other.
 * Otherwise, constructs an owned object of type T with *std::move(other), using the allocator alloc.
 *
 * Postconditions: other is valueless.
 */

TEST(IndirectTest, MoveConstructorWithAllocatorSameAllocator) {
    using T = Composite;
    CountingAllocator<T> alloc;

    {
        indirect<T> original(std::allocator_arg, alloc, std::in_place, 10, 20, 30);
        EXPECT_FALSE(original.valueless_after_move());

        indirect<T> moved(std::allocator_arg, alloc, std::move(original));

        EXPECT_EQ(*moved, T(10, 20, 30));
        EXPECT_TRUE(original.valueless_after_move());
    }

    ASSERT_NO_LEAKS(alloc);
}

// ========================================
// Forwarding Constructor Tests (U&&)
// ========================================

/**
 * template <class U = T>
 * explicit constexpr indirect(U&& u);
 *
 * Constraints:
 * • is_same_v<remove_cvref_t<U>, indirect> is false,
 * • is_same_v<remove_cvref_t<U>, in_place_t> is false,
 * • is_constructible_v<T, U> is true, and
 * • is_default_constructible_v<Allocator> is true.
 *
 * Effects: Constructs an owned object of type T with std::forward<U>(u), using the allocator alloc.
 */

TEST(IndirectTest, ForwardingConstructorFromLValue) {
    using T = SimpleType;
    T           value(42);
    indirect<T> instance(value);

    EXPECT_EQ(*instance, T(42));
    EXPECT_EQ(value, T(42)); // Original should be unchanged
}

/**
 * template <class U = T>
 * explicit constexpr indirect(U&& u);
 *
 * Constraints:
 * • is_same_v<remove_cvref_t<U>, indirect> is false,
 * • is_same_v<remove_cvref_t<U>, in_place_t> is false,
 * • is_constructible_v<T, U> is true, and
 * • is_default_constructible_v<Allocator> is true.
 *
 * Effects: Constructs an owned object of type T with std::forward<U>(u), using the allocator alloc.
 */

TEST(IndirectTest, ForwardingConstructorFromRValue) {
    using T = SimpleType;
    indirect<T> instance(T(42));

    EXPECT_EQ(*instance, T(42));
}

/**
 * template <class U = T>
 * explicit constexpr indirect(std::allocator_arg_t, const Allocator& a, U&& u);
 *
 * Constraints:
 * • is_same_v<remove_cvref_t<U>, indirect> is false,
 * • is_same_v<remove_cvref_t<U>, in_place_t> is false, and
 * • is_constructible_v<T, U> is true.
 *
 * Effects: alloc is direct-non-list-initialized with a.
 * Constructs an owned object of type T with std::forward<U>(u), using the allocator alloc.
 */

TEST(IndirectTest, ForwardingConstructorFromLValueWithAllocator) {
    using T = SimpleType;
    CountingAllocator<T> alloc;

    {
        T           value(42);
        indirect<T> instance(std::allocator_arg, alloc, value);

        EXPECT_EQ(*instance, T(42));
        EXPECT_EQ(value, T(42)); // Original should be unchanged
    }

    ASSERT_NO_LEAKS(alloc);
}

/**
 * template <class U = T>
 * explicit constexpr indirect(U&& u);
 *
 * Constraints:
 * • is_same_v<remove_cvref_t<U>, indirect> is false,
 * • is_same_v<remove_cvref_t<U>, in_place_t> is false,
 * • is_constructible_v<T, U> is true, and
 * • is_default_constructible_v<Allocator> is true.
 *
 * Effects: Constructs an owned object of type T with std::forward<U>(u), using the allocator alloc.
 */

TEST(IndirectTest, ForwardingConstructorFromMoveOnlyType) {
    using T = MoveOnlyType;
    indirect<T> instance(T(99));

    EXPECT_EQ(*instance, T(99));
}

// ========================================
// initializer_list Constructor Tests
// ========================================

/**
 * template <class I, class... Us>
 * explicit constexpr indirect(std::in_place_t, std::initializer_list<I> ilist, Us&&... us);
 *
 * Constraints:
 * • is_constructible_v<T, initializer_list<I>&, Us...> is true, and
 * • is_default_constructible_v<Allocator> is true.
 *
 * Effects: Constructs an owned object of type T with the arguments ilist, std::forward<Us>(us)...,
 * using the allocator alloc.
 */

TEST(IndirectTest, InitializerListConstructor) {
    using T = VectorWrapper;
    indirect<T> instance(std::in_place, {1, 2, 3, 4, 5});

    EXPECT_EQ(*instance, T({1, 2, 3, 4, 5}));
}

/**
 * template <class I, class... Us>
 * explicit constexpr indirect(std::in_place_t, std::initializer_list<I> ilist, Us&&... us);
 *
 * Constraints:
 * • is_constructible_v<T, initializer_list<I>&, Us...> is true, and
 * • is_default_constructible_v<Allocator> is true.
 *
 * Effects: Constructs an owned object of type T with the arguments ilist, std::forward<Us>(us)...,
 * using the allocator alloc.
 */

TEST(IndirectTest, InitializerListConstructorWithArgs) {
    using T = VectorWithInt;
    indirect<T> instance(std::in_place, {10, 20, 30}, 2);

    EXPECT_EQ(*instance, T({10, 20, 30}, 2));
}

/**
 * template <class I, class... Us>
 * explicit constexpr indirect(
 *     std::allocator_arg_t, const Allocator& a, std::in_place_t, std::initializer_list<I> ilist, Us&&... us);
 *
 * Constraints: is_constructible_v<T, initializer_list<I>&, Us...> is true.
 *
 * Effects: alloc is direct-non-list-initialized with a. Constructs an owned object of type T with the arguments
 * ilist, std::forward<Us>(us)..., using the allocator alloc.
 */

TEST(IndirectTest, InitializerListConstructorWithAllocator) {
    using T = VectorWrapper;
    CountingAllocator<T> alloc;

    {
        indirect<T> instance(std::allocator_arg, alloc, std::in_place, {7, 8, 9});

        EXPECT_EQ(*instance, T({7, 8, 9}));
    }

    ASSERT_NO_LEAKS(alloc);
}

/**
 * template <class I, class... Us>
 * explicit constexpr indirect(
 *     std::allocator_arg_t, const Allocator& a, std::in_place_t, std::initializer_list<I> ilist, Us&&... us);
 *
 * Constraints: is_constructible_v<T, initializer_list<I>&, Us...> is true.
 *
 * Effects: alloc is direct-non-list-initialized with a. Constructs an owned object of type T with the arguments
 * ilist, std::forward<Us>(us)..., using the allocator alloc.
 */

TEST(IndirectTest, InitializerListConstructorWithAllocatorAndArgs) {
    using T = VectorWithInt;
    CountingAllocator<T> alloc;

    {
        indirect<T> instance(std::allocator_arg, alloc, std::in_place, {100, 200}, 5);

        EXPECT_EQ(*instance, T({100, 200}, 5));
    }

    ASSERT_NO_LEAKS(alloc);
}
