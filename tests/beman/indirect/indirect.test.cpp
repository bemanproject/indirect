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

TEST(IndirectTest, DefaultConstructor) {
    using T = DefaultConstructible;
    indirect<T> instance;

    EXPECT_FALSE(instance.valueless_after_move());
}

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

TEST(IndirectTest, InPlaceConstructorBasic) {
    using T = Composite;
    indirect<T> instance(std::in_place, 1, 2, 3);

    EXPECT_EQ(*instance, T(1, 2, 3));
}

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

TEST(IndirectTest, InPlaceConstructorNoArgs) {
    using T = DefaultConstructible;
    indirect<T> instance(std::in_place);

    EXPECT_EQ(*instance, T());
}

TEST(IndirectTest, InPlaceConstructorWithArgs) {
    using T = Composite;
    indirect<T> instance(std::in_place, 5, 10, 15);

    EXPECT_EQ(*instance, T(5, 10, 15));
}

TEST(IndirectTest, InPlaceConstructorWithAllocator) {
    using T = Composite;
    CountingAllocator<T> alloc;

    {
        indirect<T> instance(std::allocator_arg, alloc, std::in_place, 7, 8, 9);

        EXPECT_EQ(*instance, T(7, 8, 9));
    }

    ASSERT_NO_LEAKS(alloc);
}

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

TEST(IndirectTest, MoveConstructor) {
    using T = Composite;
    indirect<T> original(std::in_place, 10, 20, 30);

    EXPECT_FALSE(original.valueless_after_move());

    indirect<T> moved(std::move(original));

    EXPECT_EQ(*moved, T(10, 20, 30));
    EXPECT_TRUE(original.valueless_after_move());
}

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

TEST(IndirectTest, ForwardingConstructorFromLValue) {
    using T = SimpleType;
    T           value(42);
    indirect<T> instance(value);

    EXPECT_EQ(*instance, T(42));
    EXPECT_EQ(value, T(42)); // Original should be unchanged
}

TEST(IndirectTest, ForwardingConstructorFromRValue) {
    using T = SimpleType;
    indirect<T> instance(T(42));

    EXPECT_EQ(*instance, T(42));
}

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

TEST(IndirectTest, ForwardingConstructorFromMoveOnlyType) {
    using T = MoveOnlyType;
    indirect<T> instance(T(99));

    EXPECT_EQ(*instance, T(99));
}

// ========================================
// initializer_list Constructor Tests
// ========================================

TEST(IndirectTest, InitializerListConstructor) {
    using T = VectorWrapper;
    indirect<T> instance(std::in_place, {1, 2, 3, 4, 5});

    EXPECT_EQ(*instance, T({1, 2, 3, 4, 5}));
}

TEST(IndirectTest, InitializerListConstructorWithArgs) {
    using T = VectorWithInt;
    indirect<T> instance(std::in_place, {10, 20, 30}, 2);

    EXPECT_EQ(*instance, T({10, 20, 30}, 2));
}

TEST(IndirectTest, InitializerListConstructorWithAllocator) {
    using T = VectorWrapper;
    CountingAllocator<T> alloc;

    {
        indirect<T> instance(std::allocator_arg, alloc, std::in_place, {7, 8, 9});

        EXPECT_EQ(*instance, T({7, 8, 9}));
    }

    ASSERT_NO_LEAKS(alloc);
}

TEST(IndirectTest, InitializerListConstructorWithAllocatorAndArgs) {
    using T = VectorWithInt;
    CountingAllocator<T> alloc;

    {
        indirect<T> instance(std::allocator_arg, alloc, std::in_place, {100, 200}, 5);

        EXPECT_EQ(*instance, T({100, 200}, 5));
    }

    ASSERT_NO_LEAKS(alloc);
}
