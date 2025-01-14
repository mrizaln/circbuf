#include <circbuf/circbuf.hpp>

#include <iostream>
#include <ranges>
#include <string>

namespace sv = std::views;

using circbuf::CircBuf;

void print(std::ranges::range auto&& range)
{
    std::cout << "------------\n";
    for (const auto& value : range) {
        std::cout << value << '\n';
    }
}

void simple()
{
    auto queue = CircBuf<std::string>{ 12 };

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
        auto queue = CircBuf<int>{ 12 };

        for (auto i : sv::iota(0, 14)) {
            queue.push_back(i);
        }

        queue.at(0) = 0;
        queue.pop_front();
        queue.at(0) = 0;
        queue.pop_front();
        queue.at(0) = 0;
        queue.pop_front();

        assert(not queue.empty());

        auto span = queue.linearize().data();
        print(span);

        assert(queue.linearized());

        queue.pop_front();

        // linearize() function is in-place, use the copy ctor to make a copy instead (policy inherited)
        auto copy = queue;
        assert(copy.linearized());

        // or linearize_copy() to make a copy instead, you can change the policy here
        auto copy2 = queue.linearize_copy(circbuf::BufferPolicy::ThrowOnFull);
        assert(copy2.linearized());

        assert(copy.policy() == queue.policy());
        assert(copy.policy() != copy2.policy());
    }

    {
        auto queue = CircBuf<int>{ 12 };

        for (auto i : sv::iota(0, 2373)) {
            queue.push_back(i);
        }

        assert(queue.full());

        // if you want the data to be whatever order in the underlying buffer, no need for linearize, though
        // make sure the buffer is full before doing this
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
