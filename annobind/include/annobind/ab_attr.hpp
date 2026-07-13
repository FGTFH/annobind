#pragma once
#include <nanobind/nanobind.h>

#include <meta>
#include <string_view>
#include <type_traits>

#include "ab_config.hpp"
#include "ab_traits.hpp"

namespace annobind {
namespace meta = std::meta;
namespace nb = nanobind;
namespace literals {
inline consteval const char* operator""_cs(const char* s, size_t) {
    return std::define_static_string(std::string_view(s));
}
}  // namespace literals

// 大部分标注都适用优先级覆盖原则: ab::bind直接绑定声明的标注 > 实体本身的标注 > dealias(实体)的标注

// =============== 通用选项 ===============
// 影响导出与否的标注(skip, unskip, nested), 只影响实体本身的导出(若实体是别名, 无视其底层实体的这些标注)
struct skip {};    // 跳过实体的导出 (优先于unskip)
struct unskip {};  // 当实体被标记skip时(如所在作用域skip当前实体), 带有此注解的实体仍会被导出

struct with_src {};  // 在实体doc中附加源码位置

// 指定实体的docstring(ab::bind直接绑定声明, 实体本身, dealias(实体), with_src等产生的docstring会被拼接)
struct doc {
    const char* text;
    consteval doc(std::string_view text_) { text = std::define_static_string(text_); }
};
// 指定实体的名称, 若不指定, 则默认使用实体的cpp名称
struct rename {
    const char* name;
    consteval rename(std::string_view name_) { name = std::define_static_string(name_); }
};
// ------------ nanobind ------------
// using name =nb::name;//deprecated, use ab::rename
// using scope =nb::scope;//deprecated, use ab::plain or ab::bind_xxx

// =============== 函数选项 ===============
struct auto_param_name {};  // 根据函数声明的形参列表, 自动生成参数名

// nb::arg的静态版本, 仅在标注中使用, 用于在运行时生成对应的nb::arg
// 要使用默认参数, 需指定可静态反射的 value: 对于对应的函数形参类型TP, 可由 TP(value) 构造
// 在 `函数参数标注` 中使用, 则不必指定是哪个参数, 只需默认构造作接口即可, 仅第一个ab::arg标注生效
// 在 `函数标注` 中使用, 须通过参数名称或序号指定参数
// 目前的实现, 对同一个参数的ab::arg标注的非bool项, 会按照优先级进行覆盖(函数参数标注的优先级最低); bool项将取或值
struct arg {
    int ind = 1;
    const char* cpp_name = nullptr;

    consteval arg() {}
    consteval arg(int ind) : ind(ind) {}
    consteval arg(std::string_view cpp_name) { this->cpp_name = std::define_static_string(cpp_name); }

    consteval arg rename(std::string_view name) {
        name_ = std::define_static_string(name);
        return *this;
    }
    // 与 nb::arg 相同的接口
    consteval arg noconvert(bool value = true) {
        nonconvert_ = value;
        return *this;
    }
    consteval arg none(bool value = true) {
        none_ = value;
        return *this;
    }
    consteval arg sig(std::string_view value) {
        signature_ = std::define_static_string(value);
        return *this;
    }
    consteval arg lock(bool value = true) {
        lock_ = value;
        return *this;
    }
    consteval arg kw_only(bool value = true) {
        kw_only_ = value;
        return *this;
    }
    consteval arg defaul_(std::string_view value) {
        default_ = std::meta::reflect_constant_string(value);
        return *this;
    }
    template <typename T>
    consteval arg defaul_(T&& value) {
        default_ = std::meta::reflect_constant(value);
        return *this;
    }
    template <typename T>
    consteval arg operator=(T&& value)
        requires(!std::is_lvalue_reference_v<T> && !std::is_same_v<std::remove_cvref_t<T>, arg>)
    {
        return defaul_(std::forward<T>(value));
    }

    std::meta::info default_{};
    const char* name_ = nullptr;
    const char* signature_ = nullptr;
    bool nonconvert_ = false;
    bool none_ = false;
    bool lock_ = false;
    bool kw_only_ = false;
};

// 常用选项的简便接口
consteval arg kw_only() { return arg().kw_only(); }
template <typename T>
consteval arg defaul_(T&& value) {
    return arg().defaul_(std::forward<T>(value));
}

// ------------ nanobind ------------
// using arg=nb::arg;// deprecated, use ab::arg
using is_method = nb::is_method;
using is_operator = nb::is_operator;
using is_implicit = nb::is_implicit;
using lock_self = nb::lock_self;
template <typename... Ts>
using call_guard = nb::call_guard<Ts...>;
template <size_t Nurse, size_t Patient>
using keep_alive = nb::keep_alive<Nurse, Patient>;
// using sig = nb::sig;// non constexpr
using rv_policy = nb::rv_policy;
// using kw_only=nb::kw_only;// deprecated, use ab::kw_only
// template <typename T>
// using for_getter = nb::for_getter<T>;  // no sense
// template <typename T>
// using for_setter = nb::for_setter<T>;  // no sense
template <typename Policy>
using call_policy = nb::call_policy<Policy>;

using gil_scoped_release = nb::gil_scoped_release;  // use with call_guard
// using gil_scoped_acquire = nb::gil_scoped_acquire;// no sense

// using is_getter = nb::is_getter;// non documented
// using ft_mutex = nb::ft_mutex;// no sense

// =============== namespace/class 选项 ===============
// 相当于给对应成员添加with_src标注
struct with_func_src {};    // 函数, 包括类(静态)成员函数
struct with_data_src {};    // 变量, 包括类(静态)成员变量
struct with_nested_src {};  // 作用域/类

// 相当于给对应成员添加skip标注, 优先级低于成员的unskip
struct skip_func {};
struct skip_data {};
struct skip_nested {};  // 作用域/类定义(为避免意外, using和typedef类型别名总是会被忽略,其中typedef可unskip)

// =============== class选项 ===============
// 传递 跳板类.
struct trampo {
    meta::info type;
};

// 将函数标记为对应名称的 property, 或者与成员变量名称相同的 property
// 根据标记的函数是否有参数, 确认其是 getter 还是 setter
// 若同时提供了 getter 和 setter, 则定义 read-write property
// 若只提供 getter, 则定义 read-only property
// 若提供了 setter, 但未提供 getter, 且是通过成员变量指定的, getter将自动生成, 并定义 read-write property
// 注意: 公开的(静态)数据成员将被自动绑定为相应的 property
struct property {
    const char* name = nullptr;
    meta::info datamem = meta::info{};
    consteval property(std::string_view name_) { name = std::define_static_string(name_); }
    consteval property(meta::info datamem) : datamem(datamem) {
        name = std::define_static_string(meta::identifier_of(datamem));
    }
};

// ------------ nanobind ------------
using is_final = nb::is_final;
using dynamic_attr = nb::dynamic_attr;
using is_weak_referenceable = nb::is_weak_referenceable;
using is_generic = nb::is_generic;
template <typename T>
using supplement = nb::supplement<T>;
// using type_slots = nb::type_slots;// non constexpr
// using type_slots_callback = nb::type_slots_callback;// non constexpr
// template <typename T>
// using intrusive_ptr = nb::intrusive_ptr<T>;// non constexpr
using never_destruct = nb::never_destruct;
// using pooled = nb::pooled;// nanobind version 2.13

// =============== 枚举 ===============
struct export_values {};  // 用于替代 nb::enum_.export_values();

// ------------ nanobind ------------
using is_flag = nb::is_flag;
using is_arithmetic = nb::is_arithmetic;
// using is_str = nb::is_str;//documented but not implemented

// =============== 枚举项 ===============
// ------------ nanobind ------------
// struct str_value {//documented but not implemented
//     const char* value;
//     consteval str_value(const char* value_) { value = std::define_static_string(value_); }
// };

// =============== 嵌套作用域声明 ===============
// 对类型: 将其绑定到根模块
// 对嵌套作用域：将其中的成员绑定到根模块
struct plain {};

// =============== 主动绑定声明 ===============
// 在作用域中, 主动绑定一个可绑定实体到当前作用域(可跨作用域, 但需有效), 可用于(成员)模板函数/类等的绑定.
// 无论在命名空间还是类中, 都以
// static constexpr fi = ab::bind{^^f<int>}
// 的形式声明,其标注与template<>f的标注都会生效, 且覆盖优先级最高
struct bind {
    meta::info obj;
};

consteval bool is_ab_annotation(meta::info anno) {
    return meta::parent_of(meta::remove_cvref(meta::type_of(anno))) == ^^annobind;
}
consteval bool is_bind_declaration(meta::info member) {
    return meta::is_variable(member) && meta::remove_cvref(meta::type_of(member)) == ^^bind;
}
namespace detail {

template <meta::info obj>
consteval auto take_bind() {
    if constexpr (is_bind_declaration(obj)) return [:obj:].obj;
    return obj;
}
// 通用优先级检查辅助函数
template <meta::info obj_b, typename Checker, typename T>
consteval T check_sources_ex(Checker&& checker, T default_val) {
    if constexpr (obj_b == meta::info{}) return default_val;
    constexpr auto obj = take_bind<obj_b>();

    if (auto res = checker(obj_b); res.has_value()) return *res;
    if constexpr (obj != obj_b)
        if (auto res = checker(obj); res.has_value()) return *res;
    if (obj != meta::dealias(obj))
        if (auto res = checker(meta::dealias(obj)); res.has_value()) return *res;
    return default_val;
}

template <meta::info obj_b, typename pro, typename con, bool default_>
consteval bool check_anno_bool_value_ex() {
    auto checker = [](meta::info src) -> std::optional<bool> {
        if (!meta::annotations_of_with_type(src, ^^pro).empty()) return true;
        if (!meta::annotations_of_with_type(src, ^^con).empty()) return false;
        return std::nullopt;
    };
    return check_sources_ex<obj_b>(checker, default_);
}
template <meta::info obj_b, typename pro, bool default_>
consteval bool check_anno_bool_value_ex() {
    auto checker = [](meta::info src) -> std::optional<bool> {
        if (!meta::annotations_of_with_type(src, ^^pro).empty()) return true;
        return std::nullopt;
    };
    return check_sources_ex<obj_b>(checker, default_);
}

template <meta::info obj_b, typename type, auto default_>
consteval type get_annotation_ex() {
    auto checker = [](meta::info src) -> std::optional<type> {
        auto ann = meta::annotations_of_with_type(src, ^^type);
        if (!ann.empty()) return meta::extract<type>(ann[0]);
        return std::nullopt;
    };
    return check_sources_ex<obj_b>(checker, default_);
}

template <meta::info obj, typename pro, typename con, bool default_>
consteval bool check_anno_bool_value() {
    if (!meta::annotations_of_with_type(obj, ^^pro).empty()) return true;
    if (!meta::annotations_of_with_type(obj, ^^con).empty()) return false;
    return default_;
}

template <meta::info obj, typename pro, bool default_>
consteval bool check_anno_bool_value() {
    if (!meta::annotations_of_with_type(obj, ^^pro).empty()) return true;
    return default_;
}

template <meta::info obj, typename type, auto default_>
consteval type get_annotation() {
    auto ann = meta::annotations_of_with_type(obj, ^^type);
    if (!ann.empty()) return meta::extract<type>(ann[0]);
    return default_;
}

}  // namespace detail

}  // namespace annobind
