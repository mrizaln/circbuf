#pragma once

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <fmt/std.h>

#include <concepts>
#include <limits>
#include <ostream>
#include <ranges>

namespace test_util
{
    struct ClassStatCounter;

    template <typename T>
    concept TestClass = requires(const T t) {
        // { auto{ T::s_movable } } -> std::same_as<bool>;
        // { auto{ T::s_copyable } } -> std::same_as<bool>;
        { t.value() } -> std::convertible_to<int>;
        { t.stat() } -> std::same_as<ClassStatCounter>;
    };

    struct ClassStatCounter
    {
        bool        m_defaulted       = false;
        std::size_t m_moveCtorCount   = 0;
        std::size_t m_copyCtorCount   = 0;
        std::size_t m_moveAssignCount = 0;
        std::size_t m_copyAssignCount = 0;

        std::size_t copycount() const { return m_copyCtorCount + m_copyAssignCount; }
        std::size_t movecount() const { return m_moveCtorCount + m_moveAssignCount; }
        bool        defaulted() const { return m_defaulted; }
        bool        nomove() const { return movecount() == 0; }
        bool        nocopy() const { return copycount() == 0; }

        friend auto format_as(const ClassStatCounter& stat)
        {
            return fmt::format(
                "{{ [d]: {}, [&&]: {}, [&]: {}, [=&&]: {}, [=&]: {} }}",
                stat.m_defaulted,
                stat.m_moveCtorCount,
                stat.m_copyCtorCount,
                stat.m_moveAssignCount,
                stat.m_copyAssignCount
            );
        }

        friend std::ostream& operator<<(std::ostream& os, const ClassStatCounter& csc)
        {
            return os << format_as(csc);
        }
    };

    template <
        bool DefaultConstructible,
        bool CopyConstructible,
        bool MoveConstructible,
        bool CopyAssignable,
        bool MoveAssignable>
    class NonTrivial
    {
    public:
        static constexpr bool s_movable  = MoveConstructible and MoveAssignable;
        static constexpr bool s_copyable = CopyConstructible and CopyAssignable;
        static constexpr bool s_default  = DefaultConstructible;

        ~NonTrivial()
        {
            --s_activeInstanceCount;
            m_value = npos + 1;
        }

        NonTrivial()
            requires DefaultConstructible
        {
            ++s_activeInstanceCount;
            m_stat.m_defaulted = true;
        }

        NonTrivial(int value)    // implicit conversion
            : m_value{ value }
        {
            ++s_activeInstanceCount;
        }

        NonTrivial(const NonTrivial& other)
            requires CopyConstructible
            : m_value{ other.m_value }
            , m_stat{ other.m_stat }
        {
            ++s_activeInstanceCount;
            ++m_stat.m_copyCtorCount;
        }

        NonTrivial(NonTrivial&& other) noexcept
            requires MoveConstructible
            : m_value{ std::exchange(other.m_value, npos) }
            , m_stat{ other.m_stat }
        {
            ++s_activeInstanceCount;
            ++m_stat.m_moveCtorCount;
        }

        NonTrivial& operator=(const NonTrivial& other)
            requires CopyAssignable
        {
            m_value = other.m_value;
            m_stat  = other.m_stat;
            ++m_stat.m_copyAssignCount;
            return *this;
        }

        NonTrivial& operator=(NonTrivial&& other) noexcept
            requires MoveAssignable
        {
            m_value = std::exchange(other.m_value, npos);
            m_stat  = other.m_stat;
            ++m_stat.m_moveAssignCount;
            return *this;
        }

        int              value() const { return m_value; }
        ClassStatCounter stat() const { return m_stat; }

        friend std::ostream& operator<<(std::ostream& os, const NonTrivial& nt) { return os << nt.value(); }
        friend int           format_as(const NonTrivial& nt) { return nt.value(); }

        // to be able to compare NonTrivial objects even if they have different template parameters
        auto operator<=>(const auto& other) const { return value() <=> other.value(); }
        auto operator==(const auto& other) const { return value() == other.value(); }

        static int  activeInstanceCount() { return s_activeInstanceCount; }
        static void resetActiveInstanceCount() { s_activeInstanceCount = 0; }

    private:
        static constexpr int npos                  = std::numeric_limits<int>::min();
        static inline int    s_activeInstanceCount = 0;

        int                      m_value = npos;
        mutable ClassStatCounter m_stat  = {};
    };

    using Regular = NonTrivial<true, true, true, true, true>;

    template <bool DefaultConstructible = true>
    using MovableOnly = NonTrivial<DefaultConstructible, false, true, false, true>;

    template <bool DefaultConstructible = true>
    using CopyableOnly = NonTrivial<DefaultConstructible, true, false, true, false>;

    template <bool DefaultConstructible = true>
    using MovableAndCopyable = NonTrivial<DefaultConstructible, true, true, true, true>;

    static_assert(std::regular<Regular>);
    static_assert(std::movable<MovableOnly<>>);
    static_assert(std::copyable<CopyableOnly<>>);

    // it would be cool to have this permutations generated automatically...
    using NonTrivialPermutations = std::tuple<
        NonTrivial<1, 1, 1, 1, 1>,
        NonTrivial<1, 1, 1, 1, 0>,
        NonTrivial<1, 1, 1, 0, 1>,
        NonTrivial<1, 1, 1, 0, 0>,
        NonTrivial<1, 1, 0, 1, 1>,
        NonTrivial<1, 1, 0, 1, 0>,
        NonTrivial<1, 1, 0, 0, 1>,
        NonTrivial<1, 1, 0, 0, 0>,
        NonTrivial<1, 0, 1, 1, 1>,
        NonTrivial<1, 0, 1, 1, 0>,
        NonTrivial<1, 0, 1, 0, 1>,
        NonTrivial<1, 0, 1, 0, 0>,
        NonTrivial<1, 0, 0, 1, 1>,
        NonTrivial<1, 0, 0, 1, 0>,
        NonTrivial<1, 0, 0, 0, 1>,
        NonTrivial<1, 0, 0, 0, 0>,
        NonTrivial<0, 1, 1, 1, 1>,
        NonTrivial<0, 1, 1, 1, 0>,
        NonTrivial<0, 1, 1, 0, 1>,
        NonTrivial<0, 1, 1, 0, 0>,
        NonTrivial<0, 1, 0, 1, 1>,
        NonTrivial<0, 1, 0, 1, 0>,
        NonTrivial<0, 1, 0, 0, 1>,
        NonTrivial<0, 1, 0, 0, 0>,
        NonTrivial<0, 0, 1, 1, 1>,
        NonTrivial<0, 0, 1, 1, 0>,
        NonTrivial<0, 0, 1, 0, 1>,
        NonTrivial<0, 0, 1, 0, 0>,
        NonTrivial<0, 0, 0, 1, 1>,
        NonTrivial<0, 0, 0, 1, 0>,
        NonTrivial<0, 0, 0, 0, 1>,
        NonTrivial<0, 0, 0, 0, 0>>;

    template <typename Tuple, typename Fn>
    inline constexpr auto forEach(Fn&& fn)
    {
        const auto handler = [&]<std::size_t... I>(std::index_sequence<I...>) {
            (fn.template operator()<std::tuple_element_t<I, Tuple>>(), ...);
        };
        handler(std::make_index_sequence<std::tuple_size_v<Tuple>>());
    }

    auto subrange(auto&& range, std::size_t start, std::size_t end)
    {
        namespace rv = std::views;
        return range | rv::drop(start) | rv::take(end - start);
    }

    template <template <typename> typename C, typename T, std::ranges::range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, T>
    void populateContainer(C<T>& list, R&& range)
    {
        for (auto value : range) {
            list.push_back(std::move(value));
        }
    }

    template <template <typename> typename C, typename T, std::ranges::range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, T>
    void populateContainerFront(C<T>& list, R&& range)
    {
        for (auto value : range) {
            list.push_front(std::move(value));
        }
    }

    template <test_util::TestClass Type, std::ranges::range R1, std::ranges::range R2>
        requires std::same_as<std::ranges::range_value_t<R1>, Type>
              && std::same_as<std::ranges::range_value_t<R2>, int>
    bool equalUnderlying(R1&& actual, R2&& expected)
    {
        namespace rr = std::ranges;
        namespace rv = std::views;
        return rr::equal(actual | rv::transform(&Type::value), expected);
    }
}
