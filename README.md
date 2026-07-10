# Annobind

**基于 C++26 反射与 nanobind 的自动绑定库**

[![C++26](https://img.shields.io/badge/C%2B%2B-26-blue)](https://en.cppreference.com/w/cpp/compiler_support/26)
[![nanobind](https://img.shields.io/badge/nanobind-latest-green)](https://github.com/wjakob/nanobind)
[![License](https://img.shields.io/badge/license-MIT-blue)](LICENSE)

`annobind` 借助 C++26 的静态反射（P2996）和 nanobind 实现 **自动导出** C++ 实体（类、命名空间、函数、变量、枚举）到 Python，极大减少手写绑定代码，同时通过 **编译期注解** 提供精细控制。

---

## ✨ 核心特性

- **零胶水代码**：一条 `annobind::bind_to_module<^^MyNamespace>(m)` 即可导出整个命名空间。
- **丰富的注解系统**（`ab::` 前缀）：控制导出、命名、文档、参数默认值、参数标志等。
- **原生 nanobind 支持**：无缝集成 `nb::call_guard`, `nb::keep_alive`, `nb::rv_policy` 等。
- **模板实例化支持**：通过 `ab::bind{^^f<int>}` 显式绑定模板实体。
- **重复绑定检测**：可选 `reg_guard` 在调试时警告重复导出。
- **完全编译期**：所有反射与注解处理均在编译期完成，运行时零开销。

---

## 📦 快速开始

### 1. 环境要求

- C++26 编译器（需支持 `-freflection`），如 **GCC 16+**。
- Python 3.12+。
- uv（Python 包管理器）。
- [nanobind](https://github.com/wjakob/nanobind)（已安装 Python 包）。
- CMake。

### 2. 将 annobind 加入项目

```cmake
# 你的 CMakeLists.txt
add_subdirectory(annobind)  # 将 annobind 源码放在同级目录
target_link_libraries(your_module PRIVATE annobind)
# 确保 C++26 与 -freflection 已启用
```

### 3. 编写绑定

```cpp
#include <annobind.hpp>
#include <nanobind/nanobind.h>

namespace ab = annobind;
namespace nb = nanobind;

// 要导出的 C++ 代码
namespace demo {
    int global_var = 42;
    void hello() { std::println("Hello!"); }
    struct MyClass { int x; };
}

NB_MODULE(my_ext, m) {
    ab::reg_guard rg;  // (可选) 启用重复绑定检查
    ab::bind_to_module<^^demo>(m);
}
```

一行代码自动导出 `demo` 命名空间内的变量、函数、类型（及其成员）到 Python。

---

## 🧩 核心接口

| 接口 | 说明 |
|------|------|
| `bind_to_module<^^ns>(nb_module, base_opt)` | 将命名空间 `ns` 的成员绑定到 `nb_module`（子成员可递归绑定） |
| `bind_submodule<^^ns>(nb_module, base_opt)` | 将 `ns` 创建为 `nb_module` 的子模块，并绑定其成员 |
| `bind_class<^^T>(scope, base_opt)` | 将类 `T` 绑定到 `scope`（模块或类） |
| `bind_enum<^^E>(scope)` | 绑定枚举 |
| `bind_function<^^func>(module)` | 绑定单个函数 |
| `bind_module_member<^^member>(module, base_opt)` | 绑定命名空间成员（可跨命名空间） |
| `bind_class_member<^^member>(class_obj, base_opt)` | 绑定类成员 |
| `bind_bind<^^ab::bind{...}>(scope, base)` | 处理显式 `ab::bind` 声明 |

所有 `*_opt` 参数均为 `std::optional`，表示“根作用域”（用于 `plain` 标注）。

---

## 🏷️ 注解系统 (Attributes)

注解使用 C++26 语法：`[[= ab::annotation(...)]]`。

### 通用注解

| 注解 | 含义 |
|------|------|
| `skip` | 跳过该实体（优先级高于 `unskip`） |
| `unskip` | 强制导出（即使所在作用域被 `skip`） |
| `with_src` | 在文档中添加源码位置 |
| `doc("...")` | 自定义文档字符串（不同来源的会进行拼接） |
| `rename("...")` | 自定义 Python 名称 |
| `plain` | 将嵌套类型/命名空间提升到根作用域（忽略嵌套层级） |

### 函数/方法注解

| 注解 | 含义 |
|------|------|
| `auto_param_name` | 自动为参数生成名称（默认启用） |
| `arg(index/name).rename("x")` | 指定参数名 |
| `arg(index/name).none()` / `.noconvert()` / `.lock()` | 对应 `nb::arg` 标志 |
| `arg(index/name).kw_only()` | 该参数变为仅关键字参数 |
| `arg(index/name).defaul_(value)` 或 `arg(index/name) = value` | 设置默认值（值需编译期常量） |
| `arg()....` | 直接在参数注解中中使用以上接口 |

### 类/命名空间注解

| 注解 | 含义 |
|------|------|
| `with_func_src` | 为所有成员函数附加源码位置 |
| `with_data_src` | 为所有数据成员附加源码位置 |
| `with_nested_src` | 为嵌套类型附加源码位置 |
| `skip_func` / `skip_data` / `skip_nested` | 跳过对应类别 |
| `trampo{^^PyT}` | 指定跳板类（`nb::class_` 的 trampoline） |
| `export_values` | 枚举：导出枚举值到父作用域（同 `enum_.export_values()`） |
| `plain` | 将嵌套类型绑定到到根作用域, 将命名空间成员绑定到根作用域 |

### 显式绑定声明

```cpp
static constexpr auto my_bind = ab::bind{^^some_template<int>};
```

该声明会触发 `some_template<int>` 的实例化, 并在被自动遍历到时将其绑定到当前作用域, 请不要在类中绑定非成员函数/变量。其上的 `ab::arg` 等注解会覆盖实体本身的注解。

---

## ⚙️ 全局配置宏

在编译时通过宏控制默认行为（可在 `CMakeLists.txt` 中定义）：

```cpp
#define AB_AUTO_MODULE_SRC         false   // 模块附加源码
#define AB_AUTO_TYPE_SRC           false   // 类型附加源码
#define AB_AUTO_FUNC_SRC           false   // 函数附加源码
#define AB_AUTO_DATA_MEMBER_SRC    false   // 数据成员附加源码
#define AB_AUTO_PARAM_NAME         true    // 自动生成参数名
#define AB_AUTO_EXPORT_ENUM        false   // 自动 export_values
#define AB_AUTO_SKIP_NESTED        false   // 默认跳过嵌套类型
#define AB_AUTO_SKIP_FUNC          false   // 默认跳过函数
#define AB_AUTO_SKIP_DATA          false   // 默认跳过数据成员
#define AB_AUTO_PLAIN_MODULE       false   // 默认 plain 到模块
#define AB_AUTO_PLAIN_NEST_CLASS   false   // 默认 plain 到类
```

---

## 🧪 测试示例

项目自带的 `test/ab_test.cpp` 展示了所有功能，包括：

- 类（含虚拟函数、跳板类）
- 静态/常量成员
- 模板函数与模板类的显式实例化
- 嵌套命名空间与 `plain` 提升
- 枚举 `export_values`
- `skip`/`unskip` 控制
- 命名空间别名绑定（需谨慎）

运行测试后，生成的 `ab_test.pyi` 存根文件可验证导出结果。

---

## ⚠️ 注意事项

1. **模板实例化**：普通模板不会自动实例化，必须通过 `ab::bind{^^T<Args>}` 手动绑定。
2. **类型别名**：`using` 声明无法携带注解，因此不会被自动绑定；`typedef` 可携带注解（需 `unskip` 才会自动被绑定）。
3. **命名空间别名**：绑定别名会创建一个新的子模块，其内容与原命名空间相同（重复），可能导致混淆，应尽量避免。
4. **递归绑定**：如命名空间/类中通过 `ab::bind` 循环绑定。栈溢出警告。
5. **`reg_guard`**：建议在调试阶段使用，它会启用 `ABRegistry` 以检测重复绑定模块、类（枚举）的重复绑定；生产环境可移除（或条件编译）。
6. **特殊成员**：复制/移动构造函数、赋值运算符、析构函数会被自动排除, 但可用的默认构造函数、用户声明的函数则不会。

---

## 🛠️ 构建与集成

### 样例工程

本项目本身是一份采用 **CMake** 构建的**完整样例工程**，包含：
- `annobind` 接口库（`./annobind/`）
- 测试绑定模块（`./test`）
- CMake 辅助脚本（`./cmake/NanobindCustomBuild.cmake`）
- 包管理配置（`./pyproject.toml`）

所有功能均已集成，可直接编译、运行，验证注解系统与自动绑定效果。

### 构建样例

在项目根目录执行：
```bash
uv sync
cmake -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo  -DCMAKE_C_COMPILER:FILEPATH=path/to/gcc -DCMAKE_CXX_COMPILER:FILEPATH=path/to/g++ -B ./build
cmake --build ./build --config RelWithDebInfo --target install --
```
这将直接编译测试样例，生成python二进制拓展与存根文件，并安装到 `./bin/` 目录下。

### 将 annobind 集成到你的项目

将 `annobind` 接口库放入你的项目，然后在顶层 `CMakeLists.txt` 中添加：
```cmake
add_subdirectory(annobind)
target_link_libraries(your_module PRIVATE annobind)
```
并确保你的 `your_module` 已启用 C++26 与 `-freflection`。

当然，`annobind`是纯头文件库，你可以直接将其添加到目标的头文件搜索路径中。

## 📄 许可证

MIT License. 详情见 [LICENSE](LICENSE)。

---

## 🙏 致谢

- [nanobind](https://github.com/wjakob/nanobind) 提供轻量级 Python 绑定。
- C++26 反射提案 [P2996](https://wg21.link/p2996) 的实现者（GCC/Clang）。

---

**Happy auto-binding!** 🚀
