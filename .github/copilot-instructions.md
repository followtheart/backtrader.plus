# Backtrader C++ - AI Agent 开发指南

## 项目概述

Python Backtrader 量化回测框架的 C++17 高性能重构版本，支持 SIMD 向量化、多线程优化及 pybind11 Python 绑定。

## 架构核心

```
Cerebro (引擎) → Strategy (策略) → Broker (经纪商) → Order/Trade (订单)
      ↓
DataFeed (数据源) → LineSeries → LineBuffer (核心数据结构)
      ↓
Indicator (指标) → SIMD 优化计算
```

### 关键设计模式

1. **LineBuffer 索引**: `[0]`=当前, `[1]`=前一个, `[-1]`=未来值(仅指标)
2. **参数宏**: `BT_PARAMS_BEGIN() BT_PARAM(period, 30) BT_PARAMS_END()`
3. **策略生命周期**: `init()` → `start()` → `prenext()` → `nextstart()` → `next()` → `stop()`

## 构建与测试

```powershell
# 快速构建 (Windows)
.\build.ps1

# 手动构建
mkdir build; cd build
cmake .. -DBT_BUILD_TESTS=ON -DBT_ENABLE_SIMD=ON
cmake --build . --config Release --parallel
ctest -C Release --output-on-failure
```

**CMake 选项**: `BT_BUILD_TESTS`, `BT_BUILD_PYTHON`, `BT_BUILD_EXAMPLES`, `BT_ENABLE_SIMD` (默认 ON)

## 调试与错误排查

### 常见编译错误
- **未定义符号**: 确保 `#include "bt/backtrader.hpp"` 或具体头文件
- **链接失败**: 检查 `target_link_libraries` 是否包含 `backtrader_core`
- **SIMD 错误**: 关闭 `-DBT_ENABLE_SIMD=OFF` 测试是否为对齐问题

### 运行时调试
- **NaN 值检查**: 使用 `bt::isnan(value)` 检测无效数据
- **索引越界**: LineBuffer 越界返回 `NaN` 而非抛异常（const 版本）
- **测试隔离**: 单独运行测试 `ctest -R test_name -V`

### GTest 调试技巧
```powershell
# 运行单个测试
.\build\bin\Release\backtrader_tests.exe --gtest_filter=TestSuite.TestCase

# 显示详细输出
.\build\bin\Release\backtrader_tests.exe --gtest_filter=* --gtest_print_time=1
```

## 开发规范

### 新增指标 (`include/bt/indicators/`)
```cpp
class MyIndicator : public bt::Indicator {
    BT_PARAMS_BEGIN()
        BT_PARAM(period, 14)
    BT_PARAMS_END()
    
    void next() override { /* 逐 bar 计算 */ }
    void once(Size start, Size end) override { /* SIMD 批量计算 */ }
};
```

### 新增策略
```cpp
class MyStrategy : public bt::Strategy {
    void init() override { setMinPeriod(20); }
    void next() override {
        if (position() == 0 && data(0)->close()[0] > data(0)->close()[1])
            buy();
    }
};
```

## Python ↔ C++ API 对照

| Python Backtrader | C++ Backtrader |
|-------------------|----------------|
| `self.data.close[0]` | `data(0)->close()[0]` |
| `self.data.close[-1]` | `data(0)->close()[1]` (注意符号相反) |
| `bt.indicators.SMA(period=20)` | `bt::indicators::SMA(data, 20)` |
| `params = (('period', 14),)` | `BT_PARAM(period, 14)` |
| `self.buy()` | `buy()` |
| `self.position.size` | `position()` |
| `cerebro.addstrategy(MyStrategy)` | `cerebro.addStrategy<MyStrategy>()` |
| `cerebro.broker.setcash(100000)` | `cerebro.broker().setCash(100000)` |

**重要差异**: Python 负索引 `[-1]` 表示前一个值，C++ 用正索引 `[1]`

## 关键文件速查

| 文件 | 用途 |
|------|------|
| `include/bt/backtrader.hpp` | 主头文件 |
| `include/bt/cerebro.hpp` | 回测引擎 |
| `include/bt/strategy.hpp` | 策略基类 |
| `include/bt/linebuffer.hpp` | 时间序列核心 |
| `include/bt/simd.hpp` | SIMD 向量化 |
| `python/bindings.cpp` | Python 绑定 |

## 类型定义 (`include/bt/common.hpp`)

```cpp
using Size = std::size_t;      // 大小/长度
using Index = std::ptrdiff_t;  // 支持负索引
using Value = double;          // 时间序列值
constexpr Value NaN = ...;     // 无效值标记
```

## 注意事项

1. **索引方向**: `[0]` 是当前而非第一个，与 Python 负索引方向相反
2. **SIMD 对齐**: 向量化计算需 32 字节对齐，使用 `simd::` 函数自动处理
3. **线程安全**: `Optimizer` 使用 `ThreadPool` 并行，单策略回测非线程安全
4. **内存模式**: Unbounded 保存全部历史，QBuffer 固定大小滑动窗口
