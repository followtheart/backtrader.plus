# 简单 SMA 示例
add_executable(example_sma example_sma.cpp)
target_link_libraries(example_sma PRIVATE backtrader_core)

# 性能基准测试
add_executable(benchmark_indicators benchmark_indicators.cpp)
target_link_libraries(benchmark_indicators PRIVATE backtrader_core)

# SMA Crossover Strategy 完整回测示例
add_executable(sma_crossover_strategy sma_crossover_strategy.cpp)
target_link_libraries(sma_crossover_strategy PRIVATE backtrader_core)
