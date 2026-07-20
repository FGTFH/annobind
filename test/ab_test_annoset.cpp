#include <print>

#include "annobind/ab_attr.hpp"
#include "annobind/annobind.hpp"

using namespace annobind::literals;
constexpr annobind::skip v_a{};
constexpr annobind::rename v_b{"b"};

NB_MODULE(PROJECT_NAME, m) {
    static constexpr auto anns = [] {
        annobind::annoset as{1, 2, "hello"_cs, 5.23};
        as = annobind::annoset::concat(as, as);
        as[0] = std::meta::reflect_constant(10);
        as.extend(annobind::annoset{v_a, v_b});

        return as;
    }();
    static constexpr annobind::annoset annsx{};
    static constexpr std::span<const std::meta::info> sp(anns);
    static constexpr annobind::annoset anns2(sp);
    static constexpr auto vals = [&] {
        return [&]<size_t... Is>(std::index_sequence<Is...>) {
            return std::tuple([:std::meta::constant_of(anns2[Is]):]...);
        }(std::make_index_sequence<anns2.size()>{});
    }();

    template for (constexpr auto i : std::views::indices((anns2.size() - 2))) { std::println("{}", std::get<i>(vals)); }
}