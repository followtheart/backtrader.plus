# Backtrader C++ 构建脚本 (Windows PowerShell)

# 创建构建目录
$buildDir = "build"
if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

# 进入构建目录
Push-Location $buildDir

try {
    # 配置 CMake
    Write-Host "=== Configuring CMake ===" -ForegroundColor Cyan
    cmake .. -G "Visual Studio 17 2022" -A x64 `
        -DBT_BUILD_TESTS=ON `
        -DBT_BUILD_PYTHON=OFF `
        -DBT_BUILD_EXAMPLES=ON `
        -DBT_ENABLE_SIMD=ON

    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed"
    }

    # 构建
    Write-Host "`n=== Building ===" -ForegroundColor Cyan
    cmake --build . --config Release --parallel

    if ($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }

    # 运行测试
    Write-Host "`n=== Running Tests ===" -ForegroundColor Cyan
    ctest -C Release --output-on-failure

    # 运行示例
    Write-Host "`n=== Running Example ===" -ForegroundColor Cyan
    if (Test-Path "bin/Release/example_sma.exe") {
        & "bin/Release/example_sma.exe"
    }

    Write-Host "`n=== Build Complete ===" -ForegroundColor Green
}
catch {
    Write-Host "Error: $_" -ForegroundColor Red
    exit 1
}
finally {
    Pop-Location
}
