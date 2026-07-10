#include <nanobind/nanobind.h>

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

// 在 `nb_mod` 下创建基于 `ns` 的空子模块
template <meta::info ns, meta::info bind_h_n = meta::info{}>
    requires(meta::is_namespace(ns))
auto make_nb_module(nb::module_& nb_mod, const std::string& identifier) {
    constexpr const char* name = detail::name_of<ns, bind_h_n>();
    constexpr auto doctuple = detail::doctuple_of<ns, conf::auto_module_src, bind_h_n>();
    static_assert(name[0] != '\0', "name must be specified");

    auto nb_mod_new = std::apply([&](const auto&... args) { return nb_mod.def_submodule(name, args...); }, doctuple);
    ABRegistry::register_module(nb_mod_new, identifier);
    return nb_mod_new;
}

// 在`scope`下创建基于`cls`的空类
template <meta::info cls, meta::info bind_h_n = meta::info{}>
    requires(detail::is_non_enum_type<cls>())
auto make_nb_class(nb::handle& nb_scope) {
    // 提取选项
    constexpr const char* cls_name = detail::name_of<cls, bind_h_n>();
    constexpr auto extra_annons = detail::extral_annotations_of<cls, conf::auto_type_src, bind_h_n>();
    constexpr meta::info trampoline = detail::get_annotation_ex<cls, trampo, trampo{meta::info{}}, bind_h_n>().type;
    static_assert(cls_name[0] != '\0', "name must be specified");

    // 构建 nb::class_ 的模板参数列表
    constexpr auto access_ctx = meta::access_context::current();
    using cls_t = typename[:[&] {
        std::vector<meta::info> class_list;
        class_list.push_back(cls);
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

// 注: 若bind_h不为空, 则其底层实体的标注会注入到所绑定实体的标注中去
// 同样的, 若bind_h_n不为空, 则其底层实体的标注会注入到作用域的标注中去
// 若涉及到嵌套作用域，则 `nb_base` 被视为根作用域(视情况可以为nb模块/类)

/// 绑定枚举`enum_`到`nb_scp`(可为模块/类)
template <meta::info enum_, meta::info bind_h = meta::info{}>
    requires(detail::is_enum_type<enum_>())
auto bind_enum(nb::handle& nb_scp);

// 绑定函数 `func` 到 `nb_mod`
template <meta::info func, meta::info bind_h = meta::info{}>
    requires(meta::is_function(func))
auto bind_function(nb::module_& nb_mod);

//  将命名空间成员 `member` 绑定到 `nb_mod` 模块. 可跨命名空间绑定
// `ns_` 提供成员跳过检查标注, 默认为 `member` 所在的命名空间(别名的标注会丢失), 可为任意可标注的实体
// `skip_check` 用于标识自动绑定, 默认值false表示不进行skip相关标注的检查, 而强制进行绑定
template <meta::info member, meta::info ns_ = meta::info{}, bool skip_check = false, meta::info bind_h = meta::info{},
          meta::info bind_h_n = meta::info{}>
    requires(meta::is_namespace_member(member))
auto bind_module_member(nb::module_& nb_mod, std::optional<nb::module_&> nb_base = std::nullopt);

//  绑定`nb_cls`对应类的成员 `member`.
// `cls_` 与 `skip_check` 同 bind_module_member
template <meta::info member, meta::info cls_ = meta::info{}, bool skip_check = false, meta::info bind_h = meta::info{},
          meta::info bind_h_n = meta::info{}>
    requires(meta::is_class_member(member))
auto bind_class_member(auto& nb_cls, std::optional<nb::handle&> nb_base = std::nullopt);

// 将直接绑定声明 `bind_h` 对应的实体绑定到 `nb_scope`. 注意, 只有当 `bind_h` 为命名空间成员时才可跨作用域绑定
template <meta::info bind_h, meta::info bind_h_n = meta::info{}>
    requires(is_bind_declaration(bind_h))
auto bind_bind(auto& nb_scope, auto& nb_base);

// 将类 `cls` 绑定到 `nb_scope`
template <meta::info cls, meta::info bind_h_n = meta::info{}>
    requires(detail::is_non_enum_type<cls>())
auto bind_class(nb::handle& nb_scope, std::optional<nb::handle&> nb_base = std::nullopt);

// 基于 `ns` 创建`nb_mod`的子模块, 其中包含 `ns` 的成员
template <meta::info ns, meta::info bind_h_n = meta::info{}>
    requires(meta::is_namespace(ns))
auto bind_submodule(nb::module_& nb_mod, std::optional<nb::module_&> nb_base = std::nullopt);

// 将命名空间 `ns` 的成员 绑定到 `nb_mod`
template <meta::info ns, meta::info bind_h_n = meta::info{}>
    requires(meta::is_namespace(ns))
auto& bind_to_module(nb::module_& nb_mod, std::optional<nb::module_&> nb_base = std::nullopt);

//===================== 实现 =====================

template <meta::info enum_, meta::info bind_h>
    requires(detail::is_enum_type<enum_>())
auto bind_enum(nb::handle& nb_scp) {
    using T = typename[:enum_:];
    if (auto nb_enum_old = ABRegistry::find_enum<T>()) {
        std::println("[annobind warn] enum {} already bound", meta::display_string_of(enum_));
        return *nb_enum_old;
    }

    constexpr const char* name = detail::name_of<enum_, bind_h>();
    constexpr auto extra_annons = detail::extral_annotations_of<enum_, conf::auto_type_src, bind_h>();
    constexpr bool export_values_v =
        detail::check_anno_bool_value_ex<enum_, export_values, conf::auto_export_enum, bind_h>();
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

template <meta::info func, meta::info bind_h>
    requires(meta::is_function(func))
auto bind_function(nb::module_& nb_mod) {
    constexpr const char* name = detail::name_of<func, bind_h>();
    static_assert(name[0] != '\0', "name must be specified");

    auto args = detail::parse_func_annotations<func, bind_h>();
    std::apply([&](const auto&... args) { nb_mod.def(name, &[:func:], args...); }, args);
}

template <meta::info member, meta::info ns_, bool skip_check, meta::info bind_h, meta::info bind_h_n>
    requires(meta::is_namespace_member(member))
auto bind_module_member(nb::module_& nb_mod, std::optional<nb::module_&> nb_base) {
    if (!nb_base.has_value()) nb_base = nb_mod;
    constexpr auto ns = [] {
        if (ns_ == meta::info{}) return meta::parent_of(member);
        return ns_;
    }();
    constexpr bool is_bind_decl = is_bind_declaration(member);
    if constexpr (detail::is_bindable<member>()) {
        constexpr const char* name = detail::name_of<member, bind_h>();
        if constexpr (name[0] == '\0')
            return;
        else if constexpr (is_bind_decl) {
            // ---------------- 直接绑定声明 ----------------
            if constexpr (!detail::check_anno_bool_value<member, skip, unskip, false>()) {
                return bind_bind<member, bind_h_n>(nb_mod, *nb_base);
            }

        } else if constexpr (meta::is_variable(member)) {
            // ---------------- 变量 ----------------
            constexpr bool df = detail::check_anno_bool_value_ex<ns, skip_data, conf::auto_skip_data, bind_h_n>();
            if constexpr (!skip_check || !detail::check_anno_bool_value<member, skip, unskip, df>()) {
                auto v = [:member:];
                nb_mod.attr(name) = v;
            }

        } else if constexpr (meta::is_function(member)) {
            // ---------------- 函数 ----------------
            constexpr bool df = detail::check_anno_bool_value_ex<ns, skip_func, conf::auto_skip_func, bind_h_n>();
            if constexpr (!skip_check || !detail::check_anno_bool_value<member, skip, unskip, df>()) {
                return bind_function<member, bind_h>(nb_mod);
            }

        } else if constexpr (detail::is_nested(member)) {
            // ---------------- 命名空间/类型 ----------------
            constexpr bool df =
                meta::dealias(member) != member
                    ? skip_check
                    : detail::check_anno_bool_value_ex<ns, skip_nested, conf::auto_skip_nested, bind_h_n>();
            if constexpr (!skip_check || !detail::check_anno_bool_value<member, skip, unskip, df>()) {
                constexpr bool plain_bind =
                    detail::check_anno_bool_value_ex<member, plain, conf::auto_plain_module, bind_h>();

                auto& to_scope = [&]() -> auto& {
                    if constexpr (plain_bind)
                        return *nb_base;
                    else
                        return nb_mod;
                }();
                if constexpr (detail::is_enum_type<member>()) {
                    return bind_enum<member, bind_h>(to_scope);
                } else if constexpr (meta::is_complete_type(member)) {
                    return bind_class<member, bind_h>(to_scope, *nb_base);
                } else if constexpr (meta::is_namespace(member)) {
                    if constexpr (plain_bind)
                        return bind_to_module<member, bind_h>(*nb_base, *nb_base);
                    else
                        return bind_submodule<member, bind_h>(nb_mod, *nb_base);
                }
            }
        }
    }
}

template <meta::info member, meta::info cls_, bool skip_check, meta::info bind_h, meta::info bind_h_n>
    requires(meta::is_class_member(member))
auto bind_class_member(auto& nb_cls, std::optional<nb::handle&> nb_base) {
    if (!nb_base.has_value()) nb_base = nb_cls;
    constexpr auto cls = [&] {
        if (cls_ == meta::info{}) return meta::parent_of(member);
        return cls_;
    }();
    constexpr bool is_bind_decl = is_bind_declaration(member);
    if constexpr (detail::is_bindable<member>()) {
        constexpr const char* name = detail::name_of<member, bind_h>();
        if constexpr (name[0] == '\0' && !meta::is_constructor(member))
            return;

        else if constexpr (is_bind_decl) {
            // ---------------- 直接绑定声明 ----------------
            if constexpr (!detail::check_anno_bool_value<member, skip, unskip, false>()) {
                return bind_bind<member, bind_h_n>(nb_cls, *nb_base);
            }

        } else if constexpr (detail::is_data_member(member)) {
            // ---------------- 数据成员 ----------------
            constexpr bool df = detail::check_anno_bool_value_ex<cls, skip_data, conf::auto_skip_data, bind_h_n>();
            if constexpr (!skip_check || !detail::check_anno_bool_value<member, skip, unskip, df>()) {
                constexpr auto doc_tup = detail::doctuple_of<member, conf::auto_data_member_src, bind_h>();

                if constexpr (detail::is_nonstatic_data_member(member)) {
                    // ---------- 非静态数据成员 ----------
                    if constexpr (meta::is_const(member))
                        std::apply([&](const auto&... srcloc) { nb_cls.def_ro(name, &[:member:], srcloc...); },
                                   doc_tup);
                    else
                        std::apply([&](const auto&... srcloc) { nb_cls.def_rw(name, &[:member:], srcloc...); },
                                   doc_tup);

                } else if constexpr (detail::is_static_data_member(member)) {
                    // ---------- 静态数据成员 ----------
                    if constexpr (meta::is_const(member)) {
                        std::apply([&](const auto&... srcloc) { nb_cls.def_ro_static(name, &[:member:], srcloc...); },
                                   doc_tup);
                    } else
                        std::apply([&](const auto&... srcloc) { nb_cls.def_rw_static(name, &[:member:], srcloc...); },
                                   doc_tup);
                }
            }

        } else if constexpr (detail::is_bindable_member_function(member)) {
            // ---------- 函数成员 ----------
            constexpr bool df = detail::check_anno_bool_value_ex<cls, skip_func, conf::auto_skip_func, bind_h_n>();
            if constexpr (!skip_check || !detail::check_anno_bool_value<member, skip, unskip, df>()) {
                auto extra_args = detail::parse_func_annotations<member, bind_h>();

                if constexpr (meta::is_constructor(member)) {
                    // ---------- 构造函数 ----------
                    using init_type =
                        typename[:meta::substitute(^^nb::init,
                                                   std::define_static_array(meta::parameters_of(member) |
                                                                            std::views::transform([](auto p) {
                                                                                return meta::type_of(p);
                                                                            }))):];
                    std::apply(
                        [&](const auto&... args) { nb_cls.def(init_type(), std::forward<decltype(args)>(args)...); },
                        extra_args);

                } else {
                    // ---------- 具名函数/运算符 ----------
                    if constexpr (meta::is_static_member(member)) {
                        std::apply(
                            [&](const auto&... args) {
                                nb_cls.def_static(name, &[:member:], std::forward<decltype(args)>(args)...);
                            },
                            extra_args);
                    } else {
                        std::apply(
                            [&](const auto&... args) {
                                nb_cls.def(name, &[:member:], std::forward<decltype(args)>(args)...);
                            },
                            extra_args);
                    }
                }
            }

        } else if constexpr (meta::is_enumerable_type(member)) {
            // ---------------- 嵌套类型 ----------------
            constexpr bool df =
                meta::dealias(member) != member
                    ? skip_check
                    : detail::check_anno_bool_value_ex<cls, skip_nested, conf::auto_skip_nested, bind_h_n>();
            if constexpr (!skip_check || !detail::check_anno_bool_value<member, skip, unskip, df>()) {
                constexpr bool plain_bind =
                    detail::check_anno_bool_value_ex<member, plain, conf::auto_plain_module, bind_h>();
                auto& to_scope = [&]() -> auto& {
                    if constexpr (plain_bind)
                        return *nb_base;
                    else
                        return nb_cls;
                }();
                if constexpr (detail::is_enum_type<member>()) {
                    return bind_enum<member, bind_h>(to_scope);
                } else if constexpr (meta::is_complete_type(member)) {
                    return bind_class<member, bind_h>(to_scope, *nb_base);
                }
            }
        }
    }
}

template <meta::info bind_h, meta::info bind_h_n>
    requires(is_bind_declaration(bind_h))
auto bind_bind(auto& nb_scope, auto& nb_base) {
    constexpr meta::info obj = [:bind_h:].obj;
    if constexpr (obj != meta::info{}) {
        constexpr auto scp = meta::parent_of(obj);
        if constexpr (meta::is_namespace(scp))
            return bind_module_member<obj, scp, false, bind_h, bind_h_n>(nb_scope, nb_base);
        else if constexpr (meta::is_enumerable_type(scp))
            return bind_class_member<obj, scp, false, bind_h, bind_h_n>(nb_scope, nb_base);
    }
}

template <meta::info cls, meta::info bind_h_n>
    requires(detail::is_non_enum_type<cls>())
auto bind_class(nb::handle& nb_scope, std::optional<nb::handle&> nb_base) {
    if (!nb_base.has_value()) nb_base = nb_scope;
    if (auto nb_cls_old = ABRegistry::find_class<typename[:cls:]>()) {
        std::println("[annobind warn] class {} already bound", meta::display_string_of(cls));
        return *nb_cls_old;
    }

    auto nb_cls = detail::make_nb_class<cls, bind_h_n>(nb_scope);
    constexpr auto access_ctx = meta::access_context::current();
    template for (constexpr const auto& member : std::define_static_array(meta::members_of(cls, access_ctx))) {
        bind_class_member<member, cls, true, meta::info{}, bind_h_n>(nb_cls, *nb_base);
    }
    return nb_cls;
}

template <meta::info ns, meta::info bind_h_n>
    requires(meta::is_namespace(ns))
auto bind_submodule(nb::module_& nb_mod, std::optional<nb::module_&> nb_base) {
    if (!nb_base.has_value()) nb_base = nb_mod;
    std::string identifier = nb::cast<std::string>(nb_mod.attr("__name__")) + '.' + detail::name_of<ns, bind_h_n>();

    if (auto nb_mod_old = ABRegistry::find_module(identifier)) {
        std::println("[annobind warn] submodule {}(from {}) already bound", identifier, meta::display_string_of(ns));
        return *nb_mod_old;
    }
    auto mod = detail::make_nb_module<ns, bind_h_n>(nb_mod, identifier);
    bind_to_module<ns, bind_h_n>(mod, *nb_base);
    return mod;
}

template <meta::info ns, meta::info bind_h_n>
    requires(meta::is_namespace(ns))
auto& bind_to_module(nb::module_& nb_mod, std::optional<nb::module_&> nb_base) {
    if (!nb_base.has_value()) nb_base = nb_mod;
    constexpr auto access_ctx = meta::access_context::current();
    template for (constexpr const auto& member : std::define_static_array(meta::members_of(ns, access_ctx))) {
        bind_module_member<member, ns, true, meta::info{}, bind_h_n>(nb_mod, *nb_base);
    }
    return nb_mod;
}

}  // namespace annobind