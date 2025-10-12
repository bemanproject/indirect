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

    constexpr CountingAllocator() = default;
    constexpr explicit CountingAllocator(size_type init_num) : num_allocated(init_num), num_deallocated(init_num) {}
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
};

// making sure the allocators are consteval-able
static_assert(std::invoke([]() {
    CountingAllocator<int> _default;
    (void)_default;
    return true;
}));

template <typename T>
using indirect = beman::indirect::indirect<T, CountingAllocator<T>>;

struct Incomplete {
    using Self = Incomplete;

    indirect<Self> ind;
    int            _ignore;
};

auto ASSERT_ALLOC_EVEN(const auto& alloc) { ASSERT_EQ(alloc.num_allocated, alloc.num_deallocated); }

TEST(IndirectRawTest, CanHoldIncompleteType) {
    // Passes if it compiles, this is statuary

    struct Incomplete {
        using Self = Incomplete;

        indirect<Self> ind;
        int            _ignore;
    };
}

// Should be generic
TEST(IncompleteTests, DefaultInitializable) { indirect<Incomplete> instance; }

TEST(IncompleteTests, DefaultInitializableWAllocator) {
    CountingAllocator<Incomplete> alloc(20);
    {
        indirect<Incomplete> instance(std::allocator_arg_t{}, alloc);
    }
    ASSERT_ALLOC_EVEN(alloc);
    // Make sure the allocator is actually passed in
    ASSERT_EQ(alloc.num_allocated, 21);
}

template <std::size_t* num_constructor_calls>
struct Composite {
    Composite() { (*num_constructor_calls)++; }
    Composite(int _a, int _b, int _c) : a(_a), b(_b), c(_c) { (*num_constructor_calls)++; }

    int a, b, c;
};

TEST(IndirectTest, InitializeWForward) {
    static std::size_t num_constructor_calls = 0;
    num_constructor_calls                    = 0;

    using T = Composite<&num_constructor_calls>;
    indirect<T> instance(1, 2, 3);

    EXPECT_EQ(num_constructor_calls, 1);
    EXPECT_EQ(*instance, T(1, 2, 3));
}

TEST(IndirectTest, InitializeWForwardAndAlloc) {
    static std::size_t num_constructor_calls = 0;
    num_constructor_calls                    = 0;

    using T = Composite<&num_constructor_calls>;
    CountingAllocator<T> alloc(25);

    {
        indirect<T> instance(1, 2, 3);
        EXPECT_EQ(num_constructor_calls, 1);
        EXPECT_EQ(*instance, T(1, 2, 3));
    }

    ASSERT_ALLOC_EVEN(alloc);
    EXPECT_EQ(alloc.num_allocated, 26);
}
