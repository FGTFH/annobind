#pragma once
#include <nanobind/nanobind.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <meta>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>

#include "ab_attr.hpp"
#include "ab_config.hpp"

namespace annobind {

namespace nb = nanobind;
namespace meta = std::meta;
namespace detail {
// ==================== 常量映射表 ====================
static constexpr auto op2magic = [] {
    auto arr = std::array<std::pair<meta::operators, const char*>, 30>{{

        // 一元运算符 & 特殊
        {meta::operators::op_parentheses, "__call__"},         // ()
        {meta::operators::op_square_brackets, "__getitem__"},  // []
        {meta::operators::op_tilde, "__invert__"},             // ~
        {meta::operators::op_exclamation, "__not__"},          // ! -> not

        // 算术运算符
        {meta::operators::op_plus, "__add__"},                // +
        {meta::operators::op_minus, "__sub__"},               // -
        {meta::operators::op_star, "__mul__"},                // *
        {meta::operators::op_slash, "__truediv__"},           // /
        {meta::operators::op_percent, "__mod__"},             // %
        {meta::operators::op_caret, "__xor__"},               // ^
        {meta::operators::op_ampersand, "__and__"},           // & -> and
        {meta::operators::op_pipe, "__or__"},                 // | -> or
        {meta::operators::op_less_less, "__lshift__"},        // <<
        {meta::operators::op_greater_greater, "__rshift__"},  // >>

        // 复合赋值运算符
        {meta::operators::op_plus_equals, "__iadd__"},                // +=
        {meta::operators::op_minus_equals, "__isub__"},               // -=
        {meta::operators::op_star_equals, "__imul__"},                // *=
        {meta::operators::op_slash_equals, "__itruediv__"},           // /=
        {meta::operators::op_percent_equals, "__imod__"},             // %=
        {meta::operators::op_caret_equals, "__ixor__"},               // ^=
        {meta::operators::op_ampersand_equals, "__iand__"},           // &=
        {meta::operators::op_pipe_equals, "__ior__"},                 // |=
        {meta::operators::op_less_less_equals, "__ilshift__"},        // <<=
        {meta::operators::op_greater_greater_equals, "__irshift__"},  // >>=

        // 比较运算符
        {meta::operators::op_equals_equals, "__eq__"},       // ==
        {meta::operators::op_exclamation_equals, "__ne__"},  // !=
        {meta::operators::op_less, "__lt__"},                // <
        {meta::operators::op_greater, "__gt__"},             // >
        {meta::operators::op_less_equals, "__le__"},         // <=
        {meta::operators::op_greater_equals, "__ge__"},      // >=

        // 注：op_plus_plus, op_minus_minus, op_arrow, op_arrow_star, op_comma,
        // op_ampersand_ampersand, op_pipe_pipe, op_spaceship, op_new, op_delete 等
        // 在 Python 中无直接对应，故省略。
    }};
    std::sort(arr.begin(), arr.end(), [](auto& a, auto& b) { return a.first < b.first; });
    return arr;
}();

// ==================== 辅助工具 ====================
inline constexpr std::string u32_to_str(unsigned int value) {
    if (value == 0) return "0";
    char buffer[11];  // 最大10位数字+1
    int idx = 10;
    buffer[idx--] = '\0';
    while (value > 0) {
        buffer[idx--] = '0' + (value % 10);
        value /= 10;
    }
    return std::string(&buffer[idx + 1]);
}

// ==================== 名称解析 ====================
// 优先级顺序：
// 1. bind_h 的 rename 注解
// 2. obj 自身的 rename 注解
// 2. dealias(obj) 自身的 rename 注解
// 3. bind_h 的 identifier（如果绑定点有名）
// 4. obj 自身的 identifier
// 5. 如果是运算符，映射到 Python 魔术方法名
template <meta::info obj, meta::info bind_h = meta::info{}>
consteval const char* name_of() {
    if constexpr (bind_h != meta::info{}) {
        auto ann = meta::annotations_of_with_type(bind_h, ^^rename);
        if (ann.size() > 0) return meta::extract<rename>(ann[0]).name;
    }

    auto ann = meta::annotations_of_with_type(obj, ^^rename);
    if (ann.size() > 0) return meta::extract<rename>(ann[0]).name;

    if (obj != meta::dealias(obj)) {
        auto ann = meta::annotations_of_with_type(meta::dealias(obj), ^^rename);
        if (ann.size() > 0) return meta::extract<rename>(ann[0]).name;
    }
    if (meta::has_identifier(bind_h)) return meta::identifier_of(bind_h).data();
    if (meta::has_identifier(obj)) return meta::identifier_of(obj).data();
    if (meta::is_operator_function(obj) && meta::is_class_member(obj)) {
        auto op = meta::operator_of(obj);
        auto it = std::lower_bound(op2magic.begin(), op2magic.end(), op, [](auto& a, auto& b) { return a.first < b; });
        if (it != op2magic.end()) return it->second;
    }
    return "";
}

// ==================== 文档字符串 ====================
template <meta::info obj, bool with_src_h, meta::info bind_h = meta::info{}>
consteval const char* docstring_of() {
    constexpr bool with_src_v = check_anno_bool_value_ex<obj, with_src, with_src_h, bind_h>();
    std::string doc_;

    // 收集 doc 注解（先后顺序：dealias(obj) > obj > bind_h）
    auto append_doc = [&](meta::info target) {
        auto ann = meta::annotations_of_with_type(target, ^^doc);
        if (!ann.empty()) {
            auto doc_ann = meta::extract<doc>(ann[0]);
            if (doc_ann.text[0] != '\0') {
                if (!doc_.empty()) doc_ += '\n';
                doc_ += doc_ann.text;
            }
        }
    };

    if constexpr (meta::dealias(obj) != obj) append_doc(meta::dealias(obj));
    append_doc(obj);
    if constexpr (bind_h != meta::info{}) append_doc(bind_h);

    // 附加源码位置（如果需要）
    if constexpr (with_src_v) {
        auto src = meta::source_location_of(obj);
        auto srcloc_str = std::string(src.file_name()) + ":" + u32_to_str(src.line()) + ":" + u32_to_str(src.column());
        if (!doc_.empty()) doc_ += '\n';
        doc_ += srcloc_str;
    }

    return std::define_static_string(doc_);
}

template <meta::info obj, bool with_src_h, meta::info bind_h = meta::info{}>
consteval auto doctuple_of() {
    constexpr const char* doc = docstring_of<obj, with_src_h, bind_h>();
    if constexpr (doc[0] != '\0') {
        return std::tuple(doc);
    } else {
        return std::tuple{};
    }
}

// ==================== nanobind 原生注解提取 ====================
template <meta::info obj, meta::info bind_h = meta::info{}>
consteval auto nb_annotations_of() {
    constexpr auto all_anns = [] {
        std::vector<std::vector<meta::info>> storage{};
        if (obj != meta::dealias(obj)) storage.push_back(meta::annotations_of(meta::dealias(obj)));
        storage.push_back(meta::annotations_of(obj));
        if (bind_h != meta::info{}) storage.push_back(meta::annotations_of(bind_h));
        return std::define_static_array(std::views::join(storage));
    }();

    constexpr auto arr =
        std::define_static_array(all_anns | std::views::filter([](meta::info ann) { return !is_ab_annotation(ann); }));
    return [&]<size_t... I>(std::index_sequence<I...>) {
        return std::tuple([:meta::constant_of(arr[I]):]...);
    }(std::make_index_sequence<arr.size()>{});
}

// 总是同时使用
template <meta::info obj, bool with_src_h, meta::info bind_h = meta::info{}>
consteval auto extral_annotations_of() {
    return std::tuple_cat(nb_annotations_of<obj, bind_h>(), doctuple_of<obj, with_src_h, bind_h>());
}

// ==================== 函数注解解析 ====================
template <meta::info func, meta::info bind_h = meta::info{}>
auto parse_func_annotations() {
    constexpr auto params = std::define_static_array(meta::parameters_of(func));
    constexpr int paramN = params.size();
    constexpr bool auto_param_name_ = check_anno_bool_value_ex<func, auto_param_name, conf::auto_param_name, bind_h>();
    constexpr bool with_src_v = check_anno_bool_value_ex<func, with_src, conf::auto_func_src, bind_h>();

    constexpr auto all_args = [] {
        std::vector<meta::info> storage{};
        if constexpr (func != meta::dealias(func))
            storage.append_range(meta::annotations_of_with_type(meta::dealias(func), ^^arg));
        storage.append_range(meta::annotations_of_with_type(func, ^^arg));
        if constexpr (bind_h != meta::info{}) storage.append_range(meta::annotations_of_with_type(bind_h, ^^arg));
        return std::define_static_array(storage);
    }();

    // 主解析逻辑
    static constexpr auto parsed_args = [&] {
        std::vector<arg> result;

        for (int i{}; i < paramN; i++) {
            const auto& p = params[i];
            arg param_arg{i};
            // 检查参数自身的 arg 注解
            auto param_args = meta::annotations_of_with_type(p, ^^arg);
            if (param_args.size() > 0) {
                param_arg = meta::extract<arg>(param_args[0]);
                param_arg.ind = i;
            }

            // 自动生成参数名（如果需要）
            if constexpr (auto_param_name_) {
                auto s = meta::has_identifier(p) ? meta::identifier_of(p)
                         : paramN == 1           ? std::string("arg")
                                                 : std::string("arg") + u32_to_str(i);
                param_arg.name_ = std::define_static_string(s);
            }
            auto anns_p =
                meta::annotations_of(p) | std::views::transform([](const auto& ann) { return meta::constant_of(ann); });
            auto simp_default_ =
                std::ranges ::find_if(anns_p, [](auto ann) { return meta::remove_cvref(meta::type_of(ann)) != ^^arg; });
            if (simp_default_ != anns_p.end()) param_arg.default_ = *simp_default_;
            result.push_back(param_arg);
        }

        // 按顺序检查函数注解
        for (const auto& ann : all_args) {
            auto arg_obj = meta::extract<arg>(ann);
            auto ind = arg_obj.ind;
            if (!(ind >= 0 && ind < paramN)) {
                auto it = std::ranges::find_if(params, [&](const auto& p) {
                    return meta::has_identifier(p) && meta::identifier_of(p) == arg_obj.cpp_name;
                });
                if (it != params.end()) ind = std::distance(params.begin(), it);
            }
            if (ind >= 0 && ind < paramN) {
                if (arg_obj.default_ != meta::info{}) result[ind].default_ = arg_obj.default_;
                if (arg_obj.name_) result[ind].name_ = arg_obj.name_;
                if (arg_obj.signature_) result[ind].signature_ = arg_obj.signature_;
                if (arg_obj.nonconvert_) result[ind].nonconvert_ = arg_obj.nonconvert_;
                if (arg_obj.none_) result[ind].none_ = arg_obj.none_;
                if (arg_obj.lock_) result[ind].lock_ = arg_obj.lock_;
                if (arg_obj.kw_only_) result[ind].kw_only_ = arg_obj.kw_only_;
            }
        }
        return std::define_static_array(result);
    }();

    //  构造 nb::arg 元组
    auto arg_tuple = [&]<size_t... I>(std::index_sequence<I...>) {
        return std::tuple([&]() {
            constexpr auto& arg_obj = parsed_args[I];
            nb::arg base = nb::arg(arg_obj.name_);
            if constexpr (arg_obj.nonconvert_) base = base.noconvert();
            if constexpr (arg_obj.none_) base = base.none();

            // 检查默认值与lock标志, 注意这里会返回4种类型
            if constexpr (arg_obj.default_ != meta::info{}) {
                auto name = arg_obj.name_;
                constexpr auto val = [:meta::constant_of(arg_obj.default_):];
                if constexpr (arg_obj.lock_)
                    return (base.lock() = val);
                else
                    return (base = val);
            } else {
                if constexpr (arg_obj.lock_)
                    return base.lock();
                else
                    return base;
            }
        }()...);
    }(std::make_index_sequence<paramN>{});

    // 查找 kw_only 位置
    static constexpr int kw_ind = std::distance(
        parsed_args.begin(), std::ranges::find_if(parsed_args, [](const auto& arg) { return arg.kw_only_; }));

    //  插入 kw_only
    auto final_args = [&]() {
        if constexpr (kw_ind < paramN) {
            auto prefix = [&]<size_t... I>(std::index_sequence<I...>) {
                return std::tuple(std::get<I>(arg_tuple)...);
            }(std::make_index_sequence<kw_ind>{});
            auto suffix = [&]<size_t... I>(std::index_sequence<I...>) {
                return std::tuple(std::get<kw_ind + I>(arg_tuple)...);
            }(std::make_index_sequence<paramN - kw_ind>{});
            return std::tuple_cat(prefix, std::tuple(nb::kw_only{}), suffix);
        } else {
            return arg_tuple;
        }
    }();

    constexpr auto doc_tuple = extral_annotations_of<func, with_src_v>();
    return std::tuple_cat(final_args, doc_tuple);
}
}  // namespace detail
// using namespace nb::literals;
}  // namespace annobind
