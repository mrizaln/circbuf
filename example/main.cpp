#include <circbuf/circular_buffer.hpp>

#include <iostream>
#include <ranges>
#include <string>

namespace sv = std::views;

using circbuf::CircularBuffer;

void print(std::ranges::range auto&& range)
{
    std::cout << "------------\n";
    for (const auto& value : range) {
        std::cout << value << '\n';
    }
}

void simple()
{
    auto queue = CircularBuffer<std::string>{ 12 };

    for (auto i : std::views::iota(0, 256)) {
        queue.push_back(std::format("{0:0b}|{0}", i));
    }

    queue.push_front("hello");
    queue.push_front("world");

    print(queue | sv::reverse);

    auto mid = queue.remove(6);
    std::cout << ">>> mid: " << mid << '\n';

    print(queue);
}

void underlying()
{
    {
        auto queue = CircularBuffer<int>{ 12 };

        for (auto i : sv::iota(0, 10)) {
            queue.push_back(i);
        }

        assert(not queue.empty());

        auto span = queue.linearize().data();
        print(span);

        assert(queue.linearized());

        // linearize() function is in-place, use linearizeCopy() to make a copy instead
        auto copy = queue.linearizeCopy();
        assert(copy.linearized());

        // copying in general will also linearize the resulting buffer
        auto copy2 = queue;
        assert(copy2.linearized());
    }

    {
        auto queue = CircularBuffer<int>{ 12 };

        for (auto i : sv::iota(0, 2373)) {
            queue.push_back(i);
        }

        assert(queue.full());

        // if you want the data to be whatever order in the underlying buffer, no need for linearize
        auto raw = queue.data();
        print(raw);

        // you need to linearize your buffer if you want to access it from head to tail
        auto buf = queue.linearize().data();
        print(buf);
    }
}

int main()
{
    simple();
    underlying();
}
