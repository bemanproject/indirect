// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <beman/indirect/indirect.hpp>

#include "test_helpers.hpp"

#include <gtest/gtest.h>

#include <array>
#include <compare>
#include <functional>
#include <map>
#include <memory>
#include <memory_resource>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using beman::indirect::indirect;

// --- Construction ---

TEST(IndirectTest, DefaultConstruction) {
    indirect<int> i;
    EXPECT_FALSE(i.valueless_after_move());
    EXPECT_EQ(*i, 0);
}

TEST(IndirectTest, DefaultConstructionString) {
    indirect<std::string> i;
    EXPECT_EQ(*i, "");
}

TEST(IndirectTest, ForwardingConstruction) {
    indirect<std::string> i("hello");
    EXPECT_EQ(*i, "hello");
}

TEST(IndirectTest, InPlaceConstruction) {
    indirect<std::string> i(std::in_place, 5, 'x');
    EXPECT_EQ(*i, "xxxxx");
}

TEST(IndirectTest, InPlaceConstructionInitializerList) {
    indirect<std::vector<int>> i(std::in_place, {1, 2, 3});
    EXPECT_EQ((*i).size(), 3u);
    EXPECT_EQ((*i)[0], 1);
    EXPECT_EQ((*i)[2], 3);
}

// --- Copy/Move ---

TEST(IndirectTest, CopyConstruction) {
    indirect<std::string> i("hello");
    indirect<std::string> j(i);
    EXPECT_EQ(*j, "hello");
    // Deep copy: modifying copy doesn't affect original
    *j = "world";
    EXPECT_EQ(*i, "hello");
    EXPECT_EQ(*j, "world");
}

TEST(IndirectTest, MoveConstruction) {
    indirect<std::string> i("hello");
    indirect<std::string> j(std::move(i));
    EXPECT_EQ(*j, "hello");
    EXPECT_TRUE(i.valueless_after_move());
}

TEST(IndirectTest, CopyAssignment) {
    indirect<std::string> i("hello");
    indirect<std::string> j("world");
    j = i;
    EXPECT_EQ(*j, "hello");
    *j = "changed";
    EXPECT_EQ(*i, "hello");
}

TEST(IndirectTest, MoveAssignment) {
    indirect<std::string> i("hello");
    indirect<std::string> j("world");
    j = std::move(i);
    EXPECT_EQ(*j, "hello");
    EXPECT_TRUE(i.valueless_after_move());
}

TEST(IndirectTest, SelfCopyAssignment) {
    indirect<int> i(42);
    auto&         ref = i;
    i                 = ref;
    EXPECT_EQ(*i, 42);
}

TEST(IndirectTest, SelfMoveAssignment) {
    indirect<int> i(42);
    auto&         ref = i;
    i                 = std::move(ref);
    // Self-move: no effects per spec
    EXPECT_EQ(*i, 42);
}

TEST(IndirectTest, ForwardingAssignment) {
    indirect<std::string> i("hello");
    i = std::string("world");
    EXPECT_EQ(*i, "world");
}

TEST(IndirectTest, ForwardingAssignmentToValueless) {
    indirect<std::string> i("hello");
    indirect<std::string> j(std::move(i));
    EXPECT_TRUE(i.valueless_after_move());
    i = std::string("recovered");
    EXPECT_FALSE(i.valueless_after_move());
    EXPECT_EQ(*i, "recovered");
}

// --- Observers ---

TEST(IndirectTest, DereferenceConst) {
    const indirect<int> i(42);
    EXPECT_EQ(*i, 42);
    static_assert(std::is_same_v<decltype(*i), const int&>);
}

TEST(IndirectTest, DereferenceNonConst) {
    indirect<int> i(42);
    *i = 100;
    EXPECT_EQ(*i, 100);
    static_assert(std::is_same_v<decltype(*i), int&>);
}

TEST(IndirectTest, DereferenceRvalue) {
    indirect<std::string> i("hello");
    std::string           s = *std::move(i);
    EXPECT_EQ(s, "hello");
}

TEST(IndirectTest, DereferenceConstRvalue) {
    const indirect<std::string> i("hello");
    auto&&                      ref = *std::move(i);
    static_assert(std::is_same_v<decltype(ref), const std::string&&>);
    EXPECT_EQ(ref, "hello");
}

struct Foo {
    int x = 42;
};

TEST(IndirectTest, ArrowOperator) {
    indirect<Foo> i;
    EXPECT_EQ(i->x, 42);
    i->x = 100;
    EXPECT_EQ(i->x, 100);
}

TEST(IndirectTest, ArrowOperatorConst) {
    const indirect<Foo> i;
    EXPECT_EQ(i->x, 42);
}

TEST(IndirectTest, ValuelessAfterMove) {
    indirect<int> i(42);
    EXPECT_FALSE(i.valueless_after_move());
    indirect<int> j(std::move(i));
    EXPECT_TRUE(i.valueless_after_move());
}

TEST(IndirectTest, GetAllocator) {
    indirect<int>         i;
    [[maybe_unused]] auto alloc = i.get_allocator();
    static_assert(std::is_same_v<decltype(alloc), std::allocator<int>>);
}

TEST(IndirectTest, ConstPropagation) {
    indirect<int>        i(42);
    const indirect<int>& ci = i;
    static_assert(std::is_same_v<decltype(*ci), const int&>);
    static_assert(std::is_same_v<decltype(*i), int&>);
    static_assert(
        std::is_same_v<decltype(ci.operator->()), std::allocator_traits<std::allocator<int>>::const_pointer>);
    static_assert(std::is_same_v<decltype(i.operator->()), std::allocator_traits<std::allocator<int>>::pointer>);
}

// --- Swap ---

TEST(IndirectTest, MemberSwap) {
    indirect<int> a(1);
    indirect<int> b(2);
    a.swap(b);
    EXPECT_EQ(*a, 2);
    EXPECT_EQ(*b, 1);
}

TEST(IndirectTest, FreeSwap) {
    indirect<int> a(1);
    indirect<int> b(2);
    swap(a, b);
    EXPECT_EQ(*a, 2);
    EXPECT_EQ(*b, 1);
}

TEST(IndirectTest, SwapWithValueless) {
    indirect<int> a(42);
    indirect<int> b(0);
    indirect<int> c(std::move(b));
    EXPECT_TRUE(b.valueless_after_move());
    a.swap(b);
    EXPECT_TRUE(a.valueless_after_move());
    EXPECT_EQ(*b, 42);
}

TEST(IndirectTest, MemberSwapWithSelf) {
    indirect<int> a(42);
    a.swap(a);
    EXPECT_EQ(*a, 42);
}

TEST(IndirectTest, SwapBothValueless) {
    indirect<int> a(1);
    indirect<int> b(2);
    indirect<int> c(std::move(a));
    indirect<int> d(std::move(b));
    EXPECT_TRUE(a.valueless_after_move());
    EXPECT_TRUE(b.valueless_after_move());
    a.swap(b);
    EXPECT_TRUE(a.valueless_after_move());
    EXPECT_TRUE(b.valueless_after_move());
}

// --- Comparisons ---

TEST(IndirectTest, Equality) {
    indirect<int> a(1);
    indirect<int> b(1);
    indirect<int> c(2);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST(IndirectTest, EqualityValueless) {
    indirect<int> a(1);
    indirect<int> b(std::move(a));
    indirect<int> c(2);
    indirect<int> d(std::move(c));
    // Both valueless
    EXPECT_TRUE(a == c);
    // One valueless
    EXPECT_FALSE(a == b);
    EXPECT_FALSE(b == d);
}

TEST(IndirectTest, ThreeWayComparison) {
    indirect<int> a(1);
    indirect<int> b(2);
    EXPECT_TRUE(a < b);
    EXPECT_TRUE(b > a);
    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(a != b);
}

TEST(IndirectTest, ThreeWayValueless) {
    indirect<int> a(1);
    indirect<int> b(std::move(a));
    indirect<int> c(2);
    // Valueless sorts before non-valueless
    EXPECT_TRUE(a < c);
    EXPECT_TRUE(c > a);
}

TEST(IndirectTest, ComparisonWithValue) {
    indirect<int> a(42);
    EXPECT_TRUE(a == 42);
    EXPECT_FALSE(a == 0);
    EXPECT_TRUE(a > 0);
    EXPECT_TRUE(a < 100);
}

TEST(IndirectTest, ComparisonWithValueValueless) {
    indirect<int> a(1);
    indirect<int> b(std::move(a));
    EXPECT_FALSE(a == 42);
    EXPECT_TRUE(a < 0); // valueless is less than any value
}

// Non-three-way-comparable type: only has == and <
struct NonThreeWay {
    int  v;
    bool operator==(const NonThreeWay& o) const { return v == o.v; }
    bool operator<(const NonThreeWay& o) const { return v < o.v; }
};

TEST(IndirectTest, NonThreeWayComparableType) {
    indirect<NonThreeWay> a(NonThreeWay{1});
    indirect<NonThreeWay> b(NonThreeWay{2});
    indirect<NonThreeWay> c(NonThreeWay{1});
    EXPECT_TRUE(a == c);
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a < b);
    EXPECT_FALSE(b < a);
}

// Cross-type comparison (indirect<int> vs indirect<double>)
TEST(IndirectTest, CrossTypeComparison) {
    indirect<int>    a(1);
    indirect<double> b(1.0);
    indirect<double> c(2.0);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_TRUE(a < c);
    EXPECT_TRUE(c > a);
}

// --- Hash ---

TEST(IndirectTest, Hash) {
    indirect<int>            a(42);
    std::hash<indirect<int>> h;
    EXPECT_EQ(h(a), std::hash<int>{}(42));
}

TEST(IndirectTest, HashConsistency) {
    indirect<int>            a(42);
    indirect<int>            b(42);
    std::hash<indirect<int>> h;
    EXPECT_EQ(h(a), h(b));
}

TEST(IndirectTest, HashValueless) {
    indirect<int>            a(1);
    indirect<int>            b(std::move(a));
    std::hash<indirect<int>> h;
    EXPECT_EQ(h(a), static_cast<std::size_t>(-1));
}

// --- Deduction guides ---

TEST(IndirectTest, DeductionGuideValue) {
    indirect i(42);
    static_assert(std::is_same_v<decltype(i), indirect<int>>);
    EXPECT_EQ(*i, 42);
}

TEST(IndirectTest, DeductionGuideAllocator) {
    std::allocator<int> alloc;
    indirect            i(std::allocator_arg, alloc, 42);
    static_assert(std::is_same_v<decltype(i), indirect<int, std::allocator<int>>>);
    EXPECT_EQ(*i, 42);
}

// --- Allocator-extended constructors ---

TEST(IndirectTest, AllocatorExtendedDefault) {
    std::allocator<int> alloc;
    indirect<int>       i(std::allocator_arg, alloc);
    EXPECT_EQ(*i, 0);
}

TEST(IndirectTest, AllocatorExtendedCopy) {
    indirect<std::string>       a("hello");
    std::allocator<std::string> alloc;
    indirect<std::string>       b(std::allocator_arg, alloc, a);
    EXPECT_EQ(*b, "hello");
}

TEST(IndirectTest, AllocatorExtendedMove) {
    indirect<std::string>       a("hello");
    std::allocator<std::string> alloc;
    indirect<std::string>       b(std::allocator_arg, alloc, std::move(a));
    EXPECT_EQ(*b, "hello");
    EXPECT_TRUE(a.valueless_after_move());
}

TEST(IndirectTest, AllocatorExtendedForwarding) {
    std::allocator<std::string> alloc;
    indirect<std::string>       i(std::allocator_arg, alloc, "hello");
    EXPECT_EQ(*i, "hello");
}

TEST(IndirectTest, AllocatorExtendedInPlace) {
    std::allocator<std::string> alloc;
    indirect<std::string>       i(std::allocator_arg, alloc, std::in_place, 3, 'a');
    EXPECT_EQ(*i, "aaa");
}

TEST(IndirectTest, AllocatorExtendedInPlaceInitList) {
    std::allocator<std::vector<int>> alloc;
    indirect<std::vector<int>>       i(std::allocator_arg, alloc, std::in_place, {1, 2, 3});
    EXPECT_EQ((*i).size(), 3u);
}

// --- Copy/Move from valueless ---

TEST(IndirectTest, CopyFromValueless) {
    indirect<int> a(1);
    indirect<int> b(std::move(a));
    indirect<int> c(a);
    EXPECT_TRUE(c.valueless_after_move());
}

TEST(IndirectTest, CopyAssignFromValueless) {
    indirect<int> a(1);
    indirect<int> b(std::move(a));
    indirect<int> c(42);
    c = a;
    EXPECT_TRUE(c.valueless_after_move());
}

TEST(IndirectTest, MoveFromValueless) {
    indirect<int> a(1);
    indirect<int> b(std::move(a));
    EXPECT_TRUE(a.valueless_after_move());
    indirect<int> c(std::move(a));
    EXPECT_TRUE(c.valueless_after_move());
    EXPECT_TRUE(a.valueless_after_move());
}

TEST(IndirectTest, MoveAssignFromValueless) {
    indirect<int> a(1);
    indirect<int> b(std::move(a));
    EXPECT_TRUE(a.valueless_after_move());
    indirect<int> c(42);
    c = std::move(a);
    EXPECT_TRUE(c.valueless_after_move());
}

TEST(IndirectTest, MovePreservesPointerAddress) {
    indirect<int> a(42);
    auto*         addr = &*a;
    indirect<int> b(std::move(a));
    EXPECT_EQ(&*b, addr);
}

// --- Allocation tracking ---

TEST(IndirectTest, CountAllocationsForDefaultConstruction) {
    unsigned alloc_counter   = 0;
    unsigned dealloc_counter = 0;
    {
        indirect<int, test::TrackingAllocator<int>> i(std::allocator_arg,
                                                      test::TrackingAllocator<int>(&alloc_counter, &dealloc_counter));
        EXPECT_EQ(alloc_counter, 1u);
        EXPECT_EQ(dealloc_counter, 0u);
    }
    EXPECT_EQ(alloc_counter, 1u);
    EXPECT_EQ(dealloc_counter, 1u);
}

TEST(IndirectTest, CountAllocationsForForwardingConstruction) {
    unsigned alloc_counter   = 0;
    unsigned dealloc_counter = 0;
    {
        indirect<int, test::TrackingAllocator<int>> i(
            std::allocator_arg, test::TrackingAllocator<int>(&alloc_counter, &dealloc_counter), 42);
        EXPECT_EQ(alloc_counter, 1u);
        EXPECT_EQ(dealloc_counter, 0u);
    }
    EXPECT_EQ(alloc_counter, 1u);
    EXPECT_EQ(dealloc_counter, 1u);
}

TEST(IndirectTest, CountAllocationsForInPlaceConstruction) {
    unsigned alloc_counter   = 0;
    unsigned dealloc_counter = 0;
    {
        indirect<int, test::TrackingAllocator<int>> i(
            std::allocator_arg, test::TrackingAllocator<int>(&alloc_counter, &dealloc_counter), std::in_place, 42);
        EXPECT_EQ(alloc_counter, 1u);
        EXPECT_EQ(dealloc_counter, 0u);
    }
    EXPECT_EQ(alloc_counter, 1u);
    EXPECT_EQ(dealloc_counter, 1u);
}

TEST(IndirectTest, CountAllocationsForCopyConstruction) {
    unsigned alloc_counter   = 0;
    unsigned dealloc_counter = 0;
    {
        indirect<int, test::TrackingAllocator<int>> i(
            std::allocator_arg, test::TrackingAllocator<int>(&alloc_counter, &dealloc_counter), 42);
        EXPECT_EQ(alloc_counter, 1u);
        auto j = i;
        EXPECT_EQ(alloc_counter, 2u);
        EXPECT_EQ(dealloc_counter, 0u);
    }
    EXPECT_EQ(dealloc_counter, 2u);
}

TEST(IndirectTest, CountAllocationsForMoveConstruction) {
    unsigned alloc_counter   = 0;
    unsigned dealloc_counter = 0;
    {
        indirect<int, test::TrackingAllocator<int>> i(
            std::allocator_arg, test::TrackingAllocator<int>(&alloc_counter, &dealloc_counter), 42);
        EXPECT_EQ(alloc_counter, 1u);
        auto j = std::move(i);
        // Move should not allocate
        EXPECT_EQ(alloc_counter, 1u);
        EXPECT_EQ(dealloc_counter, 0u);
    }
    EXPECT_EQ(dealloc_counter, 1u);
}

TEST(IndirectTest, CountAllocationsForCopyAssignment) {
    unsigned alloc_counter   = 0;
    unsigned dealloc_counter = 0;
    {
        test::TrackingAllocator<int>                alloc(&alloc_counter, &dealloc_counter);
        indirect<int, test::TrackingAllocator<int>> i(std::allocator_arg, alloc, 42);
        indirect<int, test::TrackingAllocator<int>> j(std::allocator_arg, alloc, 0);
        EXPECT_EQ(alloc_counter, 2u);
        j = i;
        // Copy assign with equal allocators: assigns through, no new allocation
        EXPECT_EQ(alloc_counter, 2u);
    }
    EXPECT_EQ(dealloc_counter, 2u);
}

TEST(IndirectTest, CountAllocationsForMoveAssignment) {
    unsigned alloc_counter   = 0;
    unsigned dealloc_counter = 0;
    {
        test::TrackingAllocator<int>                alloc(&alloc_counter, &dealloc_counter);
        indirect<int, test::TrackingAllocator<int>> i(std::allocator_arg, alloc, 42);
        indirect<int, test::TrackingAllocator<int>> j(std::allocator_arg, alloc, 0);
        EXPECT_EQ(alloc_counter, 2u);
        j = std::move(i);
        // Move assign with equal allocators: takes ownership, deallocates old
        EXPECT_EQ(alloc_counter, 2u);
        EXPECT_EQ(dealloc_counter, 1u);
    }
    EXPECT_EQ(dealloc_counter, 2u);
}

// --- Exception safety ---

TEST(IndirectTest, ConstructorExceptionCleansUpAllocation) {
    unsigned alloc_counter   = 0;
    unsigned dealloc_counter = 0;
    auto     construct       = [&]() {
        return indirect<test::ThrowsOnConstruction, test::TrackingAllocator<test::ThrowsOnConstruction>>(
            std::allocator_arg, test::TrackingAllocator<test::ThrowsOnConstruction>(&alloc_counter, &dealloc_counter));
    };
    EXPECT_THROW(construct(), test::ThrowsOnConstruction::Exception);
    EXPECT_EQ(alloc_counter, 1u);
    EXPECT_EQ(dealloc_counter, 1u);
}

TEST(IndirectTest, CopyConstructionExceptionSafety) {
    unsigned                                    alloc_counter   = 0;
    unsigned                                    dealloc_counter = 0;
    test::TrackingAllocator<test::ThrowsOnCopy> alloc(&alloc_counter, &dealloc_counter);
    // Construct via move (doesn't throw)
    indirect<test::ThrowsOnCopy, test::TrackingAllocator<test::ThrowsOnCopy>> i(
        std::allocator_arg, alloc, test::ThrowsOnCopy(42));
    EXPECT_EQ(alloc_counter, 1u);
    // Copy construction should throw and clean up
    EXPECT_THROW(auto j = i, test::ThrowsOnCopy::Exception);
    EXPECT_EQ(alloc_counter, 2u);
    EXPECT_EQ(dealloc_counter, 1u); // failed copy allocation cleaned up
}

// --- Non-equal allocator tests ---

TEST(IndirectTest, MoveConstructionWithNonEqualAllocator) {
    unsigned alloc_counter1   = 0;
    unsigned dealloc_counter1 = 0;
    unsigned alloc_counter2   = 0;
    unsigned dealloc_counter2 = 0;

    test::NonEqualTrackingAllocator<int> alloc1(&alloc_counter1, &dealloc_counter1);
    test::NonEqualTrackingAllocator<int> alloc2(&alloc_counter2, &dealloc_counter2);

    indirect<int, test::NonEqualTrackingAllocator<int>> i(std::allocator_arg, alloc1, 42);
    EXPECT_EQ(alloc_counter1, 1u);

    // Allocator-extended move with different allocator: allocators never equal, must copy
    indirect<int, test::NonEqualTrackingAllocator<int>> j(std::allocator_arg, alloc2, std::move(i));
    EXPECT_EQ(alloc_counter2, 1u); // new allocation in alloc2
    EXPECT_EQ(*j, 42);
}

TEST(IndirectTest, MoveAssignmentWithNonEqualAllocator) {
    unsigned alloc_counter1   = 0;
    unsigned dealloc_counter1 = 0;
    unsigned alloc_counter2   = 0;
    unsigned dealloc_counter2 = 0;

    test::NonEqualTrackingAllocator<int> alloc1(&alloc_counter1, &dealloc_counter1);
    test::NonEqualTrackingAllocator<int> alloc2(&alloc_counter2, &dealloc_counter2);

    indirect<int, test::NonEqualTrackingAllocator<int>> i(std::allocator_arg, alloc1, 42);
    indirect<int, test::NonEqualTrackingAllocator<int>> j(std::allocator_arg, alloc2, 0);

    // NonEqualTrackingAllocator propagates on move assignment,
    // so this should do a pointer steal + propagate allocator
    j = std::move(i);
    EXPECT_EQ(*j, 42);
    EXPECT_TRUE(i.valueless_after_move());
}

// --- Tagged allocator tests ---

TEST(IndirectTest, TaggedAllocatorGetAllocator) {
    test::TaggedAllocator<int>                alloc(42);
    indirect<int, test::TaggedAllocator<int>> i(std::allocator_arg, alloc, 7);
    EXPECT_EQ(i.get_allocator().tag, 42u);
}

TEST(IndirectTest, TaggedAllocatorCopyUsesSelectOnContainerCopyConstruction) {
    test::TaggedAllocator<int>                alloc(42);
    indirect<int, test::TaggedAllocator<int>> i(std::allocator_arg, alloc, 7);
    auto                                      j = i;
    // select_on_container_copy_construction for default allocator just copies the allocator
    EXPECT_EQ(j.get_allocator().tag, 42u);
    EXPECT_EQ(*j, 7);
}

// --- Container integration ---

TEST(IndirectTest, InteractionWithOptional) {
    std::optional<indirect<int>> opt;
    EXPECT_FALSE(opt.has_value());
    opt = indirect<int>(42);
    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(**opt, 42);
}

TEST(IndirectTest, InteractionWithVector) {
    std::vector<indirect<int>> v;
    v.push_back(indirect<int>(1));
    v.push_back(indirect<int>(2));
    v.push_back(indirect<int>(3));
    EXPECT_EQ(*v[0], 1);
    EXPECT_EQ(*v[1], 2);
    EXPECT_EQ(*v[2], 3);

    // Copy the vector
    auto v2 = v;
    EXPECT_EQ(*v2[0], 1);
    *v2[0] = 100;
    EXPECT_EQ(*v[0], 1); // original unchanged
}

TEST(IndirectTest, InteractionWithMap) {
    std::map<int, indirect<std::string>> m;
    m.emplace(1, indirect<std::string>("one"));
    m.emplace(2, indirect<std::string>("two"));
    EXPECT_EQ(*m.at(1), "one");
    EXPECT_EQ(*m.at(2), "two");
}

TEST(IndirectTest, InteractionWithUnorderedMap) {
    std::unordered_map<int, indirect<std::string>> m;
    m.emplace(1, indirect<std::string>("one"));
    m.emplace(2, indirect<std::string>("two"));
    EXPECT_EQ(*m.at(1), "one");
    EXPECT_EQ(*m.at(2), "two");
}

// --- Swap with allocator propagation ---

TEST(IndirectTest, SwapWithPOCSAllocator) {
    unsigned alloc_counter1   = 0;
    unsigned dealloc_counter1 = 0;
    unsigned alloc_counter2   = 0;
    unsigned dealloc_counter2 = 0;

    test::POCSTrackingAllocator<int> alloc1(&alloc_counter1, &dealloc_counter1);
    test::POCSTrackingAllocator<int> alloc2(&alloc_counter2, &dealloc_counter2);

    indirect<int, test::POCSTrackingAllocator<int>> a(std::allocator_arg, alloc1, 1);
    indirect<int, test::POCSTrackingAllocator<int>> b(std::allocator_arg, alloc2, 2);

    a.swap(b);
    EXPECT_EQ(*a, 2);
    EXPECT_EQ(*b, 1);
    // Allocators should have been swapped (POCS = true)
}

// --- PMR alias ---

TEST(IndirectTest, PmrAlias) {
    std::array<std::byte, 256>          buffer{};
    std::pmr::monotonic_buffer_resource resource(buffer.data(), buffer.size());
    beman::indirect::pmr::indirect<int> i(std::allocator_arg, std::pmr::polymorphic_allocator<int>(&resource), 42);
    EXPECT_EQ(*i, 42);
}

TEST(IndirectTest, PmrInVector) {
    std::array<std::byte, 4096>                                          buffer{};
    std::pmr::monotonic_buffer_resource                                  resource(buffer.data(), buffer.size());
    std::pmr::polymorphic_allocator<beman::indirect::pmr::indirect<int>> alloc(&resource);

    std::pmr::vector<beman::indirect::pmr::indirect<int>> v(alloc);
    v.push_back(
        beman::indirect::pmr::indirect<int>(std::allocator_arg, std::pmr::polymorphic_allocator<int>(&resource), 1));
    v.push_back(
        beman::indirect::pmr::indirect<int>(std::allocator_arg, std::pmr::polymorphic_allocator<int>(&resource), 2));
    EXPECT_EQ(*v[0], 1);
    EXPECT_EQ(*v[1], 2);
}

TEST(IndirectTest, PmrVectorPropagatesAllocatorToIndirectElements) {
    std::array<std::byte, 4096>                                          buffer{};
    std::pmr::monotonic_buffer_resource                                  resource(buffer.data(), buffer.size());
    std::pmr::polymorphic_allocator<beman::indirect::pmr::indirect<int>> alloc(&resource);

    std::pmr::vector<beman::indirect::pmr::indirect<int>> v(alloc);
    v.emplace_back(42);
    v.emplace_back(99);

    // The vector's allocator should propagate to the indirect elements.
    EXPECT_EQ(v[0].get_allocator().resource(), &resource);
    EXPECT_EQ(v[1].get_allocator().resource(), &resource);
    EXPECT_EQ(*v[0], 42);
    EXPECT_EQ(*v[1], 99);
}

TEST(IndirectTest, PmrIndirectPropagatesAllocatorToInnerVector) {
    std::array<std::byte, 4096>                            buffer{};
    std::pmr::monotonic_buffer_resource                    resource(buffer.data(), buffer.size());
    std::pmr::polymorphic_allocator<std::pmr::vector<int>> alloc(&resource);

    beman::indirect::pmr::indirect<std::pmr::vector<int>> i(
        std::allocator_arg, alloc, std::in_place, std::initializer_list<int>{1, 2, 3});

    // The indirect's allocator should propagate to the vector inside.
    EXPECT_EQ((*i).get_allocator().resource(), &resource);
    EXPECT_EQ((*i).size(), 3u);
    EXPECT_EQ((*i)[0], 1);
    EXPECT_EQ((*i)[2], 3);
}

} // namespace
