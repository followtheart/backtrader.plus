# 行情数据存储模块设计方案 (Market Data Storage Design)

## 1. 需求概述

本方案旨在设计一个高性能、可扩展的行情数据存储系统，集成于 Backtrader C++ 框架中。

**核心功能需求：**
1.  **多资产支持**: 兼容 Crypto (7x24小时)、股票 (交易时段)、期货 (定期交割) 等不同交易规则。
2.  **多周期支持**: 支持分钟线 (1m, 5m, 15m, 30m)、小时线 (1h, 4h)、日线等多种频率。
3.  **持久化存储**: 提供高效的数据写入与读取机制，支持百万级 Bar 数据的快速加载。
4.  **断点续传**: 能够查询最后存储的时间点，仅同步增量数据。
5.  **高性能回测**: 适配 Backtrader `DataFeed` 接口，支持快速加载至内存 `LineSeries`。

## 2. 核心架构设计

系统采用分层架构，将数据模型、存储引擎与应用适配层解耦。

```
[ Data Source (API) ]  <-- (Download) -- [ DataIngestor ]
                                              |
                                              v
[ Backtrader Engine ]  <-- (Load) ---- [ StorageFeed ]
         ^                                    |
         |                                    v
  [ bt::DataFeed ]                   [ DataStore Interface ]
                                              |
                              +---------------+---------------+
                              |                               |
                     [ SQLite (Metadata) ]           [ Binary/HDF5 (Raw Data) ]
```

### 2.1 数据模型 (Data Model)

为了解决现有 `bt::DateTime` (double) 在 Crypto 高频数据下可能存在的精度丢失问题，底层存储建议采用纳秒/毫秒级时间戳。

**BarData 结构建议:**
```cpp
struct StoredBar {
    int64_t timestamp;  // Unix Timestamp (milliseconds/nanoseconds)
    double open;
    double high;
    double low;
    double close;
    double volume;
    double open_interest;
};
```

**SymbolInfo 元数据:**
- **Symbol**: `BTC-USDT`
- **Exchange**: `Binance`
- **Type**: `Crypto`, `Stock`, `Future`
- **Timeframe**: `1m`, `1h`
- **LastUpdate**: `2023-10-01 12:00:00` (用于断点续传)

### 2.2 接口定义 (DataStore Interface)

定义抽象基类以屏蔽底层存储实现的差异。

```cpp
class DataStore {
public:
    virtual ~DataStore() = default;

    // 获取某品种最后已存储的时间戳 (用于断点续传)
    virtual int64_t getLastTimestamp(const std::string& symbol, Timeframe tf) = 0;

    // 写入 K 线数据 (追加模式)
    virtual void writeBars(const std::string& symbol, Timeframe tf, const std::vector<StoredBar>& bars) = 0;

    // 读取数据 (用于回测加载)
    virtual std::vector<StoredBar> readBars(const std::string& symbol, Timeframe tf, int64_t start, int64_t end) = 0;
};
```

## 3. 详细实施步骤

### 阶段一：基础架构与存储引擎
1.  **定义 `include/bt/data/`**: 创建新的命名空间用于数据处理。
2.  **实现 `SQLiteStore`**: 
    - 使用 SQLite 存储元数据以方便查询管理。
    - 对于海量 K 线数据，建议使用 **Blob** 存储或独立的 **二进制文件** (每品种每周期一个文件)，仅在 SQLite 中索引文件路径。
    - *优化方案*: 采用 SQLite 仅管理索引，数据体使用紧凑的二进制格式 (`Struct` 直接序列化) 写入磁盘，实现 I/O 吞吐最大化。

### 阶段二：数据同步工具 (DataIngestor)
1.  开发 `Fetcher` 模块（独立于回测引擎）。
2.  逻辑流程：
    - Input: `Symbol`, `Exchange`, `Timeframe`
    - Check: `store->getLastTimestamp(...)`
    - Download: 从 API 请求 `LastTimestamp` 之后的数据。
    - Write: `store->writeBars(...)`
    - Update: 更新元数据中的 `LastUpdate`。

### 阶段三：适配 Backtrader (StorageFeed)
1.  新建 `class StorageFeed : public bt::Feed`。
2.  实现 `load()` 方法：
    - 接收参数：`symbol`, `from_date`, `to_date`。
    - 调用 `store->readBars` 获取数据向量。
    - 遍历向量，将 `StoredBar` 转换为 `bt::Feed` 内部的 `Line` 数据，并推入 `LineBuffer`。
    - **注意**: 需将 `int64_t timestamp` 正确转换为 `bt::DateTime` (double) 以兼容现有系统的计算接口，尽管这可能会损失显示精度，但能保证回测逻辑兼容。

## 4. 技术选型建议

1.  **SQLite**: 
    - 优势: 单文件、无需服务器、生态极佳。
    - 用途: 存储 `Symbol` 列表、配置信息、数据段索引。

2.  **二进制小文件 (Flat File)**:
    - 优势: 读写极快 (Sequential R/W)，无数据库开销。
    - 用途: 存储实际的 OHLCV 数据数组。
    - 结构: `data/{exchange}/{symbol}/{timeframe}.bin`

3.  **时间处理**:
    - 存储层严格使用 UNIX Timestamp (int64)。
    - 只在进入 `bt::Engine` 计算层时转换为 `double`。

## 5. 项目结构规划

建议在现有工程中新增目录：

```
include/bt/data/
    store.hpp       // DataStore 接口
    sqlite_store.hpp
    types.hpp       // StoredBar 定义

src/data/
    sqlite_store.cpp
    storage_feed.cpp // 适配器实现
```
