cmake_minimum_required(VERSION 3.20)
project(python_ext LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(
    -freflection
)

link_libraries(E:/w64devkit/lib/gcc/x86_64-w64-mingw32/16.1.0/libstdc++exp.a)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(NanobindCustomBuild)

# 设置 Python 根目录并查找
set(Python_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/.venv/Scripts)
find_package(Python REQUIRED COMPONENTS Interpreter Development)

# 设置 nanobind 目录并查找
execute_process(
    COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE nanobind_ROOT
)
find_package(nanobind CONFIG REQUIRED)

add_subdirectory(annobind)
add_subdirectory(test)