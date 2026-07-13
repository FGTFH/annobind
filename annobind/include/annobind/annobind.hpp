#include <nanobind/nanobind.h>

#include <flat_map>
#include <meta>
#include <optional>
#include <print>
#include <string>
#include <tuple>

#include "ab_attr.hpp"
#include "ab_config.hpp"
#include "ab_parse.hpp"
#include "ab_register.hpp"

namespace annobind {

namespace nb = nanobind;
namespace meta = std::meta;

namespace detail {
// 若涉及到嵌套作用域，则 `nb_base` 被视为根作用域(视情况可以为nb模块/类)
// 带有 _b 后缀的模板参数, 如 ns_b, cls_b 意为可接受bind直接绑定声明

// 在 `nb_mod` 下创建基于 `ns_b` 的空子模块
template <meta::info ns_b>
    requires(meta::is_namespace(ns_b) || is_bind_declaration(ns_b))
auto make_nb_module(nb::module_& nb_mod, const std::string& identifier) {
    constexpr const char* name = detail::name_of<ns_b>();
    constexpr auto doctuple = detail::doctuple_of<ns_b, conf::auto_module_src>();
    static_assert(name[0] != '\0', "name must be specified");

    auto nb_mod_new = std::apply([&](const auto&... args) { return nb_mod.def_submodule(name, args...); }, doctuple);
    ABRegistry::register_module(nb_mod_new, identifier);
    return nb_mod_new;
}

// 在`scope`下创建基于`cls_b`的空类
template <meta::info cls_b>
    requires(detail::is_non_enum_type<cls_b>() || is_bind_declaration(cls_b))
auto make_nb_class(nb::handle& nb_scope) {
    // 提取选项
    constexpr auto cls = detail::take_bind<cls_b>();
    constexpr const char* cls_name = detail::name_of<cls_b>();
    constexpr auto extra_annons = detail::extral_annotations_of<cls_b, conf::auto_type_src>();
    constexpr meta::info trampoline = detail::get_annotation_ex<cls_b, trampo, trampo{meta::info{}}>().type;
    static_assert(cls_name[0] != '\0', "name must be specified");

    // 构建 nb::class_ 的模板参数列表
    constexpr auto access_ctx = meta::access_context::current();
    using cls_t = typename[:[&] {
        std::vector<meta::info> class_list{cls};
        if constexpr (trampoline != meta::info{}) {
            static_assert(meta::type_of(meta::bases_of(trampoline, access_ctx)[0]) == cls);
            class_list.push_back(trampoline);
        }
        for (auto base : meta::bases_of(cls, access_ctx)) class_list.push_back(meta::type_of(base));
        return meta::substitute(^^nb::class_, class_list);
    }():];

    auto nb_cls = std::apply([&](const auto&... args) { return cls_t(nb_scope, cls_name, args...); }, extra_annons);
    auto peeled = nb_class_peeled(nb_cls);
    ABRegistry::register_class(peeled);
    return peeled;
}

}  // namespace detail

/// 绑定枚举`enum_`到`nb_scp`(可为模块/类)
template <meta::info enum_b>
    requires(detail::is_enum_type<enum_b>() || is_bind_declaration(enum_b))
auto bind_enum(nb::handle& nb_scp);

// 绑定函数 `func` 到 `nb_mod`
template <meta::info func_b>
    requires(meta::is_function(func_b) || is_bind_declaration(func_b))
auto bind_function(nb::module_& nb_mod);

//  将命名空间成员 `mbr_b` 绑定到 `nb_mod` 模块. 可跨命名空间绑定
// `ns_b` 提供成员跳过检查标注, 默认为 `mbr_b` 所在的命名空间(别名的标注会丢失), 可为任意可标注的实体
// `skip_check` 用于标识自动绑定, 默认值false表示不进行skip相关标注的检查, 而强制进行绑定
template <meta::info mbr_b, meta::info ns_b = meta::info{}, bool skip_check = false>
    requires(meta::is_namespace_member(mbr_b) || is_bind_declaration(mbr_b))
auto bind_module_member(nb::module_& nb_mod, std::optional<nb::module_&> nb_base = std::nullopt);

//  绑定`nb_cls`对应类的成员 `mbr_b`.
// `cls_b` 与 `skip_check` 同 bind_module_member
template <meta::info mbr_b, meta::info cls_b = meta::info{}, bool skip_check = false>
    requires(meta::is_class_member(mbr_b) || is_bind_declaration(mbr_b))
auto bind_class_member(auto& nb_cls, std::optional<nb::handle&> nb_base = std::nullopt);

// 将类 `cls_b` 绑定到 `nb_scope`
template <meta::info cls_b>
    requires(detail::is_non_enum_type<cls_b>() || is_bind_declaration(cls_b))
auto bind_class(nb::handle& nb_scope, std::optional<nb::handle&> nb_base = std::nullopt);

// 基于 `ns_b` 创建`nb_mod`的子模块, 其中包含 `ns_b` 的成员
template <meta::info ns_b>
    requires(meta::is_namespace(ns_b) || is_bind_declaration(ns_b))
auto bind_submodule(nb::module_& nb_mod, std::optional<nb::module_&> nb_base = std::nullopt);

// 将命名空间 `ns_b` 的成员 绑定到 `nb_mod`
template <meta::info ns_b>
    requires(meta::is_namespace(ns_b) || is_bind_declaration(ns_b))
auto& bind_to_module(nb::module_& nb_mod, std::optional<nb::module_&> nb_base = std::nullopt);

//===================== 实现 =====================

template <meta::info enum_b>
    requires(detail::is_enum_type<enum_b>() || is_bind_declaration(enum_b))
auto bind_enum(nb::handle& nb_scp) {
    constexpr auto enum_ = detail::take_bind<enum_b>();
    using T = typename[:enum_:];
    if (auto nb_enum_old = ABRegistry::find_enum<T>()) {
        std::println("[annobind warn] enum {} already bound", meta::display_string_of(enum_));
        return *nb_enum_old;
    }

    constexpr const char* name = detail::name_of<enum_b>();
    constexpr auto extra_annons = detail::extral_annotations_of<enum_b, conf::auto_type_src>();
    constexpr bool export_values_v = detail::check_anno_bool_value_ex<enum_b, export_values, conf::auto_export_enum>();
    static_assert(name[0] != '\0', "name must be specified");

    auto nb_enum = std::apply([&](const auto&... args) { return nb::enum_<T>(nb_scp, name, args...); }, extra_annons);
    template for (constexpr auto& item : std::define_static_array(meta::enumerators_of(enum_))) {
        constexpr auto name = meta::identifier_of(item).data();
        constexpr auto value = [:item:];
        constexpr auto doc_tup = detail::extral_annotations_of<item, false>();  //  应该没必要自动带上srcloc了吧
        std::apply([&](const auto&... args) { nb_enum.value(name, value, args...); }, doc_tup);
    }
    if constexpr (export_values_v) {
        nb_enum.export_values();
    }
    ABRegistry::register_enum(nb_enum);
    return nb_enum;
}

template <meta::info func_b>
    requires(meta::is_function(func_b) || is_bind_declaration(func_b))
auto bind_function(nb::module_& nb_mod) {
    constexpr auto func = detail::take_bind<func_b>();
    constexpr const char* name = detail::name_of<func_b>();
    static_assert(name[0] != '\0', "name must be specified");

    auto args = detail::parse_func_annotations<func_b>();
    std::apply([&](const auto&... args) { nb_mod.def(name, &[:func:], args...); }, args);
}

template <meta::info mbr_b, meta::info ns_b, bool skip_check_>
    requires(meta::is_namespace_member(mbr_b) || is_bind_declaration(mbr_b))
auto bind_module_member(nb::module_& nb_mod, std::optional<nb::module_&> nb_base) {
    if (!nb_base.has_value()) nb_base = nb_mod;
    constexpr auto mbr = detail::take_bind<mbr_b>();
    constexpr auto skip_check = mbr != mbr_b ? false : skip_check_;
    constexpr auto ns = [] {
        if (ns_b == meta::info{}) return meta::parent_of(mbr);
        return ns_b;
    }();
    if constexpr (detail::is_bindable<mbr>()) {
        constexpr const char* name = detail::name_of<mbr_b>();

        if constexpr (meta::is_variable(mbr)) {
            // ---------------- 变量 ----------------
            constexpr bool df = detail::check_anno_bool_value_ex<ns, skip_data, conf::auto_skip_data>();
            if constexpr (!skip_check || !detail::check_anno_bool_value<mbr_b, skip, unskip, df>()) {
                auto v = [:mbr:];
                nb_mod.attr(name) = v;
            }
        } else if constexpr (meta::is_function(mbr)) {
            // ---------------- 函数 ----------------
            constexpr bool df = detail::check_anno_bool_value_ex<ns, skip_func, conf::auto_skip_func>();
            if constexpr (!skip_check || !detail::check_anno_bool_value<mbr_b, skip, unskip, df>()) {
                return bind_function<mbr_b>(nb_mod);
            }
        } else if constexpr (detail::is_nested(mbr)) {
            // ---------------- 命名空间/类型 ----------------
            constexpr bool df = meta::dealias(mbr) != mbr
                                    ? skip_check
                                    : detail::check_anno_bool_value_ex<ns, skip_nested, conf::auto_skip_nested>();
            if constexpr (!skip_check || !detail::check_anno_bool_value<mbr_b, skip, unskip, df>()) {
                constexpr bool plain_bind = detail::check_anno_bool_value_ex<mbr_b, plain, conf::auto_plain_module>();

                auto& to_scope = [&]() -> auto& {
                    if constexpr (plain_bind)
                        return *nb_base;
                    else
                        return nb_mod;
                }();
                if constexpr (detail::is_enum_type<mbr>()) {
                    return bind_enum<mbr_b>(to_scope);
                } else if constexpr (meta::is_complete_type(mbr)) {
                    return bind_class<mbr_b>(to_scope, *nb_base);
                } else if constexpr (meta::is_namespace(mbr)) {
                    if constexpr (plain_bind)
                        return bind_to_module<mbr_b>(*nb_base, *nb_base);
                    else
                        return bind_submodule<mbr_b>(nb_mod, *nb_base);
                }
            }
        }
    }
}

template <meta::info mbr_b, meta::info cls_b, bool skip_check_>
    requires(meta::is_class_member(mbr_b) || is_bind_declaration(mbr_b))
auto bind_class_member(auto& nb_cls, std::optional<nb::handle&> nb_base) {
    if (!nb_base.has_value()) nb_base = nb_cls;
    constexpr auto mbr = detail::take_bind<mbr_b>();
    constexpr auto skip_check = mbr != mbr_b ? false : skip_check_;
    constexpr auto cls = [&] {
        if (cls_b == meta::info{}) return meta::parent_of(mbr);
        return cls_b;
    }();
    if constexpr (detail::is_bindable<mbr>()) {
        constexpr const char* name = detail::name_of<mbr_b>();

        if constexpr (detail::is_data_member(mbr)) {
            // ---------------- 数据成员 ----------------
            constexpr bool df = detail::check_anno_bool_value_ex<cls, skip_data, conf::auto_skip_data>();
            if constexpr (!skip_check || !detail::check_anno_bool_value<mbr_b, skip, unskip, df>()) {
                constexpr auto doc_tup = detail::doctuple_of<mbr_b, conf::auto_data_member_src>();

                if constexpr (detail::is_nonstatic_data_member(mbr)) {
                    // ---------- 非静态数据成员 ----------
                    if constexpr (meta::is_const(mbr))
                        std::apply([&](const auto&... srcloc) { nb_cls.def_ro(name, &[:mbr:], srcloc...); }, doc_tup);
                    else
                        std::apply([&](const auto&... srcloc) { nb_cls.def_rw(name, &[:mbr:], srcloc...); }, doc_tup);

                } else if constexpr (detail::is_static_data_member(mbr)) {
                    // ---------- 静态数据成员 ----------
                    if constexpr (meta::is_const(mbr)) {
                        std::apply([&](const auto&... srcloc) { nb_cls.def_ro_static(name, &[:mbr:], srcloc...); },
                                   doc_tup);
                    } else
                        std::apply([&](const auto&... srcloc) { nb_cls.def_rw_static(name, &[:mbr:], srcloc...); },
                                   doc_tup);
                }
            }

        } else if constexpr (detail::is_bindable_member_function(mbr)) {
            // ---------- 函数成员 ----------
            constexpr bool df = detail::check_anno_bool_value_ex<cls, skip_func, conf::auto_skip_func>();
            if constexpr (!skip_check || !detail::check_anno_bool_value<mbr_b, skip, unskip, df>()) {
                auto extra_args = detail::parse_func_annotations<mbr>();

                if constexpr (meta::is_constructor(mbr)) {
                    // ---------- 构造函数 ----------
                    using init_type =
                        typename[:meta::substitute(^^nb::init,
                                                   std::define_static_array(meta::parameters_of(mbr) |
                                                                            std::views::transform([](auto p) {
                                                                                return meta::type_of(p);
                                                                            }))):];
                    std::apply(
                        [&](const auto&... args) { nb_cls.def(init_type(), std::forward<decltype(args)>(args)...); },
                        extra_args);

                } else {
                    // ---------- 具名函数/运算符 ----------
                    if constexpr (meta::is_static_member(mbr)) {
                        std::apply(
                            [&](const auto&... args) {
                                nb_cls.def_static(name, &[:mbr:], std::forward<decltype(args)>(args)...);
                            },
                            extra_args);
                    } else {
                        std::apply(
                            [&](const auto&... args) {
                                nb_cls.def(name, &[:mbr:], std::forward<decltype(args)>(args)...);
                            },
                            extra_args);
                    }
                }
            }

        } else if constexpr (meta::is_enumerable_type(mbr)) {
            // ---------------- 嵌套类型 ----------------
            constexpr bool df = meta::dealias(mbr) != mbr
                                    ? skip_check
                                    : detail::check_anno_bool_value_ex<cls, skip_nested, conf::auto_skip_nested>();
            if constexpr (!skip_check || !detail::check_anno_bool_value<mbr_b, skip, unskip, df>()) {
                constexpr bool plain_bind = detail::check_anno_bool_value_ex<mbr_b, plain, conf::auto_plain_module>();
                auto& to_scope = [&]() -> auto& {
                    if constexpr (plain_bind)
                        return *nb_base;
                    else
                        return nb_cls;
                }();
                if constexpr (detail::is_enum_type<mbr>()) {
                    return bind_enum<mbr_b>(to_scope);
                } else if constexpr (meta::is_complete_type(mbr)) {
                    return bind_class<mbr_b>(to_scope, *nb_base);
                }
            }
        }
    }
}

template <meta::info cls_b>
    requires(detail::is_non_enum_type<cls_b>() || is_bind_declaration(cls_b))
auto bind_class(nb::handle& nb_scope, std::optional<nb::handle&> nb_base) {
    if (!nb_base.has_value()) nb_base = nb_scope;
    constexpr auto cls = detail::take_bind<cls_b>();
    if (auto nb_cls_old = ABRegistry::find_class<typename[:cls:]>()) {
        std::println("[annobind warn] class {} already bound", meta::display_string_of(meta::dealias(cls)));
        return *nb_cls_old;
    }

    auto nb_cls = detail::make_nb_class<cls_b>(nb_scope);
    static constexpr auto access_ctx = meta::access_context::current();

    struct PropInfo {
        meta::info getter;
        meta::info setter;
    };
    static constexpr auto [props, members] = [] {
        std::flat_map<std::string_view, PropInfo> props;
        std::vector<meta::info> bindmems;

        template for (constexpr auto& member : std::define_static_array(meta::members_of(cls, access_ctx))) {
            if constexpr (detail::is_bindable<member>()) {
                constexpr auto prop_ann = std::define_static_array(meta::annotations_of_with_type(member, ^^property));
                if constexpr (prop_ann.empty())
                    bindmems.push_back(member);
                else {
                    constexpr auto func = [] {
                        if constexpr (is_bind_declaration(member))
                            return [:member:].obj;
                        else
                            return member;
                    }();
                    constexpr auto paramN = meta::parameters_of(func).size();
                    static_assert(paramN <= 1, "property getter/setter must have 0 or 1 parameter");
                    auto is_getter = paramN == 0;
                    auto prop_v = meta::extract<property>(prop_ann[0]);
                    auto& prop = props[prop_v.name];
                    if (is_getter)
                        props[prop_v.name].getter = member;
                    else {
                        prop.setter = member;
                        if (prop_v.datamem != meta::info{} && prop.getter == meta::info{}) prop.getter = prop_v.datamem;
                    }
                }
            }
        }
        return std::make_pair(std::define_static_array(props | std::views::transform([](const auto& p) {
                                                           return std::make_pair(p.first.data(), p.second);
                                                       })),
                              std::define_static_array(bindmems));
    }();
    template for (constexpr auto& prop : props) {
        constexpr auto name = prop.first;
        constexpr auto getter = prop.second.getter;
        constexpr auto setter = prop.second.setter;

        if constexpr (setter == meta::info{}) {
            static_assert(meta::is_function(getter), "bad property declaration");

            auto getter_args = detail::parse_func_annotations<getter>();
            std::apply([&](const auto&... args) { nb_cls.def_prop_ro(name, &[:getter:], args...); }, getter_args);
        } else {
            static_assert(meta::is_function(setter), "bad property declaration");
            auto setter_args = std::apply([](const auto&... args) { return std::tuple(nb::for_setter(args)...); },
                                          detail::parse_func_annotations<setter>());

            if constexpr (meta::is_function(getter)) {
                auto getter_args = std::apply([](const auto&... args) { return std::tuple(nb::for_getter(args)...); },
                                              detail::parse_func_annotations<getter>());
                std::apply([&](const auto&... args) { nb_cls.def_prop_rw(name, &[:getter:], &[:setter:], args...); },
                           std::tuple_cat(getter_args, setter_args));

            } else {
                static_assert(meta::is_nonstatic_data_member(getter), "bad property declaration");
                using T = typename[:cls:];
                auto getter_ = [](T& self) { return self.[:getter:]; };
                std::apply([&](const auto&... args) { nb_cls.def_prop_rw(name, getter_, &[:setter:], args...); },
                           setter_args);
            }
        }
    }
    template for (constexpr auto& mbr_b : members) { bind_class_member<mbr_b, cls_b, true>(nb_cls, *nb_base); }
    return nb_cls;
}

template <meta::info ns_b>
    requires(meta::is_namespace(ns_b) || is_bind_declaration(ns_b))
auto bind_submodule(nb::module_& nb_mod, std::optional<nb::module_&> nb_base) {
    if (!nb_base.has_value()) nb_base = nb_mod;
    std::string identifier = nb::cast<std::string>(nb_mod.attr("__name__")) + '.' + detail::name_of<ns_b>();

    if (auto nb_mod_old = ABRegistry::find_module(identifier)) {
        // 这里按ns_b进行区分, 而class则是按底层类型
        std::println("[annobind warn] submodule {}(from {}) already bound", identifier, meta::display_string_of(ns_b));
        return *nb_mod_old;
    }
    auto mod = detail::make_nb_module<ns_b>(nb_mod, identifier);
    bind_to_module<ns_b>(mod, *nb_base);
    return mod;
}

template <meta::info ns_b>
    requires(meta::is_namespace(ns_b) || is_bind_declaration(ns_b))
auto& bind_to_module(nb::module_& nb_mod, std::optional<nb::module_&> nb_base) {
    if (!nb_base.has_value()) nb_base = nb_mod;
    constexpr auto ns = detail::take_bind<ns_b>();
    constexpr auto access_ctx = meta::access_context::current();
    template for (constexpr const auto& member : std::define_static_array(meta::members_of(ns, access_ctx))) {
        bind_module_member<member, ns_b, true>(nb_mod, *nb_base);
    }
    return nb_mod;
}

}  // namespace annobind