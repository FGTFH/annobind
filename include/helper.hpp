#pragma once
#include <meta>
#include <string>
namespace meta = std::meta;

// 辅助宏：如果条件满足，则追加描述文本
#define APPEND_DESC(cond, text)         \
    if (cond) {                         \
        if (!desc.empty()) desc += ' '; \
        desc += text;                   \
    }

consteval const char* describe_entity(meta::info entity) {
    std::string desc;

    // ---------- 实体种类 ----------
    APPEND_DESC(meta::is_variable(entity), "variable");
    APPEND_DESC(meta::is_function(entity), "function");
    APPEND_DESC(meta::is_type(entity), "type");
    APPEND_DESC(meta::is_namespace(entity), "namespace");
    APPEND_DESC(meta::is_constructor(entity), "constructor");
    APPEND_DESC(meta::is_destructor(entity), "destructor");
    APPEND_DESC(meta::is_assignment(entity), "assignment");
    APPEND_DESC(meta::is_conversion_function(entity), "conversion");
    APPEND_DESC(meta::is_operator_function(entity), "operator");
    APPEND_DESC(meta::is_literal_operator(entity), "literal_operator");
    APPEND_DESC(meta::is_enumerator(entity), "enumerator");
    APPEND_DESC(meta::is_annotation(entity), "annotation");

    // ---------- 访问性 ----------
    APPEND_DESC(meta::is_public(entity), "public");
    APPEND_DESC(meta::is_protected(entity), "protected");
    APPEND_DESC(meta::is_private(entity), "private");

    // ---------- 函数特殊属性 ----------
    APPEND_DESC(meta::is_virtual(entity), "virtual");
    APPEND_DESC(meta::is_pure_virtual(entity), "pure_virtual");
    APPEND_DESC(meta::is_override(entity), "override");
    APPEND_DESC(meta::is_final(entity), "final");
    APPEND_DESC(meta::is_deleted(entity), "deleted");
    APPEND_DESC(meta::is_defaulted(entity), "defaulted");
    APPEND_DESC(meta::is_user_provided(entity), "user_provided");
    APPEND_DESC(meta::is_user_declared(entity), "user_declared");
    APPEND_DESC(meta::is_explicit(entity), "explicit");
    APPEND_DESC(meta::is_noexcept(entity), "noexcept");
    APPEND_DESC(meta::is_const(entity), "const");
    APPEND_DESC(meta::is_volatile(entity), "volatile");
    APPEND_DESC(meta::is_lvalue_reference_qualified(entity), "lvalue_ref_qualified");
    APPEND_DESC(meta::is_rvalue_reference_qualified(entity), "rvalue_ref_qualified");

    // ---------- 数据成员属性 ----------
    APPEND_DESC(meta::is_bit_field(entity), "bit_field");
    APPEND_DESC(meta::is_mutable_member(entity), "mutable");
    APPEND_DESC(meta::has_default_member_initializer(entity), "has_default_init");

    // ---------- 存储期 ----------
    APPEND_DESC(meta::has_static_storage_duration(entity), "static_storage");
    APPEND_DESC(meta::has_thread_storage_duration(entity), "thread_storage");
    APPEND_DESC(meta::has_automatic_storage_duration(entity), "automatic_storage");

    // ---------- 链接 ----------
    APPEND_DESC(meta::has_internal_linkage(entity), "internal_linkage");
    APPEND_DESC(meta::has_module_linkage(entity), "module_linkage");
    APPEND_DESC(meta::has_external_linkage(entity), "external_linkage");
    APPEND_DESC(meta::has_c_language_linkage(entity), "C_linkage");

    // ---------- 其他属性 ----------
    APPEND_DESC(meta::is_complete_type(entity), "complete_type");
    APPEND_DESC(meta::is_enumerable_type(entity), "enumerable_type");
    APPEND_DESC(meta::is_type_alias(entity), "type_alias");
    APPEND_DESC(meta::is_namespace_alias(entity), "namespace_alias");
    APPEND_DESC(meta::is_special_member_function(entity), "special_member");
    APPEND_DESC(meta::is_function_parameter(entity), "function_parameter");
    APPEND_DESC(meta::is_explicit_object_parameter(entity), "explicit_object_param");
    APPEND_DESC(meta::has_default_argument(entity), "has_default_arg");
    APPEND_DESC(meta::is_vararg_function(entity), "vararg");
    APPEND_DESC(meta::is_value(entity), "value");
    APPEND_DESC(meta::is_object(entity), "object");
    APPEND_DESC(meta::is_structured_binding(entity), "structured_binding");
    APPEND_DESC(meta::is_class_member(entity), "class_member");
    APPEND_DESC(meta::is_namespace_member(entity), "namespace_member");
    APPEND_DESC(meta::is_nonstatic_data_member(entity), "nonstatic_data");
    APPEND_DESC(meta::is_static_member(entity), "static_member");
    APPEND_DESC(meta::is_base(entity), "base");

    // 如果没有匹配任何描述，标记为 unknown
    if (desc.empty()) {
        desc = "unknown";
    }

    return std::define_static_string(desc);
}

#undef APPEND_DESC

struct contin {};

template <meta::info ncls, int indent = 0>
void pptree();

template <meta::info e, int indent = 0>
void pptree_imp() {
    constexpr auto t = [&] {
        // if constexpr (meta::is_variable(e) && meta::remove_cvref(meta::type_of(e)) == ^^ab::bind)
        //     return [:e:].t;
        // else
        return e;
    }();

    constexpr auto name = [] {
        if (e == t)
            return meta::display_string_of(t).data();
        else
            return std::define_static_string(std::string(meta::display_string_of(e)) + " = " +
                                             meta::display_string_of(t));
    }();
    constexpr auto desc = describe_entity(t);

    constexpr const char* s = [] {
        auto s = std::string("");
        for (int i = 0; i < indent; i++) s += "    ";
        s += "{}";
        s = s + "  {}{}\n    " + s;
        return std::define_static_string(s);
    }();

    constexpr auto cont_anN = [&] {
        bool cont = false;
        int anN = 0;
        if constexpr (meta::is_type(t) || meta::is_type_alias(t) || meta::is_variable(t) || meta::is_function(t) ||
                      meta::is_namespace(t) || meta::is_nonstatic_data_member(t)) {
            static constexpr auto anns = std::define_static_array(meta::annotations_of(t));
            static constexpr auto annsor = std::define_static_array(meta::annotations_of(meta::dealias(t)));
            template for (constexpr auto& ann : anns) {
                constexpr auto v = meta::constant_of(ann);
                if constexpr (meta::remove_cvref(meta::dealias(meta::type_of(v))) == ^^contin) cont = true;
            }
            anN = anns.size();
            if constexpr (t != meta::dealias(t)) anN += annsor.size() * 10000;
        }
        return std::tuple(cont, anN);
    }();
    constexpr auto cont = std::get<0>(cont_anN);
    constexpr auto anN = std::get<1>(cont_anN);
    constexpr const char* params = [] {
        std::string bf{};
        if constexpr (meta::is_function(t)) {
            bf += "(";
            bool first = true;
            for (const auto& p : meta::parameters_of(t)) {
                if (!first) bf += ", ";
                if (first) first = false;
                bf += meta::display_string_of(meta::type_of(p));
                if (meta::has_identifier(p)) {
                    bf += " ";
                    bf += meta::identifier_of(p);
                }
            }
            bf += ")";
        }
        return std::define_static_string(bf);
    }();
    std::println(s, anN, name, params, desc);
    if constexpr (indent > 4) return;
    if constexpr (cont) {
        pptree<t, indent + 1>();
    }
}
template <meta::info ncls, int indent>
void pptree() {
    template for (constexpr auto& e :
                  std::define_static_array(meta ::members_of(ncls, meta::access_context::unchecked()))) {
        if constexpr (meta::is_special_member_function(e)) continue;
        pptree_imp<e, indent>();
    }
}
