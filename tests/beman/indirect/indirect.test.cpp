// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// TODO: Error checking

#include <beman/indirect/indirect.hpp>

#include <cstddef>
#include <functional>
#include <gtest/gtest.h>
#include <memory>
#include <sys/types.h>
#include <variant>

enum AllocatorPropagationPolicy {
    PROPAGATE_NONE    = 0b000,
    PROPAGATE_ON_COPY = 0b001,
    PROPAGATE_ON_MOVE = 0b010,
    PROPAGATE_ON_SWAP = 0b100,
};

static constexpr int DEFAULT_ALLOCATOR_ID = -1;

template <typename T, AllocatorPropagationPolicy policy = PROPAGATE_NONE>
struct TestAllocator;
template <typename T, AllocatorPropagationPolicy policy = PROPAGATE_NONE>
struct CountingAllocatorControl;

template <typename T, AllocatorPropagationPolicy policy>
constexpr TestAllocator<T, policy> create_handle(CountingAllocatorControl<T, policy>&);

template <typename T, AllocatorPropagationPolicy policy>
struct CountingAllocatorControl {
  public:
    using size_type      = std::size_t;
    using Id_type        = int;
    using allocator_type = TestAllocator<T, policy>;

    constexpr CountingAllocatorControl() = default;
    constexpr CountingAllocatorControl(Id_type id_) : id(id_) {}

    constexpr T* allocate(std::size_t n) {
        ++num_allocated;
        return _backing.allocate(n);
    }

    constexpr void deallocate(T* p, std::size_t n) {
        ++num_deallocated;
        _backing.deallocate(p, n);
    }

    constexpr TestAllocator<T, policy> handle() { return create_handle(*this); }

    std::allocator<T> _backing{};
    size_type         num_allocated   = 0;
    size_type         num_deallocated = 0;
    Id_type           id              = DEFAULT_ALLOCATOR_ID;
};

template <typename T, AllocatorPropagationPolicy policy>
constexpr TestAllocator<T, policy> create_handle(CountingAllocatorControl<T, policy>& ctl) {
    return TestAllocator{std::ref(ctl)};
}

/**
 * There's two kinds of testing allocator:
 * 1. The default allocator, directly funnel alloc/ dealloc into std::allocator
 * 2. The counting allocator, redirect alloc/ dealloc into a allocator manager,
 *    allows stats collection/ instrummented exception behavior.
 */
template <typename T, AllocatorPropagationPolicy policy>
struct TestAllocator {
    using compatible_control_block = std::reference_wrapper<CountingAllocatorControl<T, policy>>;
    using default_allocator        = std::allocator<T>;

    struct ControlBlockWrapper {
        constexpr ControlBlockWrapper(compatible_control_block&& ctl_)
            : ctl(std::forward<compatible_control_block>(ctl_)) {}
        constexpr T*             allocate(std::size_t n) { return ctl.get().allocate(n); }
        constexpr void           deallocate(T* p, std::size_t n) { ctl.get().deallocate(p, n); }
        compatible_control_block ctl;
    };

  public:
    using value_type                             = T;
    using size_type                              = std::size_t;
    using difference_type                        = std::ptrdiff_t;
    using propagate_on_container_copy_assignment = std::bool_constant<(policy & PROPAGATE_ON_COPY) != 0>;
    using propagate_on_container_move_assignment = std::bool_constant<(policy & PROPAGATE_ON_MOVE) != 0>;
    using propagate_on_container_swap            = std::bool_constant<(policy & PROPAGATE_ON_SWAP) != 0>;
    using Id_type                                = CountingAllocatorControl<T, policy>::Id_type;

    constexpr TestAllocator() : backing(default_allocator{}), id(DEFAULT_ALLOCATOR_ID) {}
    constexpr TestAllocator(compatible_control_block&& c)
        : backing(std::forward<compatible_control_block>(c)), id(c.get().id) {}

    constexpr T* allocate(std::size_t n) {
        return std::visit([n](auto&& v) { return v.allocate(n); }, backing);
    }

    constexpr void deallocate(T* p, std::size_t n) {
        std::visit([p, n](auto&& v) { v.deallocate(p, n); }, backing);
    }

    bool operator==(const TestAllocator& other) const { return id == other.id; }
    bool operator!=(const TestAllocator& other) const { return id != other.id; }

    std::variant<default_allocator, ControlBlockWrapper> backing;
    Id_type                                              id = DEFAULT_ALLOCATOR_ID;
};

// Allocator tests

static_assert(std::invoke([]() {
    std::allocator<int> alloc;
    alloc.deallocate(alloc.allocate(1), 1);
    return true;
}));

static_assert(std::invoke([]() {
    TestAllocator<int> alloc;
    auto               p = alloc.allocate(1);
    alloc.deallocate(p, 1);
    return true;
}));

static_assert(std::invoke([]() {
    CountingAllocatorControl<int> ctl;
    auto                          p = ctl.handle().allocate(1);
    ctl.handle().deallocate(p, 1);

    return ctl.num_allocated == 1 && ctl.num_deallocated == 1;
}));

static_assert(std::invoke([]() {
    CountingAllocatorControl<int> ctl;

    auto alloc  = ctl.handle();
    auto p      = alloc.allocate(1);
    auto alloc2 = alloc;
    alloc2.deallocate(p, 1);

    return ctl.num_allocated == 1 && ctl.num_deallocated == 1;
}));

#define ASSERT_NO_LEAKS(alloc) EXPECT_EQ(alloc.num_allocated, alloc.num_deallocated)

template <typename T, typename Alloc = TestAllocator<T>>
using indirect = beman::indirect::indirect<T, Alloc>;

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
    SimpleType& operator=(const SimpleType&) = default;
    bool        operator==(const SimpleType& other) const { return value == other.value; }
};

struct ConvertibleToSimpleType {
    int value;
    explicit ConvertibleToSimpleType(int v) : value(v) {}
    operator SimpleType() const { return SimpleType(value); }
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
    CountingAllocatorControl<T> alloc;

    {
        indirect<T> instance(std::allocator_arg, alloc.handle());

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
    CountingAllocatorControl<T> alloc;

    {
        indirect<T> instance(std::allocator_arg, alloc.handle(), std::in_place, 1, 2, 3);

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
    CountingAllocatorControl<T> alloc;

    {
        indirect<T> instance(std::allocator_arg, alloc.handle(), std::in_place, 7, 8, 9);

        EXPECT_EQ(*instance, T(7, 8, 9));
    }

    ASSERT_NO_LEAKS(alloc);
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
    CountingAllocatorControl<T> alloc1(100);
    CountingAllocatorControl<T> alloc2(200);

    {
        indirect<T> original(std::allocator_arg, alloc1.handle(), std::in_place, 10, 20, 30);

        indirect<T> copy(std::allocator_arg, alloc2.handle(), original);

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
    CountingAllocatorControl<T> alloc;

    {
        indirect<T> original(std::allocator_arg, alloc.handle(), std::in_place, 10, 20, 30);
        EXPECT_FALSE(original.valueless_after_move());

        indirect<T> moved(std::allocator_arg, alloc.handle(), std::move(original));

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
    CountingAllocatorControl<T> alloc;

    {
        T           value(42);
        indirect<T> instance(std::allocator_arg, alloc.handle(), value);

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
    CountingAllocatorControl<T> alloc;

    {
        indirect<T> instance(std::allocator_arg, alloc.handle(), std::in_place, {7, 8, 9});

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
    CountingAllocatorControl<T> alloc;

    {
        indirect<T> instance(std::allocator_arg, alloc.handle(), std::in_place, {100, 200}, 5);

        EXPECT_EQ(*instance, T({100, 200}, 5));
    }

    ASSERT_NO_LEAKS(alloc);
}

// ========================================
// Copy Assignment Operator Tests
// ========================================

/**
 * constexpr indirect& operator=(const indirect& other);
 *
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
 *
 * TODO: Test for exception behavior
 */

TEST(IndirectTest, CopyAssignmentBasic) {
    using T = Composite;
    indirect<T> source(std::in_place, 10, 20, 30);
    indirect<T> target(std::in_place, 1, 2, 3);

    target = source;

    EXPECT_EQ(*target, T(10, 20, 30));
    EXPECT_EQ(*source, T(10, 20, 30));

    // Verify independence
    target->a = 999;
    EXPECT_EQ(*target, T(999, 20, 30));
    EXPECT_EQ(*source, T(10, 20, 30));
}

TEST(IndirectTest, CopyAssignmentSelfAssignment) {
    using T = Composite;
    indirect<T> instance(std::in_place, 10, 20, 30);

    instance = instance;

    EXPECT_EQ(*instance, T(10, 20, 30));
    EXPECT_FALSE(instance.valueless_after_move());
}

TEST(IndirectTest, CopyAssignmentWithAllocator) {
    using T = Composite;
    CountingAllocatorControl<T> alloc1;
    CountingAllocatorControl<T> alloc2;

    {
        indirect<T> source(std::allocator_arg, alloc1.handle(), std::in_place, 10, 20, 30);
        indirect<T> target(std::allocator_arg, alloc2.handle(), std::in_place, 1, 2, 3);

        target = source;

        EXPECT_EQ(*target, T(10, 20, 30));
        EXPECT_EQ(*source, T(10, 20, 30));
    }

    ASSERT_NO_LEAKS(alloc1);
    ASSERT_NO_LEAKS(alloc2);
}

TEST(IndirectTest, CopyAssignmentFromValuelessToNonValueless) {
    using T = Composite;
    indirect<T> source(std::in_place, 10, 20, 30);
    indirect<T> target(std::in_place, 1, 2, 3);

    // Make source valueless
    indirect<T> temp(std::move(source));
    EXPECT_TRUE(source.valueless_after_move());
    EXPECT_FALSE(target.valueless_after_move());

    // Assign from valueless source to non-valueless target
    target = source;

    // Both should be valueless now
    EXPECT_TRUE(source.valueless_after_move());
    EXPECT_TRUE(target.valueless_after_move());
}

TEST(IndirectTest, CopyAssignmentFromNonValuelessToValueless) {
    using T = Composite;
    indirect<T> source(std::in_place, 10, 20, 30);
    indirect<T> target(std::in_place, 1, 2, 3);

    // Make target valueless
    indirect<T> temp(std::move(target));
    EXPECT_FALSE(source.valueless_after_move());
    EXPECT_TRUE(target.valueless_after_move());

    // Assign from non-valueless source to valueless target
    target = source;

    EXPECT_EQ(*target, T(10, 20, 30));
    EXPECT_EQ(*source, T(10, 20, 30));
    EXPECT_FALSE(target.valueless_after_move());
}

TEST(IndirectTest, CopyAssignmentFromValuelessToValueless) {
    using T = Composite;
    indirect<T> source(std::in_place, 10, 20, 30);
    indirect<T> target(std::in_place, 1, 2, 3);

    // Make both valueless
    indirect<T> temp1(std::move(source));
    indirect<T> temp2(std::move(target));
    EXPECT_TRUE(source.valueless_after_move());
    EXPECT_TRUE(target.valueless_after_move());

    // Assign from valueless source to valueless target
    target = source;

    EXPECT_TRUE(source.valueless_after_move());
    EXPECT_TRUE(target.valueless_after_move());
}

TEST(IndirectTest, CopyAssignmentWithPropagateCopyAllocator) {
    // Test condition 1 and 6: allocator propagation on copy assignment
    // 1. The allocator needs updating if
    //    allocator_traits<Allocator>::propagate_on_container_copy_assignment::value is true.
    // 6. If the allocator needs updating, the allocator in *this is replaced with a copy of the allocator in other.

    using T        = Composite;
    using AllocCtl = CountingAllocatorControl<T, PROPAGATE_ON_COPY>;
    using Alloc    = AllocCtl::allocator_type;

    static_assert(std::allocator_traits<Alloc>::propagate_on_container_copy_assignment::value);

    AllocCtl alloc1(100);
    AllocCtl alloc2(200);

    {
        indirect<T, Alloc> source(std::allocator_arg, alloc1.handle(), std::in_place, 10, 20, 30);
        indirect<T, Alloc> target(std::allocator_arg, alloc2.handle(), std::in_place, 1, 2, 3);

        // Before assignment, allocators are different
        EXPECT_NE(source.get_allocator(), target.get_allocator());
        EXPECT_EQ(source.get_allocator().id, 100);
        EXPECT_EQ(target.get_allocator().id, 200);

        target = source;

        // After assignment with propagate_on_container_copy_assignment=true,
        // target should have source's allocator
        EXPECT_EQ(*target, T(10, 20, 30));
        EXPECT_EQ(*source, T(10, 20, 30));
        EXPECT_EQ(target.get_allocator(), source.get_allocator());
        EXPECT_EQ(target.get_allocator().id, 100);
    }

    ASSERT_NO_LEAKS(alloc1);
    ASSERT_NO_LEAKS(alloc2);
}

TEST(IndirectTest, CopyAssignmentWithoutPropagateCopyAllocator) {
    // Test inverse to the case above:
    // allocator does NOT propagate when propagate_on_container_copy_assignment is false
    using T        = Composite;
    using AllocCtl = CountingAllocatorControl<T, PROPAGATE_NONE>;
    using Alloc    = AllocCtl::allocator_type;

    static_assert(!std::allocator_traits<Alloc>::propagate_on_container_copy_assignment::value);

    AllocCtl alloc1(100);
    AllocCtl alloc2(200);

    {
        indirect<T, Alloc> source(std::allocator_arg, alloc1.handle(), std::in_place, 10, 20, 30);
        indirect<T, Alloc> target(std::allocator_arg, alloc2.handle(), std::in_place, 1, 2, 3);

        // Before assignment, allocators are different
        EXPECT_NE(source.get_allocator(), target.get_allocator());
        EXPECT_EQ(source.get_allocator().id, 100);
        EXPECT_EQ(target.get_allocator().id, 200);

        target = source;

        // After assignment with propagate_on_container_copy_assignment=false,
        // target should keep its original allocator
        EXPECT_EQ(*target, T(10, 20, 30));
        EXPECT_EQ(*source, T(10, 20, 30));
        EXPECT_NE(target.get_allocator(), source.get_allocator());
        EXPECT_EQ(target.get_allocator().id, 200); // Still has original allocator
        EXPECT_EQ(source.get_allocator().id, 100);
    }

    ASSERT_NO_LEAKS(alloc1);
    ASSERT_NO_LEAKS(alloc2);
}

TEST(IndirectTest, CopyAssignmentDifferentAllocatorsConstructsNewObject) {
    // Tests for
    // 4. Otherwise (other is not valueless, and "this" and "other" does not share allocator)
    //    a new owned object is constructed in *this using allocator_traits<Allocator>::construct
    //    with the owned object from other as the argument,
    //    using either the allocator in *this or the allocator in other if the allocator needs updating.
    // 5. The previously owned object in *this, if any,
    //    is destroyed using allocator_traits<Allocator>::destroy and then the storage is deallocated.

    using T = Composite;
    CountingAllocatorControl<T> alloc1(100);
    CountingAllocatorControl<T> alloc2(200);

    {
        indirect<T> source(std::allocator_arg, alloc1.handle(), std::in_place, 10, 20, 30);
        indirect<T> target(std::allocator_arg, alloc2.handle(), std::in_place, 1, 2, 3);

        EXPECT_NE(source.get_allocator(), target.get_allocator());

        // Before assignment: target's allocator has allocated 1 object
        EXPECT_EQ(alloc2.num_allocated, 1);

        // Assignment with different allocators should construct new object
        target = source;

        // After assignment: target's allocator should have allocated another object
        EXPECT_EQ(alloc2.num_allocated, 2);   // New allocation happened
        EXPECT_EQ(alloc2.num_deallocated, 1); // Original allocation destroyed
        EXPECT_EQ(*target, T(10, 20, 30));
        EXPECT_EQ(*source, T(10, 20, 30));
    }

    ASSERT_NO_LEAKS(alloc1);
    ASSERT_NO_LEAKS(alloc2);
}

TEST(IndirectTest, CopyAssignmentSameAllocatorInPlace) {
    // Test condition 3: if alloc == other.alloc and *this is not valueless, use **this = *other
    using T = Composite;
    CountingAllocatorControl<T> alloc(100);

    {
        indirect<T> source(std::allocator_arg, alloc.handle(), std::in_place, 10, 20, 30);
        indirect<T> target(std::allocator_arg, alloc.handle(), std::in_place, 1, 2, 3);

        EXPECT_EQ(source.get_allocator(), target.get_allocator());

        // Before assignment: allocator has allocated 2 objects
        EXPECT_EQ(alloc.num_allocated, 2);

        // Both use same allocator, so assignment should be in-place via **this = *other
        // This should NOT allocate a new object
        target = source;

        // After assignment: no new allocation should have occurred
        EXPECT_EQ(alloc.num_allocated, 2);
        EXPECT_EQ(alloc.num_deallocated, 0);

        EXPECT_EQ(*target, T(10, 20, 30));
        EXPECT_EQ(*source, T(10, 20, 30));
    }

    ASSERT_NO_LEAKS(alloc);
}

// ========================================
// Move Assignment Operator Tests
// ========================================

/**
 * constexpr indirect& operator=(indirect&& other) noexcept(
 *     std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value ||
 *     std::allocator_traits<Allocator>::is_always_equal::value);
 *
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

TEST(IndirectTest, MoveAssignmentBasic) {
    using T = Composite;
    indirect<T> source(std::in_place, 10, 20, 30);
    indirect<T> target(std::in_place, 1, 2, 3);

    EXPECT_FALSE(source.valueless_after_move());

    target = std::move(source);

    EXPECT_EQ(*target, T(10, 20, 30));
    EXPECT_TRUE(source.valueless_after_move());
}

TEST(IndirectTest, MoveAssignmentSelfAssignment) {
    using T = Composite;
    indirect<T> instance(std::in_place, 10, 20, 30);

    instance = std::move(instance);

    EXPECT_EQ(*instance, T(10, 20, 30));
    EXPECT_FALSE(instance.valueless_after_move());
}

TEST(IndirectTest, MoveAssignmentWithAllocator) {
    using T = Composite;
    CountingAllocatorControl<T> alloc1;
    CountingAllocatorControl<T> alloc2;

    {
        indirect<T> source(std::allocator_arg, alloc1.handle(), std::in_place, 10, 20, 30);
        indirect<T> target(std::allocator_arg, alloc2.handle(), std::in_place, 1, 2, 3);

        target = std::move(source);

        EXPECT_EQ(*target, T(10, 20, 30));
        EXPECT_TRUE(source.valueless_after_move());
    }

    ASSERT_NO_LEAKS(alloc1);
    ASSERT_NO_LEAKS(alloc2);
}

TEST(IndirectTest, MoveAssignmentSameAllocator) {
    using T = Composite;
    CountingAllocatorControl<T> alloc;

    {
        indirect<T> source(std::allocator_arg, alloc.handle(), std::in_place, 10, 20, 30);
        indirect<T> target(std::allocator_arg, alloc.handle(), std::in_place, 1, 2, 3);

        target = std::move(source);

        EXPECT_EQ(*target, T(10, 20, 30));
        EXPECT_TRUE(source.valueless_after_move());
    }

    ASSERT_NO_LEAKS(alloc);
}

TEST(IndirectTest, MoveAssignmentMoveOnlyType) {
    using T = MoveOnlyType;
    indirect<T> source(std::in_place, 99);
    indirect<T> target(std::in_place, 1);

    target = std::move(source);

    EXPECT_EQ(*target, T(99));
    EXPECT_TRUE(source.valueless_after_move());
}

TEST(IndirectTest, MoveAssignmentFromValuelessToNonValueless) {
    using T = Composite;
    indirect<T> source(std::in_place, 10, 20, 30);
    indirect<T> target(std::in_place, 1, 2, 3);

    // Make source valueless
    indirect<T> temp(std::move(source));
    EXPECT_TRUE(source.valueless_after_move());
    EXPECT_FALSE(target.valueless_after_move());

    // Move assign from valueless source to non-valueless target
    target = std::move(source);

    // Both should be valueless now
    EXPECT_TRUE(source.valueless_after_move());
    EXPECT_TRUE(target.valueless_after_move());
}

TEST(IndirectTest, MoveAssignmentFromNonValuelessToValueless) {
    using T = Composite;
    indirect<T> source(std::in_place, 10, 20, 30);
    indirect<T> target(std::in_place, 1, 2, 3);

    // Make target valueless
    indirect<T> temp(std::move(target));
    EXPECT_FALSE(source.valueless_after_move());
    EXPECT_TRUE(target.valueless_after_move());

    // Move assign from non-valueless source to valueless target
    target = std::move(source);

    EXPECT_EQ(*target, T(10, 20, 30));
    EXPECT_TRUE(source.valueless_after_move());
    EXPECT_FALSE(target.valueless_after_move());
}

TEST(IndirectTest, MoveAssignmentFromValuelessToValueless) {
    using T = Composite;
    indirect<T> source(std::in_place, 10, 20, 30);
    indirect<T> target(std::in_place, 1, 2, 3);

    // Make both valueless
    indirect<T> temp1(std::move(source));
    indirect<T> temp2(std::move(target));
    EXPECT_TRUE(source.valueless_after_move());
    EXPECT_TRUE(target.valueless_after_move());

    // Move assign from valueless source to valueless target
    target = std::move(source);

    EXPECT_TRUE(source.valueless_after_move());
    EXPECT_TRUE(target.valueless_after_move());
}

TEST(IndirectTest, MoveAssignmentWithPropagateMoveAllocator) {
    // Test condition 1 and 6: allocator propagation on move assignment
    // 1. The allocator needs updating if
    //    allocator_traits<Allocator>::propagate_on_container_move_assignment::value is true.
    // 6. If the allocator needs updating, the allocator in *this is replaced with a copy of the allocator in other.

    using T        = Composite;
    using AllocCtl = CountingAllocatorControl<T, PROPAGATE_ON_MOVE>;
    using Alloc    = AllocCtl::allocator_type;

    static_assert(std::allocator_traits<Alloc>::propagate_on_container_move_assignment::value);

    AllocCtl alloc1(100);
    AllocCtl alloc2(200);

    {
        indirect<T, Alloc> source(std::allocator_arg, alloc1.handle(), std::in_place, 10, 20, 30);
        indirect<T, Alloc> target(std::allocator_arg, alloc2.handle(), std::in_place, 1, 2, 3);

        // Before assignment, allocators are different
        EXPECT_NE(source.get_allocator(), target.get_allocator());
        EXPECT_EQ(source.get_allocator().id, 100);
        EXPECT_EQ(target.get_allocator().id, 200);

        target = std::move(source);

        // After move assignment with propagate_on_container_move_assignment=true,
        // target should have source's allocator
        EXPECT_EQ(*target, T(10, 20, 30));
        EXPECT_TRUE(source.valueless_after_move());
        EXPECT_EQ(target.get_allocator().id, 100);
    }

    ASSERT_NO_LEAKS(alloc1);
    ASSERT_NO_LEAKS(alloc2);
}

TEST(IndirectTest, MoveAssignmentWithoutPropagateMoveAllocator) {
    // Test inverse to the case above:
    // allocator does NOT propagate when propagate_on_container_move_assignment is false
    using T        = Composite;
    using AllocCtl = CountingAllocatorControl<T, PROPAGATE_NONE>;
    using Alloc    = AllocCtl::allocator_type;

    static_assert(!std::allocator_traits<Alloc>::propagate_on_container_move_assignment::value);

    AllocCtl alloc1(100);
    AllocCtl alloc2(200);

    {
        indirect<T, Alloc> source(std::allocator_arg, alloc1.handle(), std::in_place, 10, 20, 30);
        indirect<T, Alloc> target(std::allocator_arg, alloc2.handle(), std::in_place, 1, 2, 3);

        // Before assignment, allocators are different
        EXPECT_NE(source.get_allocator(), target.get_allocator());
        EXPECT_EQ(source.get_allocator().id, 100);
        EXPECT_EQ(target.get_allocator().id, 200);

        target = std::move(source);

        // After move assignment with propagate_on_container_move_assignment=false,
        // target should keep its original allocator
        EXPECT_EQ(*target, T(10, 20, 30));
        EXPECT_TRUE(source.valueless_after_move());
        EXPECT_EQ(target.get_allocator().id, 200); // Still has original allocator
    }

    ASSERT_NO_LEAKS(alloc1);
    ASSERT_NO_LEAKS(alloc2);
}

TEST(IndirectTest, MoveAssignmentDifferentAllocatorsConstructsNewObject) {
    // Tests for
    // 4. Otherwise (other is not valueless, and "this" and "other" does not share allocator)
    //    constructs a new owned object with the owned object of other as the argument as an rvalue,
    //    using either the allocator in *this or the allocator in other if the allocator needs updating.
    // 5. The previously owned object in *this, if any,
    //    is destroyed using allocator_traits<Allocator>::destroy and then the storage is deallocated.

    using T = Composite;
    CountingAllocatorControl<T> alloc1(100);
    CountingAllocatorControl<T> alloc2(200);

    {
        indirect<T> source(std::allocator_arg, alloc1.handle(), std::in_place, 10, 20, 30);
        indirect<T> target(std::allocator_arg, alloc2.handle(), std::in_place, 1, 2, 3);

        EXPECT_NE(source.get_allocator(), target.get_allocator());

        // Before assignment: target's allocator has allocated 1 object
        EXPECT_EQ(alloc2.num_allocated, 1);

        // Move assignment with different allocators should construct new object
        target = std::move(source);

        // After assignment: target's allocator should have allocated another object
        EXPECT_EQ(alloc2.num_allocated, 2);   // New allocation happened
        EXPECT_EQ(alloc2.num_deallocated, 1); // Original allocation destroyed
        EXPECT_EQ(*target, T(10, 20, 30));
        EXPECT_TRUE(source.valueless_after_move());
    }

    ASSERT_NO_LEAKS(alloc1);
    ASSERT_NO_LEAKS(alloc2);
}

TEST(IndirectTest, MoveAssignmentSameAllocatorSwap) {
    // Test condition 3: if alloc == other.alloc, swaps the owned objects then destroys
    using T = Composite;
    CountingAllocatorControl<T> alloc(100);

    {
        indirect<T> source(std::allocator_arg, alloc.handle(), std::in_place, 10, 20, 30);
        indirect<T> target(std::allocator_arg, alloc.handle(), std::in_place, 1, 2, 3);

        EXPECT_EQ(source.get_allocator(), target.get_allocator());

        // Before assignment: allocator has allocated 2 objects
        EXPECT_EQ(alloc.num_allocated, 2);
        size_t alloc_count_before = alloc.num_allocated;

        // Both use same allocator, so move assignment should swap then destroy
        target = std::move(source);

        // After assignment: no new allocation (swap + destroy, not construct new)
        EXPECT_EQ(alloc.num_allocated, alloc_count_before); // No new allocation
        EXPECT_EQ(alloc.num_deallocated, 1);                // One object destroyed (the swapped one)
        EXPECT_EQ(*target, T(10, 20, 30));
        EXPECT_TRUE(source.valueless_after_move());
    }

    ASSERT_NO_LEAKS(alloc);
}

// ========================================
// Forwarding Assignment Operator Tests
// ========================================

/**
 * template <class U = T>
 * constexpr indirect& operator=(U&& u);
 *
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

TEST(IndirectTest, ForwardingAssignmentFromLValue) {
    using T = SimpleType;
    CountingAllocatorControl<T> alloc;

    {
        indirect<T> instance(std::allocator_arg, alloc.handle(), std::in_place, 42);
        T           value(99);

        instance = value;

        EXPECT_EQ(*instance, T(99));
        EXPECT_EQ(value, T(99)); // Original unchanged
    }

    EXPECT_EQ(alloc.num_allocated, 1);
    ASSERT_NO_LEAKS(alloc);
}

TEST(IndirectTest, ForwardingAssignmentFromRValue) {
    using T = SimpleType;

    CountingAllocatorControl<T> alloc;

    {
        indirect<T> instance(std::allocator_arg, alloc.handle(), std::in_place, 42);

        instance = T(99);

        EXPECT_EQ(*instance, T(99));
    }

    EXPECT_EQ(alloc.num_allocated, 1);
    ASSERT_NO_LEAKS(alloc);
}

TEST(IndirectTest, ForwardingAssignmentFromLValueToValueless) {
    using T = SimpleType;
    CountingAllocatorControl<T> alloc;

    {
        indirect<T> instance(std::allocator_arg, alloc.handle(), std::in_place, 42);

        // Make instance valueless
        indirect<T> temp(std::move(instance));
        EXPECT_TRUE(instance.valueless_after_move());

        // Assign to valueless instance - should construct new object
        T value(99);
        instance = value;

        EXPECT_FALSE(instance.valueless_after_move());
        EXPECT_EQ(*instance, T(99));
        EXPECT_EQ(value, T(99)); // Original unchanged
    }

    // Should have 2 allocations: initial + new construction for valueless target
    EXPECT_EQ(alloc.num_allocated, 2);
    ASSERT_NO_LEAKS(alloc);
}

TEST(IndirectTest, ForwardingAssignmentFromRValueToValueless) {
    using T = SimpleType;
    CountingAllocatorControl<T> alloc;

    {
        indirect<T> instance(std::allocator_arg, alloc.handle(), std::in_place, 42);

        // Make instance valueless
        indirect<T> temp(std::move(instance));
        EXPECT_TRUE(instance.valueless_after_move());

        // Assign rvalue to valueless instance - should construct new object
        instance = T(99);

        EXPECT_FALSE(instance.valueless_after_move());
        EXPECT_EQ(*instance, T(99));
    }

    // Should have 2 allocations: initial + new construction for valueless target
    EXPECT_EQ(alloc.num_allocated, 2);
    ASSERT_NO_LEAKS(alloc);
}

TEST(IndirectTest, ForwardingAssignmentFromConvertibleType) {
    // Test U != T case where U is convertible to T
    using T = SimpleType;
    CountingAllocatorControl<T> alloc;

    {
        indirect<T>             instance(std::allocator_arg, alloc.handle(), std::in_place, 42);
        ConvertibleToSimpleType value(99);

        instance = value;

        EXPECT_EQ(*instance, T(99));
        EXPECT_EQ(value.value, 99); // Original unchanged
    }

    EXPECT_EQ(alloc.num_allocated, 1);
    ASSERT_NO_LEAKS(alloc);
}

TEST(IndirectTest, ForwardingAssignmentFromConvertibleTypeRValue) {
    // Test U != T case with rvalue where U is convertible to T
    using T = SimpleType;
    CountingAllocatorControl<T> alloc;

    {
        indirect<T> instance(std::allocator_arg, alloc.handle(), std::in_place, 42);

        instance = ConvertibleToSimpleType(99);

        EXPECT_EQ(*instance, T(99));
    }

    EXPECT_EQ(alloc.num_allocated, 1);
    ASSERT_NO_LEAKS(alloc);
}

TEST(IndirectTest, ForwardingAssignmentFromConvertibleTypeToValueless) {
    // Test U != T case assigning to valueless instance
    using T = SimpleType;
    CountingAllocatorControl<T> alloc;

    {
        indirect<T> instance(std::allocator_arg, alloc.handle(), std::in_place, 42);

        // Make instance valueless
        indirect<T> temp(std::move(instance));
        EXPECT_TRUE(instance.valueless_after_move());

        // Assign convertible type to valueless instance - should construct new object
        ConvertibleToSimpleType value(99);
        instance = value;

        EXPECT_FALSE(instance.valueless_after_move());
        EXPECT_EQ(*instance, T(99));
        EXPECT_EQ(value.value, 99); // Original unchanged
    }

    // Should have 2 allocations: initial + new construction for valueless target
    EXPECT_EQ(alloc.num_allocated, 2);
    ASSERT_NO_LEAKS(alloc);
}

// ========================================
// Access Operator Tests
// ========================================

/**
 * constexpr const T& operator*() const& noexcept;
 *
 * Preconditions: *this is not valueless.
 *
 * Returns: *p.
 */
TEST(IndirectTest, DereferenceOperatorConstLValue) {
    using T = Composite;
    const indirect<T> instance(std::in_place, 10, 20, 30);

    const T& ref = *instance;
    static_assert(std::is_const<std::remove_reference_t<decltype(ref)>>::value, "Should return const T&");

    EXPECT_EQ(ref, T(10, 20, 30));
    EXPECT_EQ(ref.a, 10);
    EXPECT_EQ(ref.b, 20);
    EXPECT_EQ(ref.c, 30);
}

/**
 * constexpr T& operator*() & noexcept;
 *
 * Preconditions: *this is not valueless.
 *
 * Returns: *p.
 */
TEST(IndirectTest, DereferenceOperatorLValue) {
    using T = Composite;
    indirect<T> instance(std::in_place, 10, 20, 30);

    T& ref = *instance;
    static_assert(!std::is_const<std::remove_reference_t<decltype(ref)>>::value, "Should return non-const T&");

    EXPECT_EQ(ref, T(10, 20, 30));

    // Verify we can modify through the reference
    ref.a = 999;
    EXPECT_EQ(*instance, T(999, 20, 30));
}

/**
 * constexpr const T&& operator*() const&& noexcept;
 *
 * Preconditions: *this is not valueless.
 *
 * Returns: std::move(*p).
 */
TEST(IndirectTest, DereferenceOperatorConstRValue) {
    using T = SimpleType;

    const T&& ref = *indirect<T>(std::in_place, 42);
    static_assert(std::is_const<std::remove_reference_t<decltype(ref)>>::value, "Should return const T&&");

    EXPECT_EQ(ref, T(42));
}

/**
 * constexpr T&& operator*() && noexcept;
 *
 * Preconditions: *this is not valueless.
 *
 * Returns: std::move(*p).
 */
TEST(IndirectTest, DereferenceOperatorRValue) {
    using T = SimpleType;

    T&& ref = *indirect<T>(std::in_place, 42);
    static_assert(!std::is_const<std::remove_reference_t<decltype(ref)>>::value, "Should return non-const T&&");

    EXPECT_EQ(ref, T(42));
}

/**
 * constexpr const_pointer operator->() const noexcept;
 *
 * Preconditions: *this is not valueless.
 *
 * Returns: p.
 */
TEST(IndirectTest, ArrowOperatorConst) {
    using T = Composite;
    const indirect<T> instance(std::in_place, 10, 20, 30);

    EXPECT_EQ(instance->a, 10);
    EXPECT_EQ(instance->b, 20);
    EXPECT_EQ(instance->c, 30);
}

/**
 * constexpr pointer operator->() noexcept;
 *
 * Preconditions: *this is not valueless.
 *
 * Returns: p.
 */
TEST(IndirectTest, ArrowOperator) {
    using T = Composite;
    indirect<T> instance(std::in_place, 10, 20, 30);

    EXPECT_EQ(instance->a, 10);
    EXPECT_EQ(instance->b, 20);
    EXPECT_EQ(instance->c, 30);

    // Verify we can modify through arrow operator
    instance->a = 999;
    EXPECT_EQ(instance->a, 999);
    EXPECT_EQ(*instance, T(999, 20, 30));
}

// ========================================
// valueless_after_move() Tests
// ========================================

/**
 * constexpr bool valueless_after_move() const noexcept;
 *
 * Returns: true if *this is valueless, otherwise false.
 */

TEST(IndirectTest, ValuelessAfterMoveDefaultConstructed) {
    using T = DefaultConstructible;
    indirect<T> instance;

    EXPECT_FALSE(instance.valueless_after_move());
}

TEST(IndirectTest, ValuelessAfterMoveInPlaceConstructed) {
    using T = Composite;
    indirect<T> instance(std::in_place, 10, 20, 30);

    EXPECT_FALSE(instance.valueless_after_move());
}

TEST(IndirectTest, ValuelessAfterMoveCopyConstructed) {
    using T = Composite;
    indirect<T> original(std::in_place, 10, 20, 30);
    indirect<T> copy(original);

    EXPECT_FALSE(original.valueless_after_move());
    EXPECT_FALSE(copy.valueless_after_move());
}

TEST(IndirectTest, ValuelessAfterMoveMoveConstructed) {
    using T = Composite;
    indirect<T> original(std::in_place, 10, 20, 30);

    EXPECT_FALSE(original.valueless_after_move());

    indirect<T> moved(std::move(original));

    EXPECT_TRUE(original.valueless_after_move());
    EXPECT_FALSE(moved.valueless_after_move());
}

TEST(IndirectTest, ValuelessAfterMoveMoveAssigned) {
    using T = Composite;
    indirect<T> source(std::in_place, 10, 20, 30);
    indirect<T> target(std::in_place, 1, 2, 3);

    EXPECT_FALSE(source.valueless_after_move());
    EXPECT_FALSE(target.valueless_after_move());

    target = std::move(source);

    EXPECT_TRUE(source.valueless_after_move());
    EXPECT_FALSE(target.valueless_after_move());
}

TEST(IndirectTest, ValuelessAfterMoveCopyAssignedFromValueless) {
    using T = Composite;
    indirect<T> source(std::in_place, 10, 20, 30);
    indirect<T> target(std::in_place, 1, 2, 3);

    // Make source valueless
    indirect<T> temp(std::move(source));
    EXPECT_TRUE(source.valueless_after_move());
    EXPECT_FALSE(target.valueless_after_move());

    // Copy assign from valueless source
    target = source;

    EXPECT_TRUE(source.valueless_after_move());
    EXPECT_TRUE(target.valueless_after_move());
}

TEST(IndirectTest, ValuelessAfterMoveCopyAssignedToValueless) {
    using T = Composite;
    indirect<T> source(std::in_place, 10, 20, 30);
    indirect<T> target(std::in_place, 1, 2, 3);

    // Make target valueless
    indirect<T> temp(std::move(target));
    EXPECT_FALSE(source.valueless_after_move());
    EXPECT_TRUE(target.valueless_after_move());

    // Copy assign to valueless target
    target = source;

    EXPECT_FALSE(source.valueless_after_move());
    EXPECT_FALSE(target.valueless_after_move());
}

TEST(IndirectTest, ValuelessAfterMoveForwardingAssignmentToValueless) {
    using T = SimpleType;
    indirect<T> instance(std::in_place, 42);

    // Make instance valueless
    indirect<T> temp(std::move(instance));
    EXPECT_TRUE(instance.valueless_after_move());

    // Forwarding assign to valueless instance
    T value(99);
    instance = value;

    EXPECT_FALSE(instance.valueless_after_move());
}

TEST(IndirectTest, ValuelessAfterMoveCopyConstructedFromValueless) {
    using T = Composite;
    indirect<T> original(std::in_place, 10, 20, 30);

    // Make original valueless
    indirect<T> temp(std::move(original));
    EXPECT_TRUE(original.valueless_after_move());

    // Copy construct from valueless
    indirect<T> copy(original);

    EXPECT_TRUE(original.valueless_after_move());
    EXPECT_TRUE(copy.valueless_after_move());
}

TEST(IndirectTest, ValuelessAfterMoveMoveConstructedFromValueless) {
    using T = Composite;
    indirect<T> original(std::in_place, 10, 20, 30);

    // Make original valueless
    indirect<T> temp1(std::move(original));
    EXPECT_TRUE(original.valueless_after_move());

    // Move construct from valueless
    indirect<T> temp2(std::move(original));

    EXPECT_TRUE(original.valueless_after_move());
    EXPECT_TRUE(temp2.valueless_after_move());
}

// ========================================
// get_allocator() Tests
// ========================================

/**
 * constexpr allocator_type get_allocator() const noexcept;
 *
 * Returns: alloc.
 */

TEST(IndirectTest, GetAllocatorWithPassedAllocator) {
    using T = Composite;
    CountingAllocatorControl<T> alloc(100);
    indirect<T>                 instance(std::allocator_arg, alloc.handle(), std::in_place, 10, 20, 30);

    EXPECT_EQ(instance.get_allocator().id, 100);
}

TEST(IndirectTest, GetAllocatorWithDefaultAllocator) {
    using T = Composite;
    indirect<T> instance(std::in_place, 10, 20, 30);

    auto alloc = instance.get_allocator();
    EXPECT_EQ(alloc.id, DEFAULT_ALLOCATOR_ID);
}

// ========================================
// swap() Tests
// ========================================

/**
 * constexpr void swap(indirect& other) noexcept(
 *     allocator_traits<Allocator>::propagate_on_container_swap::value ||
 *     allocator_traits<Allocator>::is_always_equal::value);
 *
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

TEST(IndirectTest, SwapBasic) {
    using T = Composite;
    indirect<T> lhs(std::in_place, 10, 20, 30);
    indirect<T> rhs(std::in_place, 40, 50, 60);

    lhs.swap(rhs);

    EXPECT_EQ(*lhs, T(40, 50, 60));
    EXPECT_EQ(*rhs, T(10, 20, 30));
    EXPECT_FALSE(lhs.valueless_after_move());
    EXPECT_FALSE(rhs.valueless_after_move());
}

TEST(IndirectTest, SwapNonValuelessWithNonValueless) {
    // Test swapping two non-valueless objects
    using T = Composite;
    CountingAllocatorControl<T> alloc;

    {
        indirect<T> lhs(std::allocator_arg, alloc.handle(), std::in_place, 1, 2, 3);
        indirect<T> rhs(std::allocator_arg, alloc.handle(), std::in_place, 7, 8, 9);

        EXPECT_FALSE(lhs.valueless_after_move());
        EXPECT_FALSE(rhs.valueless_after_move());

        lhs.swap(rhs);

        EXPECT_EQ(*lhs, T(7, 8, 9));
        EXPECT_EQ(*rhs, T(1, 2, 3));
        EXPECT_FALSE(lhs.valueless_after_move());
        EXPECT_FALSE(rhs.valueless_after_move());
    }

    ASSERT_NO_LEAKS(alloc);
}

TEST(IndirectTest, SwapNonValuelessWithValueless) {
    // Test swapping non-valueless with valueless
    using T = Composite;
    CountingAllocatorControl<T> alloc;

    {
        indirect<T> lhs(std::allocator_arg, alloc.handle(), std::in_place, 1, 2, 3);
        indirect<T> rhs(std::allocator_arg, alloc.handle(), std::in_place, 7, 8, 9);

        // Make rhs valueless
        indirect<T> temp(std::move(rhs));
        EXPECT_FALSE(lhs.valueless_after_move());
        EXPECT_TRUE(rhs.valueless_after_move());

        lhs.swap(rhs);

        // After swap: lhs should be valueless, rhs should have the original lhs value
        EXPECT_TRUE(lhs.valueless_after_move());
        EXPECT_FALSE(rhs.valueless_after_move());
        EXPECT_EQ(*rhs, T(1, 2, 3));
    }

    ASSERT_NO_LEAKS(alloc);
}

TEST(IndirectTest, SwapValuelessWithNonValueless) {
    // Test swapping valueless with non-valueless
    using T = Composite;
    CountingAllocatorControl<T> alloc;

    {
        indirect<T> lhs(std::allocator_arg, alloc.handle(), std::in_place, 1, 2, 3);
        indirect<T> rhs(std::allocator_arg, alloc.handle(), std::in_place, 7, 8, 9);

        // Make lhs valueless
        indirect<T> temp(std::move(lhs));
        EXPECT_TRUE(lhs.valueless_after_move());
        EXPECT_FALSE(rhs.valueless_after_move());

        lhs.swap(rhs);

        // After swap: lhs should have the original rhs value, rhs should be valueless
        EXPECT_FALSE(lhs.valueless_after_move());
        EXPECT_TRUE(rhs.valueless_after_move());
        EXPECT_EQ(*lhs, T(7, 8, 9));
    }

    ASSERT_NO_LEAKS(alloc);
}

TEST(IndirectTest, SwapValuelessWithValueless) {
    // Test swapping two valueless objects
    using T = Composite;
    CountingAllocatorControl<T> alloc;

    {
        indirect<T> lhs(std::allocator_arg, alloc.handle(), std::in_place, 1, 2, 3);
        indirect<T> rhs(std::allocator_arg, alloc.handle(), std::in_place, 7, 8, 9);

        // Make both valueless
        indirect<T> temp1(std::move(lhs));
        indirect<T> temp2(std::move(rhs));
        EXPECT_TRUE(lhs.valueless_after_move());
        EXPECT_TRUE(rhs.valueless_after_move());

        lhs.swap(rhs);

        // After swap: both should still be valueless
        EXPECT_TRUE(lhs.valueless_after_move());
        EXPECT_TRUE(rhs.valueless_after_move());
    }

    ASSERT_NO_LEAKS(alloc);
}

TEST(IndirectTest, SwapWithPropagateOnSwapTrue) {
    // Test that allocators are swapped when propagate_on_container_swap is true
    using T        = Composite;
    using AllocCtl = CountingAllocatorControl<T, PROPAGATE_ON_SWAP>;
    using Alloc    = AllocCtl::allocator_type;

    static_assert(std::allocator_traits<Alloc>::propagate_on_container_swap::value);

    Alloc::Id_type lhs_alloc_id = 100, rhs_alloc_id = 200;

    AllocCtl alloc1(lhs_alloc_id);
    AllocCtl alloc2(rhs_alloc_id);

    {
        indirect<T, Alloc> lhs(std::allocator_arg, alloc1.handle(), std::in_place, 1, 2, 3);
        indirect<T, Alloc> rhs(std::allocator_arg, alloc2.handle(), std::in_place, 7, 8, 9);

        // Before swap, allocators are different
        EXPECT_NE(lhs.get_allocator(), rhs.get_allocator());
        EXPECT_EQ(lhs.get_allocator().id, lhs_alloc_id);
        EXPECT_EQ(rhs.get_allocator().id, rhs_alloc_id);

        lhs.swap(rhs);

        // After swap with propagate_on_container_swap=true,
        // allocators should be swapped along with the objects
        EXPECT_EQ(*lhs, T(7, 8, 9));
        EXPECT_EQ(*rhs, T(1, 2, 3));
        EXPECT_EQ(lhs.get_allocator().id, rhs_alloc_id);
        EXPECT_EQ(rhs.get_allocator().id, lhs_alloc_id);
    }

    ASSERT_NO_LEAKS(alloc1);
    ASSERT_NO_LEAKS(alloc2);
}

TEST(IndirectTest, SwapWithPropagateOnSwapFalse) {
    // Test that allocators are NOT swapped when propagate_on_container_swap is false
    // In this case, the allocators must be equal (same allocator)
    using T        = Composite;
    using AllocCtl = CountingAllocatorControl<T, PROPAGATE_NONE>;
    using Alloc    = AllocCtl::allocator_type;

    static_assert(!std::allocator_traits<Alloc>::propagate_on_container_swap::value);

    Alloc::Id_type lhs_alloc_id = 100, rhs_alloc_id = 200;

    AllocCtl alloc1(lhs_alloc_id);
    AllocCtl alloc2(rhs_alloc_id);

    {
        indirect<T, Alloc> lhs(std::allocator_arg, alloc1.handle(), std::in_place, 1, 2, 3);
        indirect<T, Alloc> rhs(std::allocator_arg, alloc2.handle(), std::in_place, 7, 8, 9);

        // Both use same allocator
        EXPECT_NE(lhs.get_allocator(), rhs.get_allocator());
        EXPECT_EQ(lhs.get_allocator().id, lhs_alloc_id);
        EXPECT_EQ(rhs.get_allocator().id, rhs_alloc_id);

        lhs.swap(rhs);

        // After swap with propagate_on_container_swap=false,
        // allocators should remain unchanged
        EXPECT_EQ(*lhs, T(7, 8, 9));
        EXPECT_EQ(*rhs, T(1, 2, 3));
        EXPECT_EQ(lhs.get_allocator().id, lhs_alloc_id);
        EXPECT_EQ(rhs.get_allocator().id, rhs_alloc_id);
    }

    ASSERT_NO_LEAKS(alloc1);
    ASSERT_NO_LEAKS(alloc2);
}

TEST(IndirectTest, SwapSelfSwap) {
    // Test swapping an object with itself
    using T = Composite;
    indirect<T> instance(std::in_place, 10, 20, 30);

    instance.swap(instance);

    EXPECT_EQ(*instance, T(10, 20, 30));
    EXPECT_FALSE(instance.valueless_after_move());
}

TEST(IndirectTest, SwapFreeFunction) {
    // Test the free function swap(lhs, rhs)
    using T = Composite;
    indirect<T> lhs(std::in_place, 10, 20, 30);
    indirect<T> rhs(std::in_place, 40, 50, 60);

    using std::swap;
    swap(lhs, rhs);

    EXPECT_EQ(*lhs, T(40, 50, 60));
    EXPECT_EQ(*rhs, T(10, 20, 30));
    EXPECT_FALSE(lhs.valueless_after_move());
    EXPECT_FALSE(rhs.valueless_after_move());
}

TEST(IndirectTest, SwapNoAllocation) {
    // Test that swap does not allocate or deallocate memory
    using T = Composite;
    CountingAllocatorControl<T> alloc;

    {
        indirect<T> lhs(std::allocator_arg, alloc.handle(), std::in_place, 1, 2, 3);
        indirect<T> rhs(std::allocator_arg, alloc.handle(), std::in_place, 7, 8, 9);

        lhs.swap(rhs);

        // Swap should not allocate or deallocate
        EXPECT_EQ(alloc.num_allocated, 2);
        EXPECT_EQ(alloc.num_deallocated, 0);
        EXPECT_EQ(*lhs, T(7, 8, 9));
        EXPECT_EQ(*rhs, T(1, 2, 3));
    }

    ASSERT_NO_LEAKS(alloc);
}
