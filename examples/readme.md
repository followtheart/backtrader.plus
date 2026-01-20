# 1. 先下载数据
./fetch_and_store.exe BTC-USDT

# 2. 运行 SMA 策略回测
./backtest_with_stored_data.exe --symbol BTC-USDT --days 3 --strategy sma

# 3. 运行 RSI 策略回测  
./backtest_with_stored_data.exe --symbol BTC-USDT --days 3 --strategy rsi

# 查看帮助
./backtest_with_stored_data.exe --help