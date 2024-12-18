#include "test_util.hpp"

#include <circbuf/detail/raw_buffer.hpp>

#include <boost/ut.hpp>
#include <fmt/core.h>

#include <cassert>
#include <ranges>

namespace ut = boost::ut;
namespace rr = std::ranges;
namespace rv = rr::views;

template <test_util::TestClass Type>
void test()
{
    using namespace ut::operators;
    using namespace ut::literals;
    using ut::expect, ut::that;

    Type::reset_active_instance_count();

    "nrvo should happen"_test = [] {
        circbuf::detail::RawBuffer<Type> buffer{ 10 };
        for (auto i : rv::iota(0u, 10u)) {
            buffer.construct(i, 10 - i + 1);
        }

        for (auto i : rv::iota(0u, 10u)) {
            auto&& value = buffer.at(i);
            expect(that % value.value() == 10 - i + 1);
            fmt::println("stat: {}", value.stat());
        }

        if constexpr (Type::is_movable() or Type::is_copyable()) {
            for (auto i : rv::iota(0u, 10u)) {
                auto value = std::move(buffer.at(i));
                expect(that % value.value() == 10 - i + 1);
                expect(that % value.stat().nocopy() or not Type::s_movable) << fmt::format(
                    "copy shouldn't be made on '{}': {}", ut::reflection::type_name<Type>(), value.stat()
                );
                buffer.destroy(i);
            }
        } else {
            for (auto i : rv::iota(0u, 10u)) {
                buffer.destroy(i);
            }
        }
    };

    "unbalanced constructor/destructor means there is a bug in the code"_test = [] {
        expect(Type::active_instance_count() == 0_i) << "Unbalanced ctor/dtor detected!";
    };
}

int main()
{
    test_util::for_each_tuple<test_util::NonTrivialPermutations>([]<typename T>() { test<T>(); });
}
