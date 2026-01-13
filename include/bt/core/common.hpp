/**
 * @file common.hpp
 * @brief Backtrader C++ 通用定义和宏
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <cmath>

namespace bt {

// 版本信息
constexpr int VERSION_MAJOR = 0;
constexpr int VERSION_MINOR = 5;
constexpr int VERSION_PATCH = 0;

// 基础类型别名
using Size = std::size_t;
using Index = std::ptrdiff_t;  // 支持负索引
using Value = double;          // 时间序列值类型

// 特殊值
constexpr Value NaN = std::numeric_limits<Value>::quiet_NaN();
constexpr Value Inf = std::numeric_limits<Value>::infinity();

// 检查 NaN
inline bool isnan(Value v) { return std::isnan(v); }

// 默认的无界大小（unbounded mode）
constexpr Size UNBOUNDED = std::numeric_limits<Size>::max();

// 导出宏
#if defined(_WIN32) || defined(_WIN64)
    #ifdef BT_BUILDING_LIBRARY
        #define BT_API __declspec(dllexport)
    #else
        #define BT_API __declspec(dllimport)
    #endif
#else
    #define BT_API __attribute__((visibility("default")))
#endif

// 调试断言
#ifdef NDEBUG
    #define BT_ASSERT(cond) ((void)0)
#else
    #include <cassert>
    #define BT_ASSERT(cond) assert(cond)
#endif

// 禁用拷贝宏
#define BT_DISABLE_COPY(Class) \
    Class(const Class&) = delete; \
    Class& operator=(const Class&) = delete;

// 默认移动宏
#define BT_DEFAULT_MOVE(Class) \
    Class(Class&&) = default; \
    Class& operator=(Class&&) = default;

} // namespace bt
