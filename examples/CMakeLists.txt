# 简单 SMA 示例
add_executable(example_sma example_sma.cpp)
target_link_libraries(example_sma PRIVATE backtrader_core)

# 性能基准测试
add_executable(benchmark_indicators benchmark_indicators.cpp)
target_link_libraries(benchmark_indicators PRIVATE backtrader_core)

# SMA Crossover Strategy 完整回测示例
add_executable(sma_crossover_strategy sma_crossover_strategy.cpp)
target_link_libraries(sma_crossover_strategy PRIVATE backtrader_core)

# 行情抓取与存储示例 (需要 Networking)
add_executable(fetch_and_store fetch_and_store.cpp)
target_link_libraries(fetch_and_store PRIVATE backtrader_core)

# OKX Broker 实盘/模拟盘 示例
add_executable(okx_broker_example okx_broker_example.cpp)
target_link_libraries(okx_broker_example PRIVATE backtrader_core)
