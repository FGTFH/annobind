#pragma once
#include <meta>
#include <type_traits>
namespace annobind {
namespace meta = std::meta;

namespace detail {

consteval meta::info pure_type_of(meta::info member) { return meta::remove_cvref(meta::type_of(member)); }

using meta::is_nonstatic_data_member;
template <meta::info member>
consteval bool is_bindable() {
    if ((meta::is_type(member) || meta::is_type_alias(member) || meta::is_variable(member) ||
         meta::is_function(member) || meta::is_namespace(member) || meta::is_nonstatic_data_member(member))) {
        if constexpr (meta::is_enumerable_type(member)) {
            if constexpr (!std::is_enum_v<typename[:member:]>) {
                for (const auto& m : meta::nonstatic_data_members_of(member, meta::access_context::current())) {
                    if (meta::identifier_of(m) == "nb_trampoline") return false;  // 排除nb模板类
                }
            }
        }
        return true;
    }
    return false;
};

consteval bool is_static_data_member(meta::info member) {
    return meta::is_static_member(member) && !meta::is_function(member);
}

consteval bool is_data_member(meta::info member) {
    return is_static_data_member(member) || is_nonstatic_data_member(member);
}

// 排除特殊函数 析构/复制构造/移动构造/赋值等
consteval bool is_bindable_member_function(meta::info member) {
    return meta::is_function(member) &&
           (!meta::is_special_member_function(member) || meta::is_default_constructor(member));
}

consteval bool is_nested(meta::info member) { return (meta::is_enumerable_type(member)) || meta::is_namespace(member); }

// 只能通过 type_traits 简单区分 class 和 enum， 因此得用模板参数...
template <meta::info type>
consteval bool is_enum_type() {
    if constexpr (!meta::is_enumerable_type(type))
        return false;
    else
        return std::is_enum_v<typename[:type:]>;
}

template <meta::info type>
consteval bool is_non_enum_type() {
    if constexpr (!meta::is_enumerable_type(type))
        return false;
    else
        return !std::is_enum_v<typename[:type:]>;
}

}  // namespace detail
}  // namespace annobind
