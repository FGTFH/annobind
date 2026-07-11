#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/trampoline.h>

#include <array>
#include <meta>
#include <print>

#include "annobind.hpp"
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

    // 静态方法模板
    template <typename T>
    [[= ab::arg(0).defaul_(T{}).kw_only()]] static void templateMethod(T t) {
        std::println("templateMethod {}", t);
    }

    // 使用直接绑定声明是唯一的绑定模板函数的方法
    // 模板实例, 注意到最终有效的函数签名相当于ab::arg(0).rename("t_").defaul_(T{}).kw_only(),
    [[= ab::arg(0).rename("t_")]] static constexpr auto templateMethodInt = ab::bind{^^templateMethod<int>};

    // 类型尚未绑定的成员变量
    std::array<int, 10> q;
    // 嵌套类
    class InnerClass {
        // 未被禁用的默认构造函数会被隐式绑定
        InnerClass() = delete;
    };

    // 嵌套类模板
    template <typename T>
    class InnerTClass {
       public:
        int y = 9;
        void show(T x) const { std::println("InnerTClass y={}", y); }
    };
    //
    static constexpr auto templateInnerClass = ab::bind{^^InnerTClass<int>};
    [[= ab::plain()]] static constexpr auto templateInnerClassPlain = ab::bind{^^InnerTClass<float>};
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

// 命名空间内的函数
[[= ab::arg(0).rename("a_")]] int add(int a, [[= ab::defaul_(10).kw_only()]] int b) { return a + b; }

// 使用 bind 显式绑定模板实例化,可以跨命名空间
static constexpr auto f_int = ab::bind{^^f<int>};

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
    static constexpr auto templateInnerClassOuter = ab::bind{^^MyClass::InnerTClass<char>};
};
}  // namespace inner

// 使用 plain 标注将内层命名空间绑定到根模块
namespace[[= ab::plain()]] inner2 {
void mainLevelFunc() { std::println("This should be in main module"); }
}  // namespace inner2

namespace[[ = ab::plain(), = ab::skip() ]] inner3 {
void mainLevelFunc2() {};
}  // namespace inner3

// 尝试绑定STL容器. 作为聚合体，array有不能导出的成员_M_elems，需要skip_data, 当然, 它实际上并不适合直接导出
[[= ab::skip_data()]] static constexpr auto IntArray10 = ab::bind(^^std::array<int, 10>);

// 一个比较适合导出的Queue
static constexpr auto QueueInt = ab::bind(^^Queue<int>);

}  // namespace test

enum class Color2 { Black, Gray, White };

// <无限递归模型
namespace recursive2 {}

namespace recursive1 {
static constexpr auto n2 = ab::bind{^^recursive2};
}

namespace recursive2 {
static constexpr auto n1 = ab::bind{^^recursive1};
}
// 无限递归模型>

NB_MODULE(PROJECT_NAME, m) {
    // 启用重复注册检查与警告(RAII模式), 建议在调试阶段启用
    annobind::reg_guard rg;
    // 根据注解, 绑定整个命名空间 test 到模块 m
    auto test_mod = annobind::bind_to_module<^^test>(m);
    // 手动绑定
    annobind::bind_module_member<^^Color2>(m);

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