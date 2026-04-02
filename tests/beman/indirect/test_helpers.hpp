// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef BEMAN_INDIRECT_TEST_HELPERS_HPP
#define BEMAN_INDIRECT_TEST_HELPERS_HPP

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace test {

// Allocator that counts allocations and deallocations via external counters.
template <class T>
struct TrackingAllocator {
    using value_type = T;

    unsigned* alloc_counter_;
    unsigned* dealloc_counter_;

    TrackingAllocator(unsigned* alloc_counter, unsigned* dealloc_counter)
        : alloc_counter_(alloc_counter), dealloc_counter_(dealloc_counter) {}

    template <class U>
    TrackingAllocator(const TrackingAllocator<U>& other)
        : alloc_counter_(other.alloc_counter_), dealloc_counter_(other.dealloc_counter_) {}

    template <class Other>
    struct rebind {
        using other = TrackingAllocator<Other>;
    };

    T* allocate(std::size_t n) {
        if (alloc_counter_)
            ++(*alloc_counter_);
        std::allocator<T> a;
        return a.allocate(n);
    }

    void deallocate(T* p, std::size_t n) {
        if (dealloc_counter_)
            ++(*dealloc_counter_);
        std::allocator<T> a;
        a.deallocate(p, n);
    }

    friend bool operator==(const TrackingAllocator& lhs, const TrackingAllocator& rhs) noexcept {
        return lhs.alloc_counter_ == rhs.alloc_counter_ && lhs.dealloc_counter_ == rhs.dealloc_counter_;
    }

    friend bool operator!=(const TrackingAllocator& lhs, const TrackingAllocator& rhs) noexcept {
        return !(lhs == rhs);
    }
};

// TrackingAllocator variant that always compares unequal.
// Propagates on copy/move assignment so operations can proceed.
template <class T>
struct NonEqualTrackingAllocator {
    using value_type                             = T;
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;

    unsigned* alloc_counter_;
    unsigned* dealloc_counter_;

    NonEqualTrackingAllocator(unsigned* alloc_counter, unsigned* dealloc_counter)
        : alloc_counter_(alloc_counter), dealloc_counter_(dealloc_counter) {}

    template <class U>
    NonEqualTrackingAllocator(const NonEqualTrackingAllocator<U>& other)
        : alloc_counter_(other.alloc_counter_), dealloc_counter_(other.dealloc_counter_) {}

    template <class Other>
    struct rebind {
        using other = NonEqualTrackingAllocator<Other>;
    };

    T* allocate(std::size_t n) {
        if (alloc_counter_)
            ++(*alloc_counter_);
        std::allocator<T> a;
        return a.allocate(n);
    }

    void deallocate(T* p, std::size_t n) {
        if (dealloc_counter_)
            ++(*dealloc_counter_);
        std::allocator<T> a;
        a.deallocate(p, n);
    }

    friend bool operator==(const NonEqualTrackingAllocator&, const NonEqualTrackingAllocator&) noexcept {
        return false;
    }

    friend bool operator!=(const NonEqualTrackingAllocator&, const NonEqualTrackingAllocator&) noexcept {
        return true;
    }
};

// TrackingAllocator variant with propagate_on_container_swap.
template <class T>
struct POCSTrackingAllocator {
    using value_type                  = T;
    using propagate_on_container_swap = std::true_type;

    unsigned* alloc_counter_;
    unsigned* dealloc_counter_;

    POCSTrackingAllocator(unsigned* alloc_counter, unsigned* dealloc_counter)
        : alloc_counter_(alloc_counter), dealloc_counter_(dealloc_counter) {}

    template <class U>
    POCSTrackingAllocator(const POCSTrackingAllocator<U>& other)
        : alloc_counter_(other.alloc_counter_), dealloc_counter_(other.dealloc_counter_) {}

    template <class Other>
    struct rebind {
        using other = POCSTrackingAllocator<Other>;
    };

    T* allocate(std::size_t n) {
        if (alloc_counter_)
            ++(*alloc_counter_);
        std::allocator<T> a;
        return a.allocate(n);
    }

    void deallocate(T* p, std::size_t n) {
        if (dealloc_counter_)
            ++(*dealloc_counter_);
        std::allocator<T> a;
        a.deallocate(p, n);
    }

    friend bool operator==(const POCSTrackingAllocator& lhs, const POCSTrackingAllocator& rhs) noexcept {
        return lhs.alloc_counter_ == rhs.alloc_counter_ && lhs.dealloc_counter_ == rhs.dealloc_counter_;
    }

    friend bool operator!=(const POCSTrackingAllocator& lhs, const POCSTrackingAllocator& rhs) noexcept {
        return !(lhs == rhs);
    }
};

// Simple allocator with a tag for identity comparison.
template <class T>
struct TaggedAllocator {
    using value_type = T;
    std::size_t tag;

    explicit TaggedAllocator(std::size_t t = 0) : tag(t) {}

    template <class U>
    TaggedAllocator(const TaggedAllocator<U>& other) : tag(other.tag) {}

    template <class Other>
    struct rebind {
        using other = TaggedAllocator<Other>;
    };

    T* allocate(std::size_t n) {
        std::allocator<T> a;
        return a.allocate(n);
    }

    void deallocate(T* p, std::size_t n) {
        std::allocator<T> a;
        a.deallocate(p, n);
    }

    friend bool operator==(const TaggedAllocator& lhs, const TaggedAllocator& rhs) noexcept {
        return lhs.tag == rhs.tag;
    }

    friend bool operator!=(const TaggedAllocator& lhs, const TaggedAllocator& rhs) noexcept { return !(lhs == rhs); }
};

// Type that throws on construction.
struct ThrowsOnConstruction {
    struct Exception {};
    ThrowsOnConstruction() { throw Exception{}; }
    ThrowsOnConstruction(const ThrowsOnConstruction&) { throw Exception{}; }
    ThrowsOnConstruction(ThrowsOnConstruction&&) { throw Exception{}; }
};

// Type that throws on copy.
struct ThrowsOnCopy {
    struct Exception {};
    int value;
    explicit ThrowsOnCopy(int v = 0) : value(v) {}
    ThrowsOnCopy(const ThrowsOnCopy&) { throw Exception{}; }
    ThrowsOnCopy(ThrowsOnCopy&&) = default;
    ThrowsOnCopy& operator=(const ThrowsOnCopy&) { throw Exception{}; }
    ThrowsOnCopy& operator=(ThrowsOnCopy&&) = default;
};

} // namespace test

#endif // BEMAN_INDIRECT_TEST_HELPERS_HPP
