# Backtrader C++

Backtrader Python 量化回测框架的 C++ 重构版本。

## 项目状态

**阶段 1: 基础设施** ✅ 完成

- [x] CMake 构建系统
- [x] LineBuffer 核心数据结构
- [x] LineSeries 多线容器
- [x] 参数系统 (params)
- [x] 指标基类
- [x] SMA/EMA/MACD/RSI/BollingerBands 指标
- [x] pybind11 Python 绑定（基础）
- [x] Google Test 单元测试

**阶段 2: 核心引擎** ✅ 完成

- [x] Cerebro 回测引擎
  - [x] 数据管理 (addData)
  - [x] 策略管理 (addStrategy)
  - [x] 分析器管理 (addAnalyzer)
  - [x] 观察器管理 (addObserver)
  - [x] 运行控制 (run/runOnce)
- [x] Strategy 策略框架
  - [x] 生命周期方法 (init/start/prenext/nextstart/next/stop)
  - [x] 交易方法 (buy/sell/close/cancel)
  - [x] 持仓查询 (position/getPosition)
  - [x] 订单/交易通知 (notifyOrder/notifyTrade)
- [x] Broker 经纪商
  - [x] 账户管理 (cash/value)
  - [x] 订单处理 (buy/sell/cancel)
  - [x] 持仓跟踪
  - [x] 佣金计算
- [x] DataFeed 数据源
  - [x] 内存数据源 (MemoryDataFeed)
  - [x] CSV数据源 (CSVDataFeed)
  - [x] OHLCV数据结构
- [x] Order/Trade 订单系统
  - [x] 多种订单类型 (Market/Limit/Stop/StopLimit)
  - [x] 订单状态跟踪
  - [x] 交易记录
- [x] Analyzers 分析器
  - [x] TradeAnalyzer
  - [x] ReturnsAnalyzer  
  - [x] SharpeRatio
  - [x] DrawDown
  - [x] AnnualReturn
  - [x] SQN
- [x] Observers 观察器
  - [x] CashObserver
  - [x] ValueObserver
  - [x] DrawDownObserver
  - [x] BuySellObserver
  - [x] TradesObserver

**阶段 3: 高级特性** ✅ 完成

- [x] 多时间周期支持
  - [x] TimeFrame 枚举 (Ticks, Seconds, Minutes, Days, Weeks, Months, Years)
  - [x] Resampler 数据重采样
  - [x] ResampledDataFeed
- [x] 完整订单系统
  - [x] OrderExecutionBit 执行位
  - [x] OrderData 执行数据
  - [x] OCO (One-Cancels-Other) 订单
  - [x] Bracket 订单 (主订单+止损+止盈)
  - [x] StopTrail/StopTrailLimit 跟踪止损
  - [x] ValidUntil 订单有效期
- [x] 信号系统
  - [x] 14种信号类型 (SIGNAL_LONG, SIGNAL_SHORT, SIGNAL_LONGEXIT, etc.)
  - [x] Signal 指标类
  - [x] SignalGroup 信号分组
  - [x] SignalStrategy 信号策略
  - [x] CrossoverSignalStrategy SMA交叉信号策略

**阶段 4: 性能优化** ✅ 完成

- [x] SIMD 向量化计算
  - [x] 自动检测 SIMD 级别 (AVX512/AVX2/AVX/SSE2/Scalar)
  - [x] simd::sum, simd::mean, simd::dot 等基础运算
  - [x] simd::slidingMean, simd::ema 等滑动窗口运算
  - [x] simd::rsi, simd::macd, simd::bollinger 指标计算
- [x] 多线程优化
  - [x] ThreadPool 线程池
  - [x] parallelFor/parallelForEach 并行算法
  - [x] ParameterGrid 参数网格生成
- [x] 策略优化系统
  - [x] Optimizer 策略优化器
  - [x] OptResult 优化结果
  - [x] OptResultAnalyzer 结果分析 (参数敏感性, 统计摘要)
  - [x] Cerebro.optStrategy 优化策略配置
  - [x] Cerebro.runOptimize 并行优化执行
- [x] 向量化指标计算
  - [x] IVectorized 接口
  - [x] VectorMath 向量数学运算
  - [x] Indicator.once() 批量计算
  - [x] SMA/EMA/RSI/MACD SIMD 优化

**阶段 5: Python 功能对齐** ✅ 完成

- [x] Sizer 系统
  - [x] FixedSizer/FixedReverser 固定手数
  - [x] PercentSizer/PercentSizerInt 资金百分比
  - [x] AllInSizer/AllInSizerInt 全仓
  - [x] PercentReverser 百分比反转
  - [x] RiskSizer 风险百分比
  - [x] KellySizer 凯利公式
- [x] CommInfo 佣金系统
  - [x] CommInfoStock/Futures/Forex/Options 各类型佣金
  - [x] margin/automargin 保证金计算
  - [x] interest/leverage 利息/杠杆
  - [x] profitAndLoss 盈亏计算
- [x] Filter 数据过滤器
  - [x] SessionFilter/SessionFiller 会话过滤
  - [x] RenkoFilter/HeikinAshiFilter 技术过滤
  - [x] CalendarDaysFilter/DayStepsFilter 日历过滤
  - [x] VolumeFilter/PriceFilter 量价过滤
  - [x] FilterChain 过滤器链
- [x] Timer 定时器系统
  - [x] Timer/TimerManager 定时器管理
  - [x] schedule:: 辅助函数
  - [x] weekdays/monthdays 时间过滤
- [x] Writer 写入器系统
  - [x] WriterFile/CSVWriter 文件写入
  - [x] TradeWriter/EquityWriter/OrderWriter 专用写入器
  - [x] SummaryWriter/MultiWriter 摘要/多重写入
- [x] Strategy 增强
  - [x] orderTargetSize/Value/Percent 目标持仓
  - [x] prenextOpen/nextstartOpen/nextOpen Cheat-on-open
  - [x] notifyFund/notifyStore/notifyTimer 通知
  - [x] addTimer 定时器
- [x] Broker 增强
  - [x] SlippageConfig 滑点配置
  - [x] VolumeFiller 成交量填充
  - [x] COC/COO Cheat 模式
  - [x] FundMode 基金模式
- [x] Cerebro 增强
  - [x] addSizer/addSizerByIdx 头寸管理
  - [x] addWriter 写入器
  - [x] addFilter 过滤器
  - [x] addTimer 定时器

## 构建要求

- CMake 3.16+
- C++17 兼容编译器
  - MSVC 2019+ (Windows)
  - GCC 8+ (Linux)
  - Clang 8+ (macOS)
- Python 3.8+ (可选，用于 Python 绑定)

## 快速开始(建议手动构建)

### Windows (PowerShell)

```powershell
cd cpp
.\build.ps1
```

### Linux/macOS

```bash
cd cpp
chmod +x build.sh
./build.sh
```

### 手动构建

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
ctest --output-on-failure
```

## 项目结构

```
cpp/
├── include/bt/           # 头文件
│   ├── common.hpp        # 通用定义
│   ├── linebuffer.hpp    # 时间序列缓冲区
│   ├── lineseries.hpp    # 多线容器
│   ├── params.hpp        # 参数系统
│   ├── indicator.hpp     # 指标基类
│   ├── strategy.hpp      # 策略框架
│   ├── broker.hpp        # 经纪商系统
│   ├── cerebro.hpp       # 回测引擎
│   ├── datafeed.hpp      # 数据源
│   ├── order.hpp         # 订单系统
│   ├── analyzer.hpp      # 分析器
│   ├── observer.hpp      # 观察器
│   ├── signal.hpp        # 信号系统
│   ├── sizer.hpp         # 头寸管理 (Phase 5)
│   ├── comminfo.hpp      # 佣金系统 (Phase 5)
│   ├── filter.hpp        # 数据过滤 (Phase 5)
│   ├── timer.hpp         # 定时器 (Phase 5)
│   ├── writer.hpp        # 写入器 (Phase 5)
│   ├── optimizer.hpp     # 策略优化
│   ├── threadpool.hpp    # 线程池
│   ├── simd.hpp          # SIMD 计算
│   ├── vectorized.hpp    # 向量化计算
│   ├── backtrader.hpp    # 主头文件
│   └── indicators/       # 指标实现
│       ├── sma.hpp
│       ├── ema.hpp
│       ├── macd.hpp
│       ├── rsi.hpp
│       └── bollinger.hpp
├── src/                  # 源文件
├── tests/                # 单元测试
├── examples/             # 示例代码
├── python/               # Python 绑定
└── CMakeLists.txt        # 构建配置
```

## 使用示例

### C++

```cpp
#include "bt/backtrader.hpp"

int main() {
    // 创建价格数据
    bt::LineBuffer prices;
    prices.extend({100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110});
    
    // 创建 SMA 指标
    bt::SMA sma(&prices, 5);
    sma.init();
    sma.precompute();
    
    // 获取结果
    std::cout << "SMA value: " << sma.value(0) << std::endl;
    
    return 0;
}
```

### Python (需要构建 Python 绑定)

```python
from backtrader_cpp import LineBuffer, SMA

# 创建数据
prices = LineBuffer()
for p in [100, 101, 102, 103, 104, 105]:
    prices.push(p)

# 创建指标
sma = SMA(prices, 3)
sma.init()
sma.precompute()

print(f"SMA value: {sma.value(0)}")
```

## API 对比

| Python (原版) | C++ (新版) |
|--------------|-----------|
| `self.data.close[0]` | `data.close()[0]` |
| `bt.indicators.SMA(period=20)` | `bt::SMA(&data, 20)` |
| `params = (('period', 14),)` | `BT_PARAM(period, 14)` |

## 性能

初步基准测试显示，C++ 版本在指标计算上比 Python 版本快 5-10 倍：

| 数据量 | SMA(20) | EMA(20) | RSI(14) |
|-------|---------|---------|---------|
| 10K   | 0.1 ms  | 0.1 ms  | 0.2 ms  |
| 100K  | 0.8 ms  | 0.9 ms  | 1.5 ms  |
| 1M    | 8 ms    | 9 ms    | 15 ms   |

## 路线图

- **阶段 1**: 基础设施 (LineBuffer, LineSeries, Params, 指标) ✅
- **阶段 2**: 核心引擎 (Cerebro, Strategy, DataFeed, Broker) ✅
- **阶段 3**: 高级特性 (多时间周期, 完整订单系统, 信号系统) ✅
- **阶段 4**: 性能优化 (SIMD, 多线程, 向量化计算) ✅
- **阶段 5**: Python功能对齐 (Sizer, CommInfo, Filter, Timer, Writer) ✅

## 许可证

本项目遵循与 Python backtrader 相同的 GPLv3+ 许可证。
