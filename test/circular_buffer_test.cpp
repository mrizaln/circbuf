#include "circular_buffer.hpp"

#include <boost/ut.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <span>
#include <vector>
#include <ranges>

namespace sr = std::ranges;
namespace sv = std::views;

namespace ut = boost::ut;
using namespace ut::literals;
using namespace ut::operators;

template <typename T>
using CircBuffer = circbuf::CircularBuffer<T>;

struct TrivialType
{
    int   m_value1;
    float m_value2;

    auto operator<=>(const TrivialType&) const = default;

    // for fmt::format
    friend auto          format_as(TrivialType f) { return std::tuple{ f.m_value1, f.m_value2 }; }
    friend std::ostream& operator<<(std::ostream& os, TrivialType f)
    {
        return os << fmt::format("{}", format_as(f));
    }
};

struct NonTrivialType
{
    int m_value = 0;

    NonTrivialType() = default;

    NonTrivialType(int value)
        : m_value{ value }
    {
    }

    NonTrivialType(const NonTrivialType& other)
        : m_value{ other.m_value }
    {
    }

    NonTrivialType& operator=(const NonTrivialType& other)
    {
        m_value = other.m_value;
        return *this;
    }

    NonTrivialType(NonTrivialType&& other) noexcept
        : m_value{ std::exchange(other.m_value, 0) }
    {
    }

    NonTrivialType& operator=(NonTrivialType&& other) noexcept
    {
        m_value = std::exchange(other.m_value, 0);
        return *this;
    }

    auto operator<=>(const NonTrivialType&) const = default;

    // for fmt::format
    friend auto          format_as(NonTrivialType f) { return f.m_value; }
    friend std::ostream& operator<<(std::ostream& os, NonTrivialType f)
    {
        return os << fmt::format("{}", format_as(f));
    }
};

int main()
{
    "push from empty state but not until full"_test = [] {
        CircBuffer<int> circ{ 10 };
        std::vector     expected{ 1, 2, 3, 4, 5, 6, 0, 0, 0, 0 };    // 6 insertion
        for (int i = 1; i <= 6; ++i) {
            circ.push(std::move(i));
        }
        ut::expect(circ.size() == 6_ull);
        ut::expect(sr::equal(circ.buf(), expected));
    };

    "push from empty state until full"_test = [] {
        CircBuffer<int> circ{ 5 };
        std::vector     expected{ 1, 2, 3, 4, 5 };    // 5 insertion
        for (int i = 1; i <= 5; ++i) {
            circ.push(std::move(i));
        }
        ut::expect(circ.size() == 5_ull);
        ut::expect(sr::equal(circ.buf(), expected));
    };

    "push from empty state still push even after full"_test = [] {
        CircBuffer<int> circ{ 5 };
        std::vector     expected{ 6, 7, 3, 4, 5 };    // 7 insertion; 6, 7 will overwrite 1, 2
        for (int i = 1; i <= 7; ++i) {
            circ.push(std::move(i));
        }
        ut::expect(circ.size() == 5_ull);    // full
        ut::expect(sr::equal(circ.buf(), expected));
    };

    "pop until exhausted but after that the begin pointer is not at the start"_test = [] {
        CircBuffer<NonTrivialType>  circ{ 5 };
        std::vector<NonTrivialType> initial{ 6, 7, 3, 4, 5 };    // 7 insertions
        std::vector<NonTrivialType> afterPushAfterExhausted{ 0, 0, 8, 0, 0 };

        for (int i = 1; i <= 7; ++i) {
            circ.push(NonTrivialType{ i });
        }
        ut::expect(circ.size() == 5_ull);
        ut::expect(sr::equal(circ.buf(), initial));

        while (circ.pop() != std::nullopt) { }

        ut::expect(circ.size() == 0_ull);
        ut::expect(circ.begin() == 2_ull);
        ut::expect(circ.end() == 2_ull);

        ut::expect(*circ.push(NonTrivialType{ 8 }) == NonTrivialType{ 8 });
        ut::expect(circ.size() == 1_ull);
        ut::expect(sr::equal(circ.buf(), afterPushAfterExhausted));
    };

    "pop from empty state"_test = [] {
        CircBuffer<int> circ{ 5 };
        ut::expect(circ.pop() == std::nullopt);
    };

    "pop from non-empty state but not full for trivial types"_test = [] {
        CircBuffer<TrivialType>  circ{ 5 };
        std::vector<TrivialType> expected{
            { 1, 1.0f }, { 2, 2.0f }, { 3, 3.0f }, { 4, 4.0f }, { 0, 0.0f }
        };    // 4 insertion

        for (int i = 1; i <= 4; ++i) {
            circ.push(TrivialType{ i, static_cast<float>(i) });
        }
        ut::expect(circ.size() == 4_ull);
        ut::expect(sr::equal(circ.buf(), expected));

        ut::expect(*circ.pop() == TrivialType{ 1, 1.0f });
        ut::expect(circ.size() == 3_ull);
        ut::expect(sr::equal(circ.buf() | sv::drop(1), expected | sv::drop(1)));
    };

    "pop from non-empty state but not full for non-trivial types"_test = [] {
        CircBuffer<NonTrivialType>  circ{ 5 };
        std::vector<NonTrivialType> expected{ { 1 }, { 2 }, { 3 }, { 4 }, {} };    // 4 insertion

        for (int i = 1; i <= 4; ++i) {
            circ.push(NonTrivialType{ i });
        }
        ut::expect(circ.size() == 4_ull);
        ut::expect(sr::equal(circ.buf(), expected));

        ut::expect(*circ.pop() == NonTrivialType{ 1 });
        ut::expect(circ.size() == 3_ull);
        ut::expect(circ.buf()[0] == NonTrivialType{}) << "pop-ed element should be reset to default";
        ut::expect(sr::equal(circ.buf() | sv::drop(1), expected | sv::drop(1)));
    };

    "pop from a full state for trivial types"_test = [] {
        CircBuffer<TrivialType>  circ{ 5 };
        std::vector<TrivialType> expected{
            { 1, 1.0f }, { 2, 2.0f }, { 3, 3.0f }, { 4, 4.0f }, { 5, 5.0f }
        };    // 5 insertion

        for (int i = 1; i <= 5; ++i) {
            circ.push(TrivialType{ i, static_cast<float>(i) });
        }
        ut::expect(circ.size() == 5_ull);
        ut::expect(sr::equal(circ.buf(), expected));

        ut::expect(*circ.pop() == TrivialType{ 1, 1.0f });
        ut::expect(circ.size() == 4_ull);
        ut::expect(sr::equal(circ.buf() | sv::drop(1), expected | sv::drop(1)));
    };

    "pop from a full state for non-trivial types"_test = [] {
        CircBuffer<NonTrivialType>  circ{ 5 };
        std::vector<NonTrivialType> expected{ { 1 }, { 2 }, { 3 }, { 4 }, { 5 } };    // 5 insertion

        for (int i = 1; i <= 5; ++i) {
            circ.push(NonTrivialType{ i });
        }
        ut::expect(circ.size() == 5_ull);
        ut::expect(sr::equal(circ.buf(), expected));

        ut::expect(*circ.pop() == NonTrivialType{ 1 });
        ut::expect(circ.size() == 4_ull);
        ut::expect(circ.buf()[0] == NonTrivialType{}) << "pop-ed element should be reset to default";
        ut::expect(sr::equal(circ.buf() | sv::drop(1), expected | sv::drop(1)));
    };

    // I'll just skip the TrivialType case from this point onwards, since the moved-from state is unspecified

    "pop from a full state that has overwrite and the begin pointer is in the middle"_test = [] {
        CircBuffer<NonTrivialType> circ{ 5 };

        std::vector<NonTrivialType> beforePop{
            { 6 }, { 7 }, { 3 }, { 4 }, { 5 },    // 3 is the begin/end pointer/index
        };    // 7 insertion; 6, 7 will overwrite 1, 2
        std::vector<NonTrivialType> afterPop{
            { 6 }, { 7 }, {}, { 4 }, { 5 },    // 3 is moved-from; defaulted
        };
        std::vector<NonTrivialType> afterPushAfterPop{
            { 6 }, { 7 }, { 8 }, { 4 }, { 5 },    // 3 contains new value
        };
        std::vector<NonTrivialType> afterPopAfterPopAfterPushAfterPop{
            { 6 }, { 7 }, { 8 }, {}, {},    // 4 and 5 are moved-from; defaulted
        };
        std::vector<NonTrivialType> afterPoppingAll{ {}, {}, {}, {}, {} };                // end at 4
        std::vector<NonTrivialType> afterPushAfterPoppingAll{ {}, {}, {}, { 9 }, {} };    // end is filled

        for (int i = 1; i <= 7; ++i) {
            circ.push(NonTrivialType{ i });
        }
        ut::expect(circ.size() == 5_ull);
        ut::expect(sr::equal(circ.buf(), beforePop));

        ut::expect(*circ.pop() == NonTrivialType{ 3 }) << "pop-ed element should have value 3";
        ut::expect(circ.size() == 4_ull);
        ut::expect(circ.buf()[2] == NonTrivialType{}) << "pop-ed element should be reset to default";

        ut::expect(*circ.push(NonTrivialType{ 8 }) == NonTrivialType{ 8 })
            << "push should return the pushed element";

        ut::expect(circ.size() == 5_ull);
        ut::expect(sr::equal(circ.buf(), afterPushAfterPop));

        ut::expect(*circ.pop() == NonTrivialType{ 4 });
        ut::expect(*circ.pop() == NonTrivialType{ 5 });
        ut::expect(circ.size() == 3_ull);
        ut::expect(sr::equal(circ.buf(), afterPopAfterPopAfterPushAfterPop));

        while (circ.pop() != std::nullopt) { }
        ut::expect(circ.size() == 0_ull);
        ut::expect(sr::equal(circ.buf(), afterPoppingAll));

        ut::expect(*circ.push(NonTrivialType{ 9 }) == NonTrivialType{ 9 });
        ut::expect(circ.size() == 1_ull);
        ut::expect(sr::equal(circ.buf(), afterPushAfterPoppingAll));
    };

    "pop from a full state that has overwrite and the begin is at the boundary of the buffer"_test = [] {
        CircBuffer<NonTrivialType> circ{ 5 };

        std::vector<NonTrivialType> beforePop{
            { 11 }, { 12 }, { 13 }, { 14 }, { 10 },    // begin/end at 10
        };    // 14 insertion; 11, 12, 13, 14 overwrite previous val
        std::vector<NonTrivialType> afterPop{
            { 11 }, { 12 }, { 13 }, { 14 }, {},    // 10 is moved-from; defaulted
        };
        std::vector<NonTrivialType> afterPopAfterPop{
            {}, { 12 }, { 13 }, { 14 }, {},    // 11 is moved-from; defaulted
        };
        std::vector<NonTrivialType> afterPushPushPush{
            { 16 }, { 17 }, { 13 }, { 14 }, { 15 },    // 3 insertions; 15, 16, 17 overwrite {}, 12
        };

        for (int i = 1; i <= 14; ++i) {
            circ.push(NonTrivialType{ i });
        }

        ut::expect(circ.size() == 5_ull);
        ut::expect(sr::equal(circ.buf(), beforePop));

        ut::expect(*circ.pop() == NonTrivialType{ 10 });
        ut::expect(circ.size() == 4_ull);
        ut::expect(circ.buf()[4] == NonTrivialType{}) << "pop-ed element should be reset to default";
        ut::expect(sr::equal(circ.buf(), afterPop));

        ut::expect(*circ.pop() == NonTrivialType{ 11 });
        ut::expect(circ.size() == 3_ull);
        ut::expect(circ.buf()[0] == NonTrivialType{}) << "pop-ed element should be reset to default";
        ut::expect(circ.begin() == 1_ull);
        ut::expect(circ.end() == 4_ull);
        ut::expect(sr::equal(circ.buf(), afterPopAfterPop));

        for (int i = 15; i <= 17; ++i) {
            circ.push(NonTrivialType{ i });
        }
        ut::expect(circ.size() == 5_ull);
        ut::expect(static_cast<std::size_t>(circ.begin()) == 2_ull);
        ut::expect(static_cast<std::size_t>(circ.end()) == CircBuffer<NonTrivialType>::npos);
        ut::expect(sr::equal(circ.buf(), afterPushPushPush));
    };

    "iter through full buffer should start from the begin pointer and stop at the end pointer"_test = [] {
        CircBuffer<int> circ{ 5 };
        std::vector     internalBuf{ 6, 7, 3, 4, 5 };      // values in memory; begin/end at 3
        std::vector     outwardAppear{ 3, 4, 5, 6, 7 };    // values seen from iterating the buffer

        for (int i = 1; i <= 7; ++i) {
            circ.push(std::move(i));
        }

        ut::expect(sr::equal(circ.buf(), internalBuf));
        ut::expect(sr::equal(circ, outwardAppear));
    };

    "iter through non-full buffer should start from the begin pointer and stop at the end pointer"_test = [] {
        CircBuffer<NonTrivialType>  circ{ 5 };
        std::vector<NonTrivialType> internalBuf{ 6, 7, 0, 0, 5 };    // begin at 5, end at 7
        std::vector<NonTrivialType> outwardAppear{ 5, 6, 7 };        // values seen from iterating the buffer

        for (int i = 1; i <= 7; ++i) {
            circ.push(std::move(i));
        }
        std::ignore = circ.pop();
        std::ignore = circ.pop();

        ut::expect(sr::equal(circ.buf(), internalBuf));
        ut::expect(sr::equal(circ, outwardAppear));
    };

    "linearize a non-full buffer with the begin pointer at start should return the buffer as is"_test = [] {
        CircBuffer<int> circ{ 5 };
        std::vector     buf{ 1, 2, 3, 4, 0 };

        for (int i = 1; i <= 4; ++i) {
            circ.push(std::move(i));
        }
        ut::expect(sr::equal(circ.buf(), buf));

        // copy
        auto newCirc = circ.linearizeCopy();
        ut::expect(sr::equal(newCirc.buf(), buf));

        // in-place
        circ.linearize();
        ut::expect(sr::equal(circ.buf(), buf));

        ut::expect(sr::equal(circ.buf(), newCirc.buf()));
    };

    "linearize a non-full buffer with the begin pointer at the middle should move values to start"_test = [] {
        CircBuffer<NonTrivialType>  circ{ 5 };
        std::vector<NonTrivialType> buf{ 6, 0, 0, 4, 5 };
        std::vector<NonTrivialType> bufAfterLinearize{ 4, 5, 6, 0, 0 };

        for (int i = 1; i <= 6; ++i) {
            circ.push(NonTrivialType{ i });
        }
        std::ignore = circ.pop();
        std::ignore = circ.pop();

        ut::expect(sr::equal(circ.buf(), buf));

        // copy
        auto newCirc = circ.linearizeCopy();
        ut::expect(sr::equal(newCirc.buf(), bufAfterLinearize));

        // in-place
        circ.linearize();
        ut::expect(sr::equal(circ.buf(), bufAfterLinearize));

        ut::expect(sr::equal(circ.buf(), newCirc.buf()));
    };

    "linearize a full buffer with the begin pointer at the start should return the buffer as is"_test = [] {
        CircBuffer<int> circ{ 5 };
        std::vector     buf{ 1, 2, 3, 4, 5 };

        for (int i = 1; i <= 5; ++i) {
            circ.push(std::move(i));
        }
        ut::expect(sr::equal(circ.buf(), buf));

        // copy
        auto newCirc = circ.linearizeCopy();
        ut::expect(sr::equal(newCirc.buf(), buf));

        // in-place
        circ.linearize();
        ut::expect(sr::equal(circ.buf(), buf));

        ut::expect(sr::equal(circ.buf(), newCirc.buf()));
    };

    "linearize a full buffer with the begin pointer at the middle should move values to start"_test = [] {
        CircBuffer<NonTrivialType>  circ{ 5 };
        std::vector<NonTrivialType> buf{ 6, 7, 3, 4, 5 };
        std::vector<NonTrivialType> bufAfterLinearize{ 3, 4, 5, 6, 7 };

        for (int i = 1; i <= 7; ++i) {
            circ.push(NonTrivialType{ i });
        }

        ut::expect(sr::equal(circ.buf(), buf));

        // copy
        auto newCirc = circ.linearizeCopy();
        ut::expect(sr::equal(newCirc.buf(), bufAfterLinearize));

        // in-place
        circ.linearize();
        ut::expect(sr::equal(circ.buf(), bufAfterLinearize));

        ut::expect(sr::equal(circ.buf(), newCirc.buf()));
    };

    "reset should do nothing to the buffer but reset the begin and end pointer to start"_test = [] {
        CircBuffer<NonTrivialType>  circ{ 5 };
        std::vector<NonTrivialType> buf{ { 6 }, {}, {}, { 4 }, { 5 } };    // 6 insertions; 2 pops
        std::vector<NonTrivialType> bufAfterPushAfterReset{ { 7 }, { 8 }, {}, { 4 }, { 5 } };    // 2 insert

        for (int i = 1; i <= 6; ++i) {
            circ.push(NonTrivialType{ i });
        }
        std::ignore = circ.pop();
        std::ignore = circ.pop();

        ut::expect(circ.size() == 3_ull);
        ut::expect(sr::equal(circ.buf(), buf));

        circ.reset();
        ut::expect(circ.size() == 0_ull);
        ut::expect(circ.begin() == 0_ull);
        ut::expect(circ.end() == 0_ull);
        ut::expect(sr::equal(circ.buf(), buf));

        circ.push(NonTrivialType{ 7 });
        circ.push(NonTrivialType{ 8 });

        ut::expect(circ.size() == 2_ull);
        ut::expect(sr::equal(circ.buf(), bufAfterPushAfterReset));
    };

    "clear should reset the elements to its default state"_test = [] {
        CircBuffer<NonTrivialType>  circ{ 5 };
        std::vector<NonTrivialType> buf{ { 6 }, {}, {}, { 4 }, { 5 } };     // 6 insertions; 2 pops
        std::vector<NonTrivialType> bufAfterClear{ {}, {}, {}, {}, {} };    // all elements are reset

        for (int i = 1; i <= 6; ++i) {
            circ.push(NonTrivialType{ i });
        }
        std::ignore = circ.pop();
        std::ignore = circ.pop();

        ut::expect(circ.size() == 3_ull);
        ut::expect(sr::equal(circ.buf(), buf));

        circ.clear();
        ut::expect(circ.size() == 0_ull);
        ut::expect(circ.begin() == 0_ull);
        ut::expect(circ.end() == 0_ull);
        ut::expect(sr::equal(circ.buf(), bufAfterClear));
    };

    "resize non-full buffer to bigger capacity change order in memory when begin not at start"_test = [] {
        CircBuffer<NonTrivialType>  circ{ 5 };
        std::vector<NonTrivialType> buf{ 6, 7, 0, 0, 5 };                              // begin at 5, end at 7
        std::vector<NonTrivialType> bufAfterResize{ 5, 6, 7, 0, 0, 0, 0, 0, 0, 0 };    // 5 -> 10
        std::vector<NonTrivialType> bufAfterPushAfterResize{ 5, 6, 7, 8, 0, 0, 0, 0, 0, 0 };

        for (int i = 1; i <= 7; ++i) {
            circ.push(std::move(i));
        }
        std::ignore = circ.pop();
        std::ignore = circ.pop();

        ut::expect(circ.size() == 3_ull);
        ut::expect(sr::equal(circ.buf(), buf));

        circ.resize(10);
        ut::expect(circ.size() == 3_ull);
        ut::expect(circ.begin() == 0_ull);
        ut::expect(circ.end() == 3_ull);
        ut::expect(sr::equal(circ.buf(), bufAfterResize));

        ut::expect(*circ.push(NonTrivialType{ 8 }) == NonTrivialType{ 8 });
        ut::expect(circ.size() == 4_ull);
        ut::expect(sr::equal(circ.buf(), bufAfterPushAfterResize));
    };

    "resize full buffer to bigger capacity should change order in memory when begin not at start"_test = [] {
        CircBuffer<NonTrivialType>  circ{ 5 };
        std::vector<NonTrivialType> buf{ 11, 12, 13, 14, 10 };    // begin at 10, end at 14
        std::vector<NonTrivialType> bufAfterResize{ 10, 11, 12, 13, 14, 0, 0, 0, 0, 0 };
        std::vector<NonTrivialType> bufAfterPushAfterResize{ 10, 11, 12, 13, 14, 15, 0, 0, 0, 0 };

        for (int i = 1; i <= 14; ++i) {
            circ.push(NonTrivialType{ i });
        }

        ut::expect(circ.size() == 5_ull);
        ut::expect(sr::equal(circ.buf(), buf));

        circ.resize(10);
        ut::expect(circ.size() == 5_ull);
        ut::expect(circ.begin() == 0_ull);
        ut::expect(circ.end() == 5_ull);
        ut::expect(sr::equal(circ.buf(), bufAfterResize));

        ut::expect(*circ.push(NonTrivialType{ 15 }) == NonTrivialType{ 15 });
        ut::expect(circ.size() == 6_ull);
        ut::expect(sr::equal(circ.buf(), bufAfterPushAfterResize));
    };

    "resize non-full buffer to smaller capacity and number of elements less than new capacity"_test = [] {
        CircBuffer<NonTrivialType>  circ{ 10 };
        std::vector<NonTrivialType> buf{ 11, 12, 0, 0, 0, 0, 0, 0, 9, 10 };    // 12 insertions; 6 pops
        std::vector<NonTrivialType> bufAfterResize{ 9, 10, 11, 12, 0 };        // 10 -> 5

        for (int i = 1; i <= 12; ++i) {
            circ.push(NonTrivialType{ i });
        }
        for (int i = 1; i <= 6; ++i) {
            std::ignore = circ.pop();
        }

        ut::expect(circ.size() == 4_ull);
        ut::expect(sr::equal(circ.buf(), buf));

        using R = decltype(circ)::ResizePolicy;

        auto tmp = circ;
        tmp.resize(5, R::DISCARD_OLD);
        ut::expect(tmp.size() == 4_ull);
        ut::expect(sr::equal(tmp.buf(), bufAfterResize))
            << fmt::format("{} vs {}", tmp.buf(), bufAfterResize);

        tmp = circ;
        tmp.resize(5, R::DISCARD_NEW);
        ut::expect(tmp.size() == 4_ull);
        ut::expect(sr::equal(tmp.buf(), bufAfterResize))
            << fmt::format("{} vs {}", tmp.buf(), bufAfterResize);
    };

    "resize non-full buffer to smaller capacity and number of elements greater than new capacity"_test = [] {
        CircBuffer<NonTrivialType>  circ{ 10 };
        std::vector<NonTrivialType> buf{ 11, 12, 13, 14, 15, 0, 0, 0, 9, 10 };      // 15 insertions; 3 pops
        std::vector<NonTrivialType> bufAfterResizeDropOld{ 11, 12, 13, 14, 15 };    // 10 -> 5
        std::vector<NonTrivialType> bufAfterResizeDropNew{ 9, 10, 11, 12, 13 };     // 10 -> 5

        for (int i = 1; i <= 15; ++i) {
            circ.push(NonTrivialType{ i });
        }
        for (int i = 1; i <= 3; ++i) {
            std::ignore = circ.pop();
        }

        ut::expect(circ.size() == 7_ull);
        ut::expect(sr::equal(circ.buf(), buf));

        using R = decltype(circ)::ResizePolicy;

        auto tmp = circ;
        tmp.resize(5, R::DISCARD_OLD);
        ut::expect(tmp.size() == 5_ull);
        ut::expect(sr::equal(tmp.buf(), bufAfterResizeDropOld))
            << fmt::format("{} vs {}", tmp.buf(), bufAfterResizeDropOld);

        tmp = circ;
        tmp.resize(5, R::DISCARD_NEW);
        ut::expect(tmp.size() == 5_ull);
        ut::expect(sr::equal(tmp.buf(), bufAfterResizeDropNew))
            << fmt::format("{} vs {}", tmp.buf(), bufAfterResizeDropNew);
    };

    "resize full buffer to a bigger capacity should move each element to start of new buffer"_test = [] {
        CircBuffer<NonTrivialType>  circ{ 5 };
        std::vector<NonTrivialType> buf{ 11, 12, 13, 14, 10 };    // begin at 10, end at 14
        std::vector<NonTrivialType> bufAfterResize{ 10, 11, 12, 13, 14, 0, 0, 0, 0, 0 };

        for (int i = 1; i <= 14; ++i) {
            circ.push(NonTrivialType{ i });
        }

        ut::expect(circ.size() == 5_ull);
        ut::expect(sr::equal(circ.buf(), buf));

        circ.resize(10);
        ut::expect(circ.size() == 5_ull);
        ut::expect(circ.begin() == 0_ull);
        ut::expect(circ.end() == 5_ull);
        ut::expect(sr::equal(circ.buf(), bufAfterResize));
    };

    "resize full buffer to a smaller capacity should move each element according to policy"_test = [] {
        CircBuffer<NonTrivialType>  circ{ 12 };
        std::vector<NonTrivialType> buf{ 13, 14, 15, 16, 17, 6, 7, 8, 9, 10, 11, 12 };    // 15 insertions
        std::vector<NonTrivialType> bufAfterResizeDropOld{ 13, 14, 15, 16, 17 };          // 10 -> 5
        std::vector<NonTrivialType> bufAfterResizeDropNew{ 6, 7, 8, 9, 10 };              // 10 -> 5

        for (int i = 1; i <= 17; ++i) {
            circ.push(NonTrivialType{ i });
        }

        ut::expect(circ.size() == 12_ull);
        ut::expect(sr::equal(circ.buf(), buf));

        using R = decltype(circ)::ResizePolicy;

        auto tmp = circ;
        tmp.resize(5, R::DISCARD_OLD);
        ut::expect(tmp.size() == 5_ull);
        ut::expect(sr::equal(tmp.buf(), bufAfterResizeDropOld))
            << fmt::format("{} vs {}", tmp.buf(), bufAfterResizeDropOld);

        tmp = circ;
        tmp.resize(5, R::DISCARD_NEW);
        ut::expect(tmp.size() == 5_ull);
        ut::expect(sr::equal(tmp.buf(), bufAfterResizeDropNew))
            << fmt::format("{} vs {}", tmp.buf(), bufAfterResizeDropNew);
    };
}
