// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <beman/indirect/polymorphic.hpp>

#include "test_helpers.hpp"

#include <gtest/gtest.h>

#include <array>
#include <map>
#include <memory>
#include <memory_resource>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using beman::indirect::polymorphic;

// Test hierarchy
struct Base {
    virtual ~Base()                   = default;
    virtual int         value() const = 0;
    virtual std::string name() const  = 0;
    Base()                            = default;
    Base(const Base&)                 = default;
    Base(Base&&)                      = default;
    Base& operator=(const Base&)      = default;
    Base& operator=(Base&&)           = default;
};

struct Derived : Base {
    int x_;
    explicit Derived(int x = 0) : x_(x) {}
    int         value() const override { return x_; }
    std::string name() const override { return "Derived"; }
};

struct Derived2 : Base {
    std::string s_;
    explicit Derived2(std::string s = "") : s_(std::move(s)) {}
    int         value() const override { return static_cast<int>(s_.size()); }
    std::string name() const override { return "Derived2:" + s_; }
};

// Non-abstract base for default construction test
struct SimpleBase {
    virtual ~SimpleBase() = default;
    virtual int value() const { return 0; }
    SimpleBase()                             = default;
    SimpleBase(const SimpleBase&)            = default;
    SimpleBase(SimpleBase&&)                 = default;
    SimpleBase& operator=(const SimpleBase&) = default;
    SimpleBase& operator=(SimpleBase&&)      = default;
};

struct SimpleDerived : SimpleBase {
    int x_;
    explicit SimpleDerived(int x = 0) : x_(x) {}
    int value() const override { return x_; }
};

// --- Construction ---

TEST(PolymorphicTest, DefaultConstruction) {
    polymorphic<SimpleBase> p;
    EXPECT_FALSE(p.valueless_after_move());
    EXPECT_EQ((*p).value(), 0);
}

TEST(PolymorphicTest, ConstructionFromDerived) {
    polymorphic<Base> p(Derived(42));
    EXPECT_EQ((*p).value(), 42);
    EXPECT_EQ((*p).name(), "Derived");
}

TEST(PolymorphicTest, ConstructionFromDerived2) {
    polymorphic<Base> p(Derived2("hello"));
    EXPECT_EQ((*p).value(), 5);
    EXPECT_EQ((*p).name(), "Derived2:hello");
}

TEST(PolymorphicTest, InPlaceTypeConstruction) {
    polymorphic<Base> p(std::in_place_type<Derived>, 99);
    EXPECT_EQ((*p).value(), 99);
    EXPECT_EQ((*p).name(), "Derived");
}

TEST(PolymorphicTest, InPlaceTypeConstructionDerived2) {
    polymorphic<Base> p(std::in_place_type<Derived2>, "world");
    EXPECT_EQ((*p).value(), 5);
    EXPECT_EQ((*p).name(), "Derived2:world");
}

struct WithInitList : SimpleBase {
    std::vector<int> data_;
    WithInitList(std::initializer_list<int> il, int extra = 0) : data_(il) {
        if (extra != 0)
            data_.push_back(extra);
    }
    int value() const override { return static_cast<int>(data_.size()); }
};

TEST(PolymorphicTest, InPlaceTypeConstructionInitializerList) {
    polymorphic<SimpleBase> p(std::in_place_type<WithInitList>, {1, 2, 3});
    EXPECT_EQ((*p).value(), 3);
}

TEST(PolymorphicTest, InPlaceTypeConstructionInitializerListWithArgs) {
    polymorphic<SimpleBase> p(std::in_place_type<WithInitList>, {1, 2, 3}, 4);
    EXPECT_EQ((*p).value(), 4);
}

// --- Copy/Move ---

TEST(PolymorphicTest, CopyConstructionPreservesDynamicType) {
    polymorphic<Base> p(Derived(42));
    polymorphic<Base> q(p);
    EXPECT_EQ((*q).value(), 42);
    EXPECT_EQ((*q).name(), "Derived");
    EXPECT_EQ((*p).value(), 42);
}

TEST(PolymorphicTest, CopyConstructionPreservesDerived2) {
    polymorphic<Base> p(Derived2("test"));
    polymorphic<Base> q(p);
    EXPECT_EQ((*q).name(), "Derived2:test");
}

TEST(PolymorphicTest, CopiesOfDerivedObjectsAreDistinct) {
    // Verify copies are truly independent by checking address difference
    polymorphic<Base> p(Derived(42));
    polymorphic<Base> q(p);
    EXPECT_NE(&*p, &*q);
    EXPECT_EQ((*p).value(), 42);
    EXPECT_EQ((*q).value(), 42);
}

TEST(PolymorphicTest, MoveConstruction) {
    polymorphic<Base> p(Derived(42));
    polymorphic<Base> q(std::move(p));
    EXPECT_EQ((*q).value(), 42);
    EXPECT_TRUE(p.valueless_after_move());
}

TEST(PolymorphicTest, CopyAssignment) {
    polymorphic<Base> p(Derived(42));
    polymorphic<Base> q(Derived2("hello"));
    q = p;
    EXPECT_EQ((*q).value(), 42);
    EXPECT_EQ((*q).name(), "Derived");
    EXPECT_EQ((*p).value(), 42); // original unaffected
}

TEST(PolymorphicTest, MoveAssignment) {
    polymorphic<Base> p(Derived(42));
    polymorphic<Base> q(Derived2("hello"));
    q = std::move(p);
    EXPECT_EQ((*q).value(), 42);
    EXPECT_TRUE(p.valueless_after_move());
}

TEST(PolymorphicTest, SelfCopyAssignment) {
    polymorphic<Base> p(Derived(42));
    auto&             ref = p;
    p                     = ref;
    EXPECT_EQ((*p).value(), 42);
}

TEST(PolymorphicTest, SelfMoveAssignment) {
    polymorphic<Base> p(Derived(42));
    auto&             ref = p;
    p                     = std::move(ref);
    EXPECT_EQ((*p).value(), 42);
}

// --- Valueless operations ---

TEST(PolymorphicTest, MoveFromValueless) {
    polymorphic<Base> a(Derived(1));
    polymorphic<Base> b(std::move(a));
    EXPECT_TRUE(a.valueless_after_move());
    polymorphic<Base> c(std::move(a));
    EXPECT_TRUE(c.valueless_after_move());
    EXPECT_TRUE(a.valueless_after_move());
}

TEST(PolymorphicTest, MoveAssignFromValueless) {
    polymorphic<Base> a(Derived(1));
    polymorphic<Base> b(std::move(a));
    EXPECT_TRUE(a.valueless_after_move());
    polymorphic<Base> c(Derived(42));
    c = std::move(a);
    EXPECT_TRUE(c.valueless_after_move());
}

TEST(PolymorphicTest, CopyAssignToValueless) {
    polymorphic<Base> a(Derived(42));
    polymorphic<Base> b(Derived(0));
    polymorphic<Base> c(std::move(b));
    EXPECT_TRUE(b.valueless_after_move());
    b = a;
    EXPECT_FALSE(b.valueless_after_move());
    EXPECT_EQ((*b).value(), 42);
}

TEST(PolymorphicTest, MoveAssignToValueless) {
    polymorphic<Base> a(Derived(42));
    polymorphic<Base> b(Derived(0));
    polymorphic<Base> c(std::move(b));
    EXPECT_TRUE(b.valueless_after_move());
    b = std::move(a);
    EXPECT_FALSE(b.valueless_after_move());
    EXPECT_EQ((*b).value(), 42);
}

// --- Observers ---

TEST(PolymorphicTest, DereferenceConst) {
    const polymorphic<Base> p(Derived(42));
    EXPECT_EQ((*p).value(), 42);
    static_assert(std::is_same_v<decltype(*p), const Base&>);
}

TEST(PolymorphicTest, DereferenceNonConst) {
    polymorphic<Base> p(Derived(42));
    static_assert(std::is_same_v<decltype(*p), Base&>);
}

TEST(PolymorphicTest, ArrowOperatorConst) {
    const polymorphic<Base> p(Derived(42));
    EXPECT_EQ(p->value(), 42);
}

TEST(PolymorphicTest, ArrowOperatorNonConst) {
    polymorphic<Base> p(Derived(42));
    EXPECT_EQ(p->value(), 42);
}

TEST(PolymorphicTest, ValuelessAfterMove) {
    polymorphic<Base> p(Derived(42));
    EXPECT_FALSE(p.valueless_after_move());
    polymorphic<Base> q(std::move(p));
    EXPECT_TRUE(p.valueless_after_move());
}

TEST(PolymorphicTest, GetAllocator) {
    polymorphic<Base>     p(Derived(1));
    [[maybe_unused]] auto alloc = p.get_allocator();
    static_assert(std::is_same_v<decltype(alloc), std::allocator<Base>>);
}

TEST(PolymorphicTest, ConstPropagation) {
    polymorphic<Base>        p(Derived(42));
    const polymorphic<Base>& cp = p;
    static_assert(std::is_same_v<decltype(*cp), const Base&>);
    static_assert(std::is_same_v<decltype(*p), Base&>);
}

// --- Swap ---

TEST(PolymorphicTest, MemberSwap) {
    polymorphic<Base> a(Derived(1));
    polymorphic<Base> b(Derived(2));
    a.swap(b);
    EXPECT_EQ((*a).value(), 2);
    EXPECT_EQ((*b).value(), 1);
}

TEST(PolymorphicTest, FreeSwap) {
    polymorphic<Base> a(Derived(1));
    polymorphic<Base> b(Derived(2));
    swap(a, b);
    EXPECT_EQ((*a).value(), 2);
    EXPECT_EQ((*b).value(), 1);
}

TEST(PolymorphicTest, SwapWithValueless) {
    polymorphic<Base> a(Derived(42));
    polymorphic<Base> b(Derived(0));
    polymorphic<Base> c(std::move(b));
    EXPECT_TRUE(b.valueless_after_move());
    a.swap(b);
    EXPECT_TRUE(a.valueless_after_move());
    EXPECT_EQ((*b).value(), 42);
}

TEST(PolymorphicTest, SwapFromValueless) {
    polymorphic<Base> a(Derived(0));
    polymorphic<Base> b(Derived(42));
    polymorphic<Base> c(std::move(a));
    EXPECT_TRUE(a.valueless_after_move());
    a.swap(b);
    EXPECT_EQ((*a).value(), 42);
    EXPECT_TRUE(b.valueless_after_move());
}

TEST(PolymorphicTest, MemberSwapWithSelf) {
    polymorphic<Base> a(Derived(42));
    a.swap(a);
    EXPECT_EQ((*a).value(), 42);
}

TEST(PolymorphicTest, SwapBothValueless) {
    polymorphic<Base> a(Derived(1));
    polymorphic<Base> b(Derived(2));
    polymorphic<Base> c(std::move(a));
    polymorphic<Base> d(std::move(b));
    EXPECT_TRUE(a.valueless_after_move());
    EXPECT_TRUE(b.valueless_after_move());
    a.swap(b);
    EXPECT_TRUE(a.valueless_after_move());
    EXPECT_TRUE(b.valueless_after_move());
}

// --- Derived type with additional data ---

struct DerivedWithData : Base {
    std::string extra_;
    int         x_;
    DerivedWithData(int x, std::string extra) : extra_(std::move(extra)), x_(x) {}
    int         value() const override { return x_; }
    std::string name() const override { return extra_; }
};

TEST(PolymorphicTest, DerivedWithAdditionalData) {
    polymorphic<Base> p(DerivedWithData(42, "custom"));
    EXPECT_EQ((*p).value(), 42);
    EXPECT_EQ((*p).name(), "custom");

    // Copy preserves derived data
    polymorphic<Base> q(p);
    EXPECT_EQ((*q).value(), 42);
    EXPECT_EQ((*q).name(), "custom");
}

// --- Multiple inheritance ---

struct Interface1 {
    virtual ~Interface1()                    = default;
    virtual int value1() const               = 0;
    Interface1()                             = default;
    Interface1(const Interface1&)            = default;
    Interface1(Interface1&&)                 = default;
    Interface1& operator=(const Interface1&) = default;
    Interface1& operator=(Interface1&&)      = default;
};

struct MultiDerived : Interface1 {
    int x_;
    explicit MultiDerived(int x = 0) : x_(x) {}
    int value1() const override { return x_; }
};

TEST(PolymorphicTest, MultipleInheritanceSupport) {
    polymorphic<Interface1> p(MultiDerived(42));
    EXPECT_EQ((*p).value1(), 42);

    auto q = p;
    EXPECT_EQ((*q).value1(), 42);
}

// --- Allocator-extended constructors ---

TEST(PolymorphicTest, AllocatorExtendedDefault) {
    std::allocator<SimpleBase> alloc;
    polymorphic<SimpleBase>    p(std::allocator_arg, alloc);
    EXPECT_EQ((*p).value(), 0);
}

TEST(PolymorphicTest, AllocatorExtendedFromDerived) {
    std::allocator<Base> alloc;
    polymorphic<Base>    p(std::allocator_arg, alloc, Derived(42));
    EXPECT_EQ((*p).value(), 42);
}

TEST(PolymorphicTest, AllocatorExtendedCopy) {
    polymorphic<Base>    a(Derived(42));
    std::allocator<Base> alloc;
    polymorphic<Base>    b(std::allocator_arg, alloc, a);
    EXPECT_EQ((*b).value(), 42);
    EXPECT_EQ((*b).name(), "Derived");
}

TEST(PolymorphicTest, AllocatorExtendedMove) {
    polymorphic<Base>    a(Derived(42));
    std::allocator<Base> alloc;
    polymorphic<Base>    b(std::allocator_arg, alloc, std::move(a));
    EXPECT_EQ((*b).value(), 42);
    EXPECT_TRUE(a.valueless_after_move());
}

TEST(PolymorphicTest, AllocatorExtendedInPlaceType) {
    std::allocator<Base> alloc;
    polymorphic<Base>    p(std::allocator_arg, alloc, std::in_place_type<Derived>, 99);
    EXPECT_EQ((*p).value(), 99);
}

TEST(PolymorphicTest, AllocatorExtendedInPlaceTypeInitList) {
    std::allocator<SimpleBase> alloc;
    polymorphic<SimpleBase>    p(std::allocator_arg, alloc, std::in_place_type<WithInitList>, {1, 2, 3});
    EXPECT_EQ((*p).value(), 3);
}

// --- Copy from valueless ---

TEST(PolymorphicTest, CopyFromValueless) {
    polymorphic<Base> a(Derived(1));
    polymorphic<Base> b(std::move(a));
    polymorphic<Base> c(a);
    EXPECT_TRUE(c.valueless_after_move());
}

TEST(PolymorphicTest, CopyAssignFromValueless) {
    polymorphic<Base> a(Derived(1));
    polymorphic<Base> b(std::move(a));
    polymorphic<Base> c(Derived(42));
    c = a;
    EXPECT_TRUE(c.valueless_after_move());
}

// --- Allocation tracking ---

// Polymorphic base for allocator tests - must be copyable
struct AllocBase {
    virtual ~AllocBase() = default;
    virtual int val() const { return 0; }
    AllocBase()                            = default;
    AllocBase(const AllocBase&)            = default;
    AllocBase(AllocBase&&)                 = default;
    AllocBase& operator=(const AllocBase&) = default;
    AllocBase& operator=(AllocBase&&)      = default;
};

struct AllocDerived : AllocBase {
    int x_;
    explicit AllocDerived(int x = 0) : x_(x) {}
    int val() const override { return x_; }
};

TEST(PolymorphicTest, CountAllocationsForDefaultConstruction) {
    unsigned alloc_counter   = 0;
    unsigned dealloc_counter = 0;
    {
        polymorphic<AllocBase, test::TrackingAllocator<AllocBase>> p(
            std::allocator_arg, test::TrackingAllocator<AllocBase>(&alloc_counter, &dealloc_counter));
        EXPECT_EQ(alloc_counter, 1u);
        EXPECT_EQ(dealloc_counter, 0u);
    }
    EXPECT_EQ(alloc_counter, 1u);
    EXPECT_EQ(dealloc_counter, 1u);
}

TEST(PolymorphicTest, CountAllocationsForDerivedConstruction) {
    unsigned alloc_counter   = 0;
    unsigned dealloc_counter = 0;
    {
        polymorphic<AllocBase, test::TrackingAllocator<AllocBase>> p(
            std::allocator_arg,
            test::TrackingAllocator<AllocBase>(&alloc_counter, &dealloc_counter),
            AllocDerived(42));
        EXPECT_EQ(alloc_counter, 1u);
        EXPECT_EQ(dealloc_counter, 0u);
        EXPECT_EQ((*p).val(), 42);
    }
    EXPECT_EQ(dealloc_counter, 1u);
}

TEST(PolymorphicTest, CountAllocationsForInPlaceTypeConstruction) {
    unsigned alloc_counter   = 0;
    unsigned dealloc_counter = 0;
    {
        polymorphic<AllocBase, test::TrackingAllocator<AllocBase>> p(
            std::allocator_arg,
            test::TrackingAllocator<AllocBase>(&alloc_counter, &dealloc_counter),
            std::in_place_type<AllocDerived>,
            42);
        EXPECT_EQ(alloc_counter, 1u);
        EXPECT_EQ(dealloc_counter, 0u);
    }
    EXPECT_EQ(dealloc_counter, 1u);
}

TEST(PolymorphicTest, CountAllocationsForCopyConstruction) {
    unsigned alloc_counter   = 0;
    unsigned dealloc_counter = 0;
    {
        polymorphic<AllocBase, test::TrackingAllocator<AllocBase>> p(
            std::allocator_arg,
            test::TrackingAllocator<AllocBase>(&alloc_counter, &dealloc_counter),
            AllocDerived(42));
        EXPECT_EQ(alloc_counter, 1u);
        auto q = p;
        EXPECT_EQ(alloc_counter, 2u);
        EXPECT_EQ(dealloc_counter, 0u);
    }
    EXPECT_EQ(dealloc_counter, 2u);
}

TEST(PolymorphicTest, CountAllocationsForMoveConstruction) {
    unsigned alloc_counter   = 0;
    unsigned dealloc_counter = 0;
    {
        polymorphic<AllocBase, test::TrackingAllocator<AllocBase>> p(
            std::allocator_arg,
            test::TrackingAllocator<AllocBase>(&alloc_counter, &dealloc_counter),
            AllocDerived(42));
        EXPECT_EQ(alloc_counter, 1u);
        auto q = std::move(p);
        // Move should not allocate
        EXPECT_EQ(alloc_counter, 1u);
        EXPECT_EQ(dealloc_counter, 0u);
    }
    EXPECT_EQ(dealloc_counter, 1u);
}

TEST(PolymorphicTest, CountAllocationsForCopyAssignment) {
    unsigned alloc_counter   = 0;
    unsigned dealloc_counter = 0;
    {
        test::TrackingAllocator<AllocBase>                         alloc(&alloc_counter, &dealloc_counter);
        polymorphic<AllocBase, test::TrackingAllocator<AllocBase>> p(std::allocator_arg, alloc, AllocDerived(42));
        polymorphic<AllocBase, test::TrackingAllocator<AllocBase>> q(std::allocator_arg, alloc, AllocDerived(0));
        EXPECT_EQ(alloc_counter, 2u);
        q = p;
        // Copy assignment: clone new + destroy old
        EXPECT_EQ(alloc_counter, 3u);
        EXPECT_EQ(dealloc_counter, 1u);
    }
    EXPECT_EQ(dealloc_counter, 3u);
}

TEST(PolymorphicTest, CountAllocationsForMoveAssignment) {
    unsigned alloc_counter   = 0;
    unsigned dealloc_counter = 0;
    {
        test::TrackingAllocator<AllocBase>                         alloc(&alloc_counter, &dealloc_counter);
        polymorphic<AllocBase, test::TrackingAllocator<AllocBase>> p(std::allocator_arg, alloc, AllocDerived(42));
        polymorphic<AllocBase, test::TrackingAllocator<AllocBase>> q(std::allocator_arg, alloc, AllocDerived(0));
        EXPECT_EQ(alloc_counter, 2u);
        q = std::move(p);
        // Move assign with equal allocators: takes ownership, destroys old
        EXPECT_EQ(alloc_counter, 2u);
        EXPECT_EQ(dealloc_counter, 1u);
    }
    EXPECT_EQ(dealloc_counter, 2u);
}

// --- Non-equal allocator tests ---

TEST(PolymorphicTest, MoveConstructionWithNonEqualAllocator) {
    unsigned alloc_counter1   = 0;
    unsigned dealloc_counter1 = 0;
    unsigned alloc_counter2   = 0;
    unsigned dealloc_counter2 = 0;

    test::NonEqualTrackingAllocator<AllocBase> alloc1(&alloc_counter1, &dealloc_counter1);
    test::NonEqualTrackingAllocator<AllocBase> alloc2(&alloc_counter2, &dealloc_counter2);

    polymorphic<AllocBase, test::NonEqualTrackingAllocator<AllocBase>> p(std::allocator_arg, alloc1, AllocDerived(42));
    EXPECT_EQ(alloc_counter1, 1u);

    // Allocator-extended move with non-equal allocator: must move_clone
    polymorphic<AllocBase, test::NonEqualTrackingAllocator<AllocBase>> q(std::allocator_arg, alloc2, std::move(p));
    EXPECT_EQ(alloc_counter2, 1u);
    EXPECT_EQ((*q).val(), 42);
}

TEST(PolymorphicTest, MoveAssignmentWithNonEqualAllocator) {
    unsigned alloc_counter1   = 0;
    unsigned dealloc_counter1 = 0;
    unsigned alloc_counter2   = 0;
    unsigned dealloc_counter2 = 0;

    test::NonEqualTrackingAllocator<AllocBase> alloc1(&alloc_counter1, &dealloc_counter1);
    test::NonEqualTrackingAllocator<AllocBase> alloc2(&alloc_counter2, &dealloc_counter2);

    polymorphic<AllocBase, test::NonEqualTrackingAllocator<AllocBase>> p(std::allocator_arg, alloc1, AllocDerived(42));
    polymorphic<AllocBase, test::NonEqualTrackingAllocator<AllocBase>> q(std::allocator_arg, alloc2, AllocDerived(0));

    // NonEqualTrackingAllocator propagates on move assignment
    q = std::move(p);
    EXPECT_EQ((*q).val(), 42);
    EXPECT_TRUE(p.valueless_after_move());
}

// --- Tagged allocator tests ---

TEST(PolymorphicTest, TaggedAllocatorGetAllocator) {
    test::TaggedAllocator<Base>                    alloc(42);
    polymorphic<Base, test::TaggedAllocator<Base>> p(std::allocator_arg, alloc, Derived(7));
    EXPECT_EQ(p.get_allocator().tag, 42u);
}

TEST(PolymorphicTest, TaggedAllocatorCopy) {
    test::TaggedAllocator<Base>                    alloc(42);
    polymorphic<Base, test::TaggedAllocator<Base>> p(std::allocator_arg, alloc, Derived(7));
    auto                                           q = p;
    EXPECT_EQ(q.get_allocator().tag, 42u);
    EXPECT_EQ((*q).value(), 7);
}

// --- Container integration ---

TEST(PolymorphicTest, InteractionWithOptional) {
    std::optional<polymorphic<Base>> opt;
    EXPECT_FALSE(opt.has_value());
    opt = polymorphic<Base>(Derived(42));
    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ((*opt)->value(), 42);
}

TEST(PolymorphicTest, InteractionWithVector) {
    std::vector<polymorphic<Base>> v;
    v.push_back(polymorphic<Base>(Derived(1)));
    v.push_back(polymorphic<Base>(Derived(2)));
    v.push_back(polymorphic<Base>(Derived2("abc")));
    EXPECT_EQ((*v[0]).value(), 1);
    EXPECT_EQ((*v[1]).value(), 2);
    EXPECT_EQ((*v[2]).name(), "Derived2:abc");

    // Copy the vector
    auto v2 = v;
    EXPECT_EQ((*v2[0]).value(), 1);
    EXPECT_EQ((*v2[2]).name(), "Derived2:abc");
}

TEST(PolymorphicTest, InteractionWithMap) {
    std::map<int, polymorphic<Base>> m;
    m.emplace(1, polymorphic<Base>(Derived(10)));
    m.emplace(2, polymorphic<Base>(Derived2("hello")));
    EXPECT_EQ((*m.at(1)).value(), 10);
    EXPECT_EQ((*m.at(2)).name(), "Derived2:hello");
}

// --- Swap with POCS allocator ---

TEST(PolymorphicTest, SwapWithPOCSAllocator) {
    unsigned alloc_counter1   = 0;
    unsigned dealloc_counter1 = 0;
    unsigned alloc_counter2   = 0;
    unsigned dealloc_counter2 = 0;

    test::POCSTrackingAllocator<AllocBase> alloc1(&alloc_counter1, &dealloc_counter1);
    test::POCSTrackingAllocator<AllocBase> alloc2(&alloc_counter2, &dealloc_counter2);

    polymorphic<AllocBase, test::POCSTrackingAllocator<AllocBase>> a(std::allocator_arg, alloc1, AllocDerived(1));
    polymorphic<AllocBase, test::POCSTrackingAllocator<AllocBase>> b(std::allocator_arg, alloc2, AllocDerived(2));

    a.swap(b);
    EXPECT_EQ((*a).val(), 2);
    EXPECT_EQ((*b).val(), 1);
}

// --- PMR alias ---

// PMR-compatible base for testing
struct PmrBase {
    virtual ~PmrBase() = default;
    virtual int val() const { return 0; }
    PmrBase()                          = default;
    PmrBase(const PmrBase&)            = default;
    PmrBase(PmrBase&&)                 = default;
    PmrBase& operator=(const PmrBase&) = default;
    PmrBase& operator=(PmrBase&&)      = default;
};

struct PmrDerived : PmrBase {
    int x_;
    explicit PmrDerived(int x = 0) : x_(x) {}
    int val() const override { return x_; }
};

TEST(PolymorphicTest, PmrAlias) {
    std::array<std::byte, 256>                 buffer{};
    std::pmr::monotonic_buffer_resource        resource(buffer.data(), buffer.size());
    beman::indirect::pmr::polymorphic<PmrBase> p(
        std::allocator_arg, std::pmr::polymorphic_allocator<PmrBase>(&resource), PmrDerived(42));
    EXPECT_EQ((*p).val(), 42);
}

TEST(PolymorphicTest, PmrCopy) {
    std::array<std::byte, 1024>                buffer{};
    std::pmr::monotonic_buffer_resource        resource(buffer.data(), buffer.size());
    std::pmr::polymorphic_allocator<PmrBase>   alloc(&resource);
    beman::indirect::pmr::polymorphic<PmrBase> p(std::allocator_arg, alloc, PmrDerived(42));
    auto                                       q = p;
    EXPECT_EQ((*q).val(), 42);
}

} // namespace
