#!/bin/bash
# Backtrader C++ 构建脚本 (Linux/macOS)

set -e

BUILD_DIR="build"

# 创建构建目录
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "=== Configuring CMake ==="
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBT_BUILD_TESTS=ON \
    -DBT_BUILD_PYTHON=OFF \
    -DBT_BUILD_EXAMPLES=ON \
    -DBT_ENABLE_SIMD=ON

echo ""
echo "=== Building ==="
cmake --build . --parallel $(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo "=== Running Tests ==="
ctest --output-on-failure

echo ""
echo "=== Running Example ==="
if [ -f "bin/example_sma" ]; then
    ./bin/example_sma
fi

echo ""
echo "=== Build Complete ==="
