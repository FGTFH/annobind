# test-binds

基于 C++26、nanobind 与自定义 `annobind` 注解系统的 Python 扩展示例工程。

这个项目展示了如何使用 C++ 反射风格注解和 `nanobind`，将复杂的 C++ 类型、模板、命名空间、枚举、类、嵌套类型与线程安全对象绑定到 Python。

## 核心功能

- 结合 `nanobind` 与 `annobind` 注解，自动化生成 Python 绑定。
- 演示命名空间绑定、子模块绑定、枚举导出、类绑定、虚函数 trampoline、模板实例绑定。
- 提供参数重命名、默认值、关键字参数、跳过绑定、导出到主模块等注解控制。
- 绑定 `Queue<int>` 并通过 `nanobind::gil_scoped_release()` 释放 GIL，用于多线程测试。
- 自定义 CMake helper 自动生成 `.pyi` 类型存根，提高 Python 编辑器与类型检查体验。
- 演示重复绑定警告与绑定安全检查。

## 项目结构

- `CMakeLists.txt` - 根工程配置，查找 Python、nanobind，并添加子项目。
- `annobind/` - 自定义绑定库，通过注解读取和反射式绑定实现 Python 导出。
- `test/ab_test.cpp` - 核心示例模块，实现了 `MyClass`、枚举、函数、模板绑定、以及 `Queue<int>`。
- `include/t_queue.hpp` - 线程安全队列实现，`pop()` 释放 GIL。
- `cmake/NanobindCustomBuild.cmake` - 自定义 CMake 函数，生成 `.pyi` stub 并复制输出。
- `test.py` - Python 端调用示例。
- `test_gil.py` - GIL 释放与多线程行为测试。

## 依赖

- CMake >= 3.20
- Python >= 3.12
- `nanobind`
- `pybind11`/`pybind11-stubgen`（用于类型存根生成）
- Windows + MinGW/GCC 环境（示例使用 `E:/w64devkit` 作为编译环境）

## 快速开始

1. 创建并激活虚拟环境：

```powershell
.venv\Scripts\python.exe -m venv .venv
.venv\Scripts\python.exe -m pip install -U pip
```

2. 安装 Python 依赖：

```powershell
.venv\Scripts\python.exe -m pip install nanobind pybind11 pybind11-stubgen
```

3. 使用 CMake 编译项目：

```powershell
cmake -S . -B build
cmake --build build --config Release
```

> `cmake/NanobindCustomBuild.cmake` 中的 `nanobind_custom_build()` 会自动调用 `nanobind.stubgen` 生成 `.pyi` 文件，并将扩展与 stub 拷贝到 `bin/` 目录。

## 运行示例

在项目根目录中运行：

```powershell
.venv\Scripts\python.exe test.py
```

`test.py` 示例包含：

- 调用 `ab_test.f_int(2)`。
- 创建 `ab_test.MyClass` 并调用 `greet()`。
- 使用 `ab_test.IntArray10()` 填充数组并访问元素。

## GIL 释放测试

运行：

```powershell
.venv\Scripts\python.exe test_gil.py
```

`test_gil.py` 示例说明：

- `Queue<int>::pop()` 使用 `nanobind::gil_scoped_release()` 释放 Python GIL。
- 后台线程在 `pop()` 阻塞期间继续运行，用于验证 C++ 扩展是否支持并发。

## 亮点展示

- `annobind` 注解极大简化了绑定代码，避免手写大量 `nanobind` 绑定逻辑。
- 支持直接绑定模板实例，如 `ab::bind{^^templateMethod<int>}` 和 `ab::bind(^^std::array<int, 10>)`。
- 支持嵌套命名空间绑定、`plain` 注解导出到主模块、`skip` 注解跳过绑定项。
- 绑定类的 trampoline 机制支持虚函数重写。
- 提供类型存根文件，提升 Python 编辑器补全与类型检查效果。

## 目录说明

- `annobind/include/annobind` - `annobind` 头文件实现。
- `bin/ab_test` - 编译后生成的 Python 扩展包和 `.pyi` 存根。
- `test` - C++ 扩展源代码与 CMake 构建配置。
- `test.py` - Python 调试与功能演示脚本。
- `test_gil.py` - GIL 释放与多线程测试脚本。

## 扩展方向

- 增加更多注解示例（文档注释、导出值、skip_data、plain 等）。
- 支持更多 STL 与自定义泛型类型的自动绑定。
- 将 `annobind` 封装成独立可复用的 C++ 绑定库。
