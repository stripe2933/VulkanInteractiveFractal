module;

#define FWD(...) std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)

export module vkbase:ref_holder;

import std;

export namespace vkbase{
    template <typename T, typename... Ts>
    struct RefHolder {
        std::tuple<Ts...> temporaryValues;
        T value;

        template <std::invocable<Ts&...> F>
        RefHolder(F &&f, auto &&...temporaryValues)
            : temporaryValues { FWD(temporaryValues)... },
              value { std::apply(FWD(f), this->temporaryValues) } {

        }

        // TODO.CXX23: can be de-duplicated using explicit object parameter.
        constexpr operator T&() noexcept {
            return value;
        }
        constexpr operator const T&() const noexcept {
            return value;
        }

        constexpr auto get() noexcept -> T& {
            return value;
        }
        constexpr auto get() const noexcept -> const T& {
            return value;
        }
    };
}