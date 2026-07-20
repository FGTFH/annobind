#pragma once
#include <nanobind/nanobind.h>

#include <meta>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

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

// =============== 属性集 ===============
// 将多个ab标注放在一起, 方便手动绑定(目前不支持嵌套).
// 出于效率和处理便宜方面的考虑, 使用静态数组. 空间上应该完全不用关心, 毕竟只在编译期有效

struct annoset {
    static constexpr size_t maxN = conf::attrs_max_n;
    std::array<meta::info, maxN> arr;
    size_t size_;
    template <typename... Ts>
        requires(sizeof...(Ts) != 1 || (... && !std::same_as<std::decay_t<Ts>, annoset>))
    consteval annoset(Ts&&... vals)
        : arr{meta::reflect_constant(std::forward<decltype(vals)>(vals))...}, size_(sizeof...(vals)) {
        static_assert(sizeof...(vals) <= maxN,
                      "too many annotations in a single annoset, you may need to increase maxN");
    };

    consteval annoset(std::span<const meta::info> const& objs) : arr{}, size_(objs.size()) {
        std::copy(objs.begin(), objs.end(), arr.begin());
    };

    consteval auto begin(this auto&& self) { return self.arr.begin(); }
    consteval auto end(this auto&& self) { return self.arr.begin() + self.size_; }
    consteval auto& operator[](this auto&& self, size_t i) { return self.arr[i]; }

    consteval auto size() const { return size_; }

    consteval void extend(annoset const& objs) {
        std::copy(objs.begin(), objs.end(), end());
        size_ += objs.size();
    }

    static consteval annoset concat(const annoset& a, const annoset& b) {
        annoset res = a;
        std::copy(b.begin(), b.end(), res.end());  // 具有边界检查
        res.size_ += b.size();
        return res;
    }
};

// 替代实现, 但使用时需要检查模板参数, 非常不方便
//  template <size_t N>
//  struct annoset {
//      std::array<meta::info, N> annoset;
//      consteval annoset(auto&&... args) : annoset{meta::reflect_constant(std::forward<decltype(args)>(args))...} {
//          static_assert(sizeof...(args) == N);
//      };
//  };
// annoset(auto&&... args) -> annoset<sizeof...(args)>;

// =============== 主动绑定声明 ===============
// 主动绑定一个可绑定实体(可跨作用域, 但需有效), 可用于(成员)模板函数/类等的绑定.
// 在任何地方, 都建议以
// static constexpr fi = ab::bind<^^f<int>,ab::annoset(...)>
// 的形式声明, ab::annoset中的与template<>f的标注都会生效, 且覆盖优先级最高
// 该类提供了将标注参数化的方式, 这在手动绑定中可能会很有用
template <std::meta::info obj_, annobind::annoset annoset_ = {}>
struct bind {
    [[= annoset_]] static constexpr auto obj = obj_;
};

// =============== attr相关辅助函数 ===============
consteval bool is_ab_annotation(meta::info anno) {
    return meta::parent_of(meta::remove_cvref(meta::type_of(anno))) == ^^annobind;
}
consteval bool is_bind_declaration(meta::info member) {
    if (!meta::is_variable(member))
        return false;
    else {
        meta::info type = meta::remove_cvref(meta::type_of(member));
        if (!meta::has_template_arguments(type))
            return false;
        else
            return meta::template_of(type) == ^^bind;
    }
}

consteval auto annotations_of_with_type_a(meta::info obj, meta::info type) {
    std::vector<meta::info> anns{std::from_range, meta::annotations_of_with_type(obj, type)};
    for (const auto& atrset : meta::annotations_of_with_type(obj, ^^annoset)) {
        for (const auto& ann : meta::extract<annoset>(atrset)) {
            if (meta::remove_cvref(meta::type_of(ann)) == type) anns.push_back(ann);
        }
    }
    return anns;
}

consteval auto annotations_of_a(meta::info obj) {
    std::vector<meta::info> anns;
    for (const auto& ann : meta::annotations_of(obj)) {
        if (meta::remove_cvref(meta::type_of(ann)) != ^^annoset) {
            anns.push_back(ann);
        } else {
            for (const auto& an : meta::extract<annoset>(ann)) {
                anns.push_back(an);
            }
        }
    }
    return anns;
}
namespace detail {
template <meta::info obj_b>
consteval auto take_bind_obj() {
    if constexpr (is_bind_declaration(obj_b)) {
        using Bind = typename[:std::meta::type_of(obj_b):];
        return Bind::obj;
    }
    return obj_b;
}

template <meta::info obj_b>
consteval auto take_bind_anno_obj() {
    using Bind = typename[:std::meta::type_of(obj_b):];
    return ^^Bind::obj;
}

// 通用优先级检查辅助函数
template <meta::info obj_b, typename Checker, typename T>
consteval T check_sources_ex(Checker&& checker, T default_val) {
    // 0. 空值
    if constexpr (obj_b == meta::info{})
        return default_val;
    else {
        constexpr auto obj = take_bind_obj<obj_b>();
        // 1. 主动绑定声明
        if constexpr (obj_b != obj)
            if (auto res = checker(take_bind_anno_obj<obj_b>()); res.has_value()) return *res;
        // 2. 实体(可能为别名)
        if (auto res = checker(obj); res.has_value()) return *res;
        // 3. 别名dealias
        if (obj != meta::dealias(obj))
            if (auto res = checker(meta::dealias(obj)); res.has_value()) return *res;
        return default_val;
    }
}

template <meta::info obj_b, typename pro, typename con, bool default_>
consteval bool check_anno_bool_value_ex() {
    auto checker = [](meta::info src) -> std::optional<bool> {
        if (!annotations_of_with_type_a(src, ^^pro).empty()) return true;
        if (!annotations_of_with_type_a(src, ^^con).empty()) return false;
        return std::nullopt;
    };
    return check_sources_ex<obj_b>(checker, default_);
}
template <meta::info obj_b, typename pro, bool default_>
consteval bool check_anno_bool_value_ex() {
    auto checker = [](meta::info src) -> std::optional<bool> {
        if (!annotations_of_with_type_a(src, ^^pro).empty()) return true;
        return std::nullopt;
    };
    return check_sources_ex<obj_b>(checker, default_);
}

template <meta::info obj_b, typename type, auto default_>
consteval type get_annotation_ex() {
    auto checker = [](meta::info src) -> std::optional<type> {
        auto ann = annotations_of_with_type_a(src, ^^type);
        if (!ann.empty()) return meta::extract<type>(ann[0]);
        return std::nullopt;
    };
    return check_sources_ex<obj_b>(checker, default_);
}

template <meta::info obj, typename pro, typename con, bool default_>
consteval bool check_anno_bool_value() {
    if (!annotations_of_with_type_a(obj, ^^pro).empty()) return true;
    if (!annotations_of_with_type_a(obj, ^^con).empty()) return false;
    return default_;
}

template <meta::info obj, typename pro, bool default_>
consteval bool check_anno_bool_value() {
    if (!annotations_of_with_type_a(obj, ^^pro).empty()) return true;
    return default_;
}

template <meta::info obj, typename type, auto default_>
consteval type get_annotation() {
    auto ann = annotations_of_with_type_a(obj, ^^type);
    if (!ann.empty()) return meta::extract<type>(ann[0]);
    return default_;
}

}  // namespace detail

}  // namespace annobind
