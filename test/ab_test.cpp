#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/trampoline.h>

#include <array>
#include <meta>
#include <print>
#include <ranges>

#include "annobind/ab_register.hpp"
#include "annobind/annobind.hpp"
#include "t_queue.hpp"

namespace nb = nanobind;
namespace ab = annobind;
namespace meta = std::meta;

template <typename T>
[[= ab::doc(std::string("Template function[T=") + meta::display_string_of(^^T) + "]")]] void f(T t) {
    std::println("f called with {}", t);
}

// 命名空间 test，包含各种元素
namespace[[= ab::with_nested_src()]] test {

// 枚举
enum class[[= ab::export_values()]] Color { Red, Green, Blue };

namespace inner {
struct IInnerClass;
}

// 跳板类前向声明
struct PyMyClass;
// 类
class[[= ab::trampo{^^PyMyClass}]] MyClass {
   private:
    int e, f, g, h;

   public:
    int x = 0;
    std::string name = "default";
    static constexpr int staticConstexprInt = 42;
    inline static int staticInt = 43;

    MyClass(int x_, std::string name_) : x(x_), name(name_) {}

    // 自动处理适用于python的运算符(+ -> __add__)
    MyClass operator+(const MyClass& other) const { return MyClass(x + other.x, name + other.name); }

    virtual void greet() const { std::println("Hello, {} (x={})", name, x); }

    // 普通成员函数, 参数注解中的第一个非ab::arg标注会被视为默认值
    [[= ab::arg("x_").rename("new_x")]] void setX([[= 42]] int x_) { x = x_; }

    // 绑定 property rw -- 以成员变量之名命名
    [[= ab::property(^^MyClass::e)]] int get_e() const { return e; }  // 实际上可自动生成这样的getter
    [[ = ab::property(^^MyClass::e), = ab::arg(0).rename("e").defaul_(10).kw_only() ]] void set_e(int e_) { e = e_; }

    // 绑定 property rw -- 以字符串命名
    [[= ab::property("f")]] void set_f(int f_) { f = f_; }
    [[= ab::property("f")]] auto get_f() { return f; }  // 直接以字符串声明的property需要提供getter

    // 绑定 property ro
    [[= ab::property("g")]] auto get_g() { return g; }  // 只给出getter的property将read-only

    // 无法绑定 property wo
    // [[= ab::property("h")]] void set_h(int h_) { h = h_; } // 无法定义write-only property

    // 静态方法模板
    template <typename T>
    [[= ab::arg(0).defaul_(T{}).kw_only()]] static void templateMethod(T t) {
        std::println("templateMethod {}", t);
    }

    // 模板实例, 注意到最终有效的函数签名相当于ab::arg(0).rename("t_").defaul_(T{}).kw_only()
    // 这里使用了直接绑定声明 bind + annoset(列表初始化)
    static constexpr auto templateMethodInt = ab::bind<^^templateMethod<int>, {
                                                                                  ab::arg(0).rename("t_")}>{};

    // 类型尚未绑定的成员变量
    std::array<int, 10> q;
    // 嵌套类
    class InnerClass {
        // 未被禁用的默认构造函数会被隐式绑定
        InnerClass() = delete;
        // 显式声明的复制构造函数会被绑定
        InnerClass(const InnerClass&) = default;
    };

    // 嵌套类模板
    template <typename T>
    class InnerTClass {
       public:
        int y = 9;
        void show(T x) const { std::println("InnerTClass y={}", y); }
    };
    //
    static constexpr auto templateInnerClass = ab::bind<^^InnerTClass<int>>{};
    static constexpr auto templateInnerClassPlain = ab::bind<^^InnerTClass<float>, ab::annoset(ab::plain{})>{};
    // typedef类型别名可以被绑定
    typedef InnerTClass<double> templateInnerClassDef[[= ab::unskip()]];

    // using声明无法绑定, 因为似乎不能对它们进行标注
    // using usingInnerClassDouble = InnerTClass<double>[[= ab::unskip()]];

    // 嵌套枚举
    enum class NestedEnum { A, B, C };
};

// 跳板类会被自动跳过
struct PyMyClass : public MyClass {
    NB_TRAMPOLINE(MyClass, 1);
    void greet() const override { NB_OVERRIDE(greet); }
};
// 命名空间内的变量
int defaultVar;  // 默认初始化
auto globalString = "hello";
int globalVar = 42;
static int staticGlobal = 43;
static const int statiConstGlobalVar = 44;
const int constGlobal = 45;
constexpr int constexprGlobal = 46;

namespace[[= ab::skip()]] conf {
// 可预配置注解集, 这在部分手动绑定时或许很有用
constexpr auto add_a = ab::annoset(ab::arg(0).rename("a_"), ab::rename("add_a"));
}  // namespace conf

// 命名空间内的函数, 这里使用了预配置的注解集
[[= conf::add_a]] int add(int a, [[= ab::defaul_(10).kw_only()]] int b) { return a + b; }

// 使用 bind 显式绑定模板实例化,可以跨命名空间
static constexpr auto f_int = ab::bind<^^f<int>>{};
// 嵌套命名空间->子模块
namespace inner {
[[= ab::doc("Inner function")]] void innerFunc() { std::println("inner"); }
struct IInnerClass {
    // 嵌套类与嵌套作用域
    struct IInnerClass1 {
    } i1;
    // 将绑定到根模块
    [[= ab::skip()]] struct[[= ab::plain()]] IInnerClass2 {
    } i2;

    // 虽然类可以绑定到任何地方，但不建议也没有必要这样做
    static constexpr auto templateInnerClassOuter = ab::bind<^^MyClass::InnerTClass<char>>{};
};
}  // namespace inner

// 使用 plain 标注将内层命名空间绑定到根模块
namespace[[= ab::plain()]] inner2 {
void mainLevelFunc() { std::println("This should be in main module"); }
}  // namespace inner2

namespace[[ = ab::plain(), = ab::skip() ]] inner3 {
void mainLevelFunc2() {};
}  // namespace inner3
using namespace inner3;  //  并不会污染父命名空间, 即mainLevelFunc2不会被认为是test的成员而被自动绑定

// 尝试绑定STL容器. 作为聚合体，array有不能导出的成员_M_elems，需要skip_data, 当然, 它实际上并不适合直接导出
[[= ab::skip_data()]] static constexpr auto IntArray10 =
    ab::bind<^^std::array<int, 10>, ab::annoset(ab::skip_data{})>{};

// 一个比较适合导出的Queue
static constexpr auto QueueInt = ab::bind<^^Queue<int>>{};

}  // namespace test

enum class Color2 { Black, Gray, White };

// <无限递归模型
namespace recursive2 {}

namespace recursive1 {
static constexpr auto n2 = ab::bind<^^recursive2>{};
}

namespace recursive2 {
static constexpr auto n1 = ab::bind<^^recursive1>{};
}
// 无限递归模型>

NB_MODULE(PROJECT_NAME, m) {
    // 启用重复注册检查与警告(RAII模式), 建议在调试阶段启用
    annobind::reg_guard _reg_guard;
    auto reg = annobind::ABRegistry::instance();
    // 根据注解, 绑定整个命名空间 test 到模块 m
    auto test_mod = annobind::bind_to_module<^^test>(m);
    // 手动绑定
    // - 绑定模块成员
    annobind::bind_module_member<^^Color2>(m);

    // -绑定类成员
    // --查找类的绑定实例(需要开启reg_guard)
    auto inner_cls_p = reg.find_class<test::inner::IInnerClass>();
    // --进行绑定
    if (inner_cls_p) {
        auto inner_cls = *inner_cls_p;
        static constexpr auto bind_i2 = ab::bind<^^test::inner::IInnerClass::i2, ab::annoset(ab::rename("i2"))>{};
        annobind::bind_class_member<^^bind_i2>(inner_cls);
        // 或者直接
        //  annobind::bind_class_member<^^test::inner::IInnerClass::i2>(inner_cls);

        auto f = [](test::inner::IInnerClass& self, int a) { return a; };
        // 可以直接使用nb接口绑定函数.
        inner_cls.def("f", f);
        // decltype(f2) 是lambda匿名类, 会被当作普通类来处理(即将其绑定为嵌套类), 注意:
        // 0. 得到的是(嵌套)类而非(成员)函数
        // 1. 必须显式提供名称(通过 直接bind声明 / rename)
        // 2. 作用域须额外指定
        // 3. 其成员(包括operator())对外不可见, 将得到空类
        // 4. 使annobind能够转发lambda绑定虽然技术上可行, 但对于这种零散的绑定, 用户代码量相比nanobind并不会减少
        static constexpr auto lambda_class = ab::bind<^^decltype(f)>{};                      // 1.
        annobind::bind_class_member<^^lambda_class, ^^test::inner::IInnerClass>(inner_cls);  // 2. 3. 4.
    }
    //  手动绑定默认会忽略skip注解. 注意plain注解仍生效了,没有创建新的子模块, 且mainLevelFunc2在主模块中
    annobind::bind_module_member<^^test::inner3>(m, m);

    // 重复绑定类/枚举/子模块会触发警告, 并打断绑定
    annobind::bind_module_member<^^test::MyClass>(m);
    annobind::bind_module_member<^^Color2>(m);
    annobind::bind_submodule<^^test::inner>(m);

    // 由于子模块是由最终生效的名称标识的, bind_submodule会将每个命名空间别名视为独立的子模块,
    // 并绑定原命名空间的成员到子模块. 这通常应被视为错误, 请小心应对.
#define ALIAS_TEST(n)              \
    namespace ia##n = test::inner; \
    annobind::bind_submodule<^^ia##n>(m);
    ALIAS_TEST(1);
    ALIAS_TEST(2);
    ALIAS_TEST(3);
#undef ALIAS_TEST

    // 这将导致Stack overflow
    // annobind::bind_submodule<^^recursive1>(m);
}