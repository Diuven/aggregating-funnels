#pragma once

#include <tuple>
#include <stdexcept>
#include <string_view>

namespace mpg {

template<class, class>
struct CatImpl;

template<class... T>
struct TypeSet {
    using Tuple = std::tuple<T...>;

    template<size_t i>
    using Get = std::tuple_element_t<i, Tuple>;

    template<class... T2>
    using Append = TypeSet<T..., T2...>;

    template<class Ts>
    using Cat = typename CatImpl<TypeSet<T...>, Ts>::Type;

    template<class F>
    static constexpr void foreach(F func) {
        (func.template operator()<T>(), ...);
    }
};

template<class... T1, class... T2>
struct CatImpl<TypeSet<T1...>, TypeSet<T2...>> {
    using Type = TypeSet<T1..., T2...>;
};

template<class ValueType>
struct ParameterSet {
    template<ValueType... values>
    struct Of {
    public:
        static constexpr std::array Values = {values...};

        using ValueConstants = TypeSet<std::integral_constant<ValueType, values>...>;

        template<class F>
        static constexpr void foreach(F f) {
            ValueConstants::foreach([&f]<class C>() {
                f.template operator()<C::value>();
            });
        }

        template<class F>
        static constexpr void apply(ValueType value, F&& f) {
            foreach([value, f = std::forward<F>(f)]<ValueType v>() mutable {
                if (v == value) {
                    std::forward<F>(f).template operator()<v>();
                }
            });
        }
    };
};

}