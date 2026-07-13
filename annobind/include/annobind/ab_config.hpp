#pragma once
#include <meta>
#ifndef AB_AUTO_MODULE_SRC
#define AB_AUTO_MODULE_SRC false
#endif
#ifndef AB_AUTO_TYPE_SRC
#define AB_AUTO_TYPE_SRC true
#endif
#ifndef AB_AUTO_FUNC_SRC
#define AB_AUTO_FUNC_SRC false
#endif
#ifndef AB_AUTO_DATA_MEMBER_SRC
#define AB_AUTO_DATA_MEMBER_SRC false
#endif
#ifndef AB_AUTO_PARAM_NAME
#define AB_AUTO_PARAM_NAME true
#endif
#ifndef AB_AUTO_EXPORT_ENUM
#define AB_AUTO_EXPORT_ENUM false
#endif
#ifndef AB_AUTO_SKIP_NESTED
#define AB_AUTO_SKIP_NESTED false
#endif
#ifndef AB_AUTO_SKIP_FUNC
#define AB_AUTO_SKIP_FUNC false
#endif
#ifndef AB_AUTO_SKIP_DATA
#define AB_AUTO_SKIP_DATA false  // 或许在大多数情况下应被设为true
#endif
#ifndef AB_AUTO_PLAIN_MODULE
#define AB_AUTO_PLAIN_MODULE false
#endif
#ifndef AB_AUTO_PLAIN_NEST_CLASS
#define AB_AUTO_PLAIN_NEST_CLASS false
#endif

namespace annobind {
// ============= 自动绑定全局设置 ==============
namespace conf {
// 自动附加源码位置到doc
constexpr bool auto_module_src = AB_AUTO_MODULE_SRC;
constexpr bool auto_type_src = AB_AUTO_TYPE_SRC;
constexpr bool auto_func_src = AB_AUTO_FUNC_SRC;
constexpr bool auto_data_member_src = AB_AUTO_DATA_MEMBER_SRC;

// 自动设置函数参数名
constexpr bool auto_param_name = AB_AUTO_PARAM_NAME;
// 自动导出枚举值到绑定作用域
constexpr bool auto_export_enum = AB_AUTO_EXPORT_ENUM;

// 自动跳过
constexpr bool auto_skip_nested = AB_AUTO_SKIP_NESTED;  // 命名空间/类
constexpr bool auto_skip_func = AB_AUTO_SKIP_FUNC;
constexpr bool auto_skip_data = AB_AUTO_SKIP_DATA;

// 扁平化绑定作用域
constexpr bool auto_plain_module = AB_AUTO_PLAIN_MODULE;
constexpr bool auto_plain_nest_class = AB_AUTO_PLAIN_NEST_CLASS;

}  // namespace conf
}  // namespace annobind
