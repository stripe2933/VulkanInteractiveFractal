module;

#define FWD(...) std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)

export module vkbase:range_utils;

import std;

namespace vkbase::ru {
    // TODO.CXX23: remove this when std::ranges::range_adaptor_closure available.
    template <typename Self>
    struct range_adaptor_closure {
        template <std::ranges::range R>
        friend constexpr auto operator|(R &&r, const Self &self) {
            return self(FWD(r));
        }
    };

    // TODO.CXX23: Remove this when std::views::enumerate available.
    template <std::integral T>
    struct enumerate_fn : range_adaptor_closure<enumerate_fn<T>> {
        template <std::ranges::input_range R>
        constexpr auto operator()(R &&r) const {
            return std::views::zip(std::views::iota(T { 0 }), FWD(r));
        }
    };
}

export namespace vkbase::ru {
    template <std::integral T = std::size_t>
    enumerate_fn<T> enumerate;

    template <std::integral T>
    constexpr auto upto(T n) {
        return std::views::iota(T { 0 }, n);
    }
}