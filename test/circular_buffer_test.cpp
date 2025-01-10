#include "test_util.hpp"

#include <circbuf/circular_buffer.hpp>

#include <boost/ut.hpp>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <fmt/std.h>

#include <cassert>
#include <ranges>
#include <concepts>
#include <vector>

namespace ut = boost::ut;
namespace rr = std::ranges;
namespace rv = rr::views;

using test_util::equal_underlying;
using test_util::populate_container;
using test_util::populate_container_front;
using test_util::subrange;

static constexpr std::tuple g_policy_permutations = {
    circbuf::BufferPolicy::ReplaceOnFull,
    circbuf::BufferPolicy::ThrowOnFull,
};

// TODO: check whether copy happens on operations that should not copy (unless type is not movable)
// TODO: add test for edge case: 1 digit capacity
template <test_util::TestClass Type>
void test()
{
    using namespace ut::operators;
    using namespace ut::literals;
    using ut::expect, ut::that, ut::throws, ut::nothrow;

    Type::reset_active_instance_count();

    "iterator should be a random access iterator"_test = [] {
        using Iter = circbuf::CircularBuffer<Type>::template Iterator<false>;
        static_assert(std::random_access_iterator<Iter>);

        using ConstIter = circbuf::CircularBuffer<Type>::template Iterator<true>;
        static_assert(std::random_access_iterator<ConstIter>);
    };

    "push_back should add an element to the back"_test = [](circbuf::BufferPolicy policy) {
        auto buffer = circbuf::CircularBuffer<Type>{ 10, policy };

        // first push
        auto  size  = buffer.size();
        auto& value = buffer.push_back(42);
        expect(that % buffer.size() == size + 1);
        expect(value.value() == 42_i);
        expect(buffer.back().value() == 42_i);
        expect(buffer.front().value() == 42_i);

        for (auto i : rv::iota(0, 9)) {
            auto& value = buffer.push_back(i);
            expect(that % value.value() == i);
            expect(that % buffer.back().value() == i);
            expect(that % buffer.front().value() == 42);
        }

        auto expected = std::array{ 42, 0, 1, 2, 3, 4, 5, 6, 7, 8 };
        expect(equal_underlying<Type>(buffer, expected));

        buffer.clear();
        expect(buffer.size() == 0_i);

        rr::fill_n(std::back_inserter(buffer), 10, 42);
        for (const auto& value : buffer) {
            expect(value.value() == 42_i);
        }
        expect(buffer.size() == 10_i);
    } | g_policy_permutations;

    "push_back with ReplaceOnFull policy should replace the adjacent element when buffer is full"_test = [] {
        auto policy = circbuf::BufferPolicy::ReplaceOnFull;
        auto buffer = circbuf::CircularBuffer<Type>{ 10, policy };

        // fully populate buffer
        populate_container(buffer, rv::iota(0, 10));
        expect(buffer.size() == 10_i);
        expect(that % buffer.size() == buffer.capacity());
        expect(equal_underlying<Type>(buffer, rv::iota(0, 10)));

        // replace old elements (4 times)
        for (auto i : rv::iota(21, 25)) {
            auto& value = buffer.push_back(i);
            expect(that % value.value() == i);
            expect(that % buffer.back().value() == i);
            expect(buffer.capacity() == 10_i);
            expect(buffer.size() == 10_i);
        }

        // the circular-buffer iterator
        expect(equal_underlying<Type>(subrange(buffer, 0, 6), rv::iota(0, 10) | rv::drop(4)));
        expect(equal_underlying<Type>(subrange(buffer, 6, 10), rv::iota(21, 25)));

        // the underlying array
        auto underlying = buffer.data();
        expect(equal_underlying<Type>(subrange(underlying, 0, 4), rv::iota(21, 25)));
        expect(equal_underlying<Type>(subrange(underlying, 4, 10), rv::iota(0, 10) | rv::drop(4)));
    };

    "push_back with ThrowOnFull policy should throw when buffer is full"_test = [] {
        auto policy = circbuf::BufferPolicy::ThrowOnFull;
        auto buffer = circbuf::CircularBuffer<Type>{ 10, policy };

        // fully populate buffer
        populate_container(buffer, rv::iota(0, 10));
        expect(buffer.size() == 10_i);
        expect(that % buffer.size() == buffer.capacity());
        expect(equal_underlying<Type>(buffer, rv::iota(0, 10)));

        expect(throws([&] { buffer.push_back(42); })) << "should throw when push to full buffer";
    };

    "push_front should add an element to the front"_test = [](circbuf::BufferPolicy policy) {
        auto buffer = circbuf::CircularBuffer<Type>{ 10, policy };

        // first push
        auto  size  = buffer.size();
        auto& value = buffer.push_front(42);
        expect(that % buffer.size() == size + 1);
        expect(value.value() == 42_i);
        expect(buffer.front().value() == 42_i);
        expect(buffer.back().value() == 42_i);

        for (auto i : rv::iota(0, 9)) {
            auto& value = buffer.push_front(i);
            expect(that % value.value() == i);
            expect(that % buffer.front().value() == i);
            expect(that % buffer.back().value() == 42);
        }

        auto expected = std::array{ 42, 0, 1, 2, 3, 4, 5, 6, 7, 8 };
        expect(equal_underlying<Type>(buffer | rv::reverse, expected));

        buffer.clear();
        expect(buffer.size() == 0_i);

        rr::fill_n(std::back_inserter(buffer), 10, 42);
        for (const auto& value : buffer) {
            expect(value.value() == 42_i);
        }
        expect(buffer.size() == 10_i);
    } | g_policy_permutations;

    "push_front with ReplaceOnFull policy should replace the adjacent element when buffer is full"_test = [] {
        auto policy = circbuf::BufferPolicy::ReplaceOnFull;
        auto buffer = circbuf::CircularBuffer<Type>{ 10, policy };

        // fully populate buffer
        populate_container_front(buffer, rv::iota(0, 10));
        expect(buffer.size() == 10_i);
        expect(that % buffer.size() == buffer.capacity());
        expect(equal_underlying<Type>(buffer | rv::reverse, rv::iota(0, 10)));

        // replace old elements (4 times)
        for (auto i : rv::iota(21, 25)) {
            auto& value = buffer.push_front(i);
            expect(that % value.value() == i);
            expect(that % buffer.front().value() == i);
            expect(buffer.capacity() == 10_i);
            expect(buffer.size() == 10_i);
        }

        // the circular-buffer iterator
        auto rbuffer = buffer | rv::reverse;
        expect(equal_underlying<Type>(subrange(rbuffer, 0, 6), rv::iota(0, 10) | rv::drop(4)));
        expect(equal_underlying<Type>(subrange(rbuffer, 6, 10), rv::iota(21, 25)));

        // the underlying array
        auto runderlying = buffer.data() | rv::reverse;
        expect(equal_underlying<Type>(subrange(runderlying, 0, 4), rv::iota(21, 25)));
        expect(equal_underlying<Type>(subrange(runderlying, 4, 10), rv::iota(0, 10) | rv::drop(4)));
    };

    "push_front with ThrowOnFull policy should throw when buffer is full"_test = [] {
        auto policy = circbuf::BufferPolicy::ThrowOnFull;
        auto buffer = circbuf::CircularBuffer<Type>{ 10, policy };

        // fully populate buffer
        populate_container_front(buffer, rv::iota(0, 10));
        expect(buffer.size() == 10_i);
        expect(that % buffer.size() == buffer.capacity());
        expect(equal_underlying<Type>(buffer | rv::reverse, rv::iota(0, 10)));

        expect(throws([&] { buffer.push_front(42); })) << "should throw when push to full buffer";
    };

    "pop_front should remove the first element on the buffer"_test = [](circbuf::BufferPolicy policy) {
        auto values = std::array{ 42, 0, 1, 2, 3, 4, 5, 6, 7, 8 };
        auto buffer = circbuf::CircularBuffer<Type>{ 10, policy };

        for (auto value : values) {
            buffer.push_back(std::move(value));
        }
        expect(that % buffer.size() == values.size());

        // first pop
        auto size  = buffer.size();
        auto value = buffer.pop_front();
        expect(that % buffer.size() == size - 1);
        expect(value.value() == 42_i);

        for (auto i : rv::iota(0, 8)) {
            expect(buffer.pop_front().value() == i);
        }
        expect(buffer.size() == 1_i);
        expect(buffer.pop_front().value() == 8_i);
        expect(buffer.size() == 0_i);

        expect(throws([&] { buffer.pop_front(); })) << "should throw when pop from empty buffer";
    } | g_policy_permutations;

    "insertion in the middle should move the elements around"_test = [] {
        // full buffer condition
        {
            auto buffer = circbuf::CircularBuffer<Type>{ 10 };    // default policy
            populate_container(buffer, rv::iota(0, 10));

            buffer.insert(3, 42, circbuf::BufferInsertPolicy::DiscardHead);
            expect(buffer.size() == 10_i);
            expect(equal_underlying<Type>(subrange(buffer, 0, 3), rv::iota(1, 4)));
            expect(buffer.at(3).value() == 42_i);
            expect(equal_underlying<Type>(subrange(buffer, 4, 10), rv::iota(4, 10)));

            auto expected = std::vector{ 1, 2, 3, 42, 4, 5, 6, 7, 8, 9 };
            expect(equal_underlying<Type>(buffer, expected));

            buffer.insert(0, 42, circbuf::BufferInsertPolicy::DiscardHead);
            expected = std::vector{ 42, 2, 3, 42, 4, 5, 6, 7, 8, 9 };
            expect(equal_underlying<Type>(buffer, expected));

            buffer.insert(9, 32748, circbuf::BufferInsertPolicy::DiscardHead);
            expected = std::vector{ 2, 3, 42, 4, 5, 6, 7, 8, 9, 32748 };
            expect(equal_underlying<Type>(buffer, expected));
        }

        // partially filled buffer condition
        {
            auto buffer = circbuf::CircularBuffer<Type>{ 10 };    // default policy
            populate_container(buffer, rv::iota(0, 15));
            for (auto _ : rv::iota(0, 5)) {
                buffer.pop_front();
            }

            auto expected = std::vector{ 10, 11, 12, 13, 14 };
            expect(equal_underlying<Type>(buffer, expected));

            buffer.insert(2, -42, circbuf::BufferInsertPolicy::DiscardHead);
            expected = std::vector{ 10, 11, -42, 12, 13, 14 };
            expect(equal_underlying<Type>(buffer, expected));

            buffer.insert(0, -42, circbuf::BufferInsertPolicy::DiscardHead);
            expected = std::vector{ -42, 10, 11, -42, 12, 13, 14 };
            buffer.insert(buffer.size(), -42, circbuf::BufferInsertPolicy::DiscardHead);
        }
    };

    "removal should be able to remove value anywhere in the buffer"_test = [] {
        auto buffer = circbuf::CircularBuffer<Type>{ 10 };    // default policy
        populate_container(buffer, rv::iota(0, 15));

        auto expected = std::vector{ 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 };
        expect(equal_underlying<Type>(buffer, expected));

        auto value = buffer.remove(3);
        expected   = std::vector{ 5, 6, 7, 9, 10, 11, 12, 13, 14 };
        expect(value.value() == 8_i);
        expect(equal_underlying<Type>(buffer, expected));

        auto end = buffer.size() - 1;
        value    = buffer.remove(end);
        expected = std::vector{ 5, 6, 7, 9, 10, 11, 12, 13 };
        expect(value.value() == 14_i);
        expect(equal_underlying<Type>(buffer, expected));

        value    = buffer.remove(0);
        expected = std::vector{ 6, 7, 9, 10, 11, 12, 13 };
        expect(value.value() == 5_i);
        expect(equal_underlying<Type>(buffer, expected));
    };

    "default initialized CircularBuffer is basically useless"_test = [] {
        auto buffer = circbuf::CircularBuffer<int>{};
        expect(buffer.size() == 0_i);
        expect(buffer.capacity() == 0_i);

        using circbuf::error::ZeroCapacity, circbuf::error::BufferEmpty;

        expect(throws<ZeroCapacity>([&] { buffer.push_back(42); })) << "throw when push to empty buffer";
        expect(throws<BufferEmpty>([&] { buffer.pop_front(); })) << "throw when pop from empty buffer";
    };

    "move should leave buffer into an empty state that is not usable"_test = [] {
        auto buffer = circbuf::CircularBuffer<Type>{ 20 };
        populate_container(buffer, rv::iota(0, 10));

        expect(buffer.size() == 10_i);
        expect(equal_underlying<Type>(buffer, rv::iota(0, 10)));

        auto buffer2 = std::move(buffer);
        expect(buffer.size() == 0_i);
        expect(buffer.capacity() == 0_i);

        using circbuf::error::ZeroCapacity, circbuf::error::BufferEmpty;

        expect(throws<ZeroCapacity>([&] { buffer.push_back(42); })) << "throw when push to empty buffer";
        expect(throws<BufferEmpty>([&] { buffer.pop_front(); })) << "throw when pop from empty buffer";
    };

    if constexpr (std::copyable<Type>) {
        "copy should copy each element exactly"_test = [] {
            auto buffer = circbuf::CircularBuffer<Type>{ 20 };
            populate_container(buffer, rv::iota(0, 10));

            expect(buffer.size() == 10_i);
            expect(equal_underlying<Type>(buffer, rv::iota(0, 10)));

            auto buffer2 = buffer;
            expect(buffer2.size() == 10_i);
            expect(rr::equal(buffer2, buffer));

            auto buffer3 = buffer2;
            expect(buffer3.size() == 10_i);
            expect(rr::equal(buffer3, buffer));
        };

        "copying buffer with zero capacity should success"_test = [] {
            auto buffer = circbuf::CircularBuffer<Type>{ 0 };
            auto copy   = buffer;

            expect(that % buffer.capacity() == copy.capacity());
            expect(that % copy.capacity() == 0);
            expect(that % copy.size() == 0);
        };

        "copying buffer with non-zero capacity but zero element should success"_test = [] {
            auto buffer = circbuf::CircularBuffer<Type>{ 10 };
            auto copy   = buffer;

            expect(that % buffer.capacity() == copy.capacity());
            expect(that % copy.capacity() == 10);
            expect(that % copy.size() == 0);
        };

        "copying buffer with non-zero capacity but partially filled should success"_test = [] {
            auto buffer = circbuf::CircularBuffer<Type>{ 10 };
            populate_container(buffer, rv::iota(0, 5));

            auto copy = buffer;

            expect(that % buffer.capacity() == copy.capacity());
            expect(that % copy.capacity() == 10);
            expect(that % copy.size() == 5);
        };

        "copying buffer with non-zero capacity but partially filled but once full should success"_test = [] {
            auto buffer = circbuf::CircularBuffer<Type>{ 10 };
            populate_container(buffer, rv::iota(0, 15));
            for (auto _ : rv::iota(0, 5)) {
                buffer.pop_front();
            }

            auto copy = buffer;

            expect(that % buffer.capacity() == copy.capacity());
            expect(that % copy.capacity() == 10);
            expect(that % copy.size() == 5);
        };

        "copying a buffer which is fully filled should success"_test = [] {
            auto buffer = circbuf::CircularBuffer<Type>{ 10 };
            populate_container(buffer, rv::iota(0, 15));

            auto copy = buffer;

            expect(that % buffer.capacity() == copy.capacity());
            expect(that % copy.capacity() == 10);
            expect(that % copy.size() == 10);
        };
    }

    "unbalanced constructor/destructor means there is a bug in the code"_test = [] {
        expect(Type::active_instance_count() == 0_i) << "Unbalanced ctor/dtor detected!";
    };
}

int main()
{
    test_util::for_each_tuple<test_util::NonTrivialPermutations>([]<typename T>() {
        if constexpr (circbuf::CircularBufferElement<T>) {
            test<T>();
        }
    });
}
