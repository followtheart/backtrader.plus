cmake_minimum_required(VERSION 3.16)
project(backtrader_cpp VERSION 0.4.0 LANGUAGES CXX)

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
if(MSVC)
    add_compile_options(/W4 /WX- /permissive-)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    # SIMD 支持
    if(BT_ENABLE_SIMD)
        add_compile_options(/arch:AVX2)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    # SIMD 支持
    if(BT_ENABLE_SIMD)
        add_compile_options(-mavx2 -mfma)
    endif()
endif()

# 输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 选项
option(BT_BUILD_TESTS "Build unit tests" ON)
option(BT_BUILD_PYTHON "Build Python bindings" ON)
option(BT_BUILD_EXAMPLES "Build examples" ON)
option(BT_ENABLE_SIMD "Enable SIMD optimizations" ON)

# 线程支持
find_package(Threads REQUIRED)

# 头文件目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# 依赖库: SQLite3 (用于元数据存储)
include(FetchContent)

FetchContent_Declare(
    sqlite3
    GIT_REPOSITORY https://github.com/azadkuh/sqlite-amalgamation
    GIT_TAG master
)
FetchContent_MakeAvailable(sqlite3)

# 依赖库: nlohmann_json (用于 JSON 解析)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# 依赖库: CPR (用于 HTTP 请求)
# 注意: 在 Windows 上编译 cpr (及其依赖 curl/openssl) 可能需要较长时间和环境配置
# 如果遇到问题，建议预先安装 OpenSSL
option(BT_ENABLE_NETWORKING "Enable networking features (cpr)" ON)

if(BT_ENABLE_NETWORKING)
    FetchContent_Declare(
        cpr
        GIT_REPOSITORY https://github.com/libcpr/cpr.git
        GIT_TAG 1.10.4
    )
    FetchContent_MakeAvailable(cpr)
    add_compile_definitions(BT_ENABLE_NETWORKING)
endif()

# 核心库
add_subdirectory(src)

# 测试
if(BT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Python 绑定
if(BT_BUILD_PYTHON)
    add_subdirectory(python)
endif()

# 示例
if(BT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# 安装配置
install(DIRECTORY include/ DESTINATION include)
install(TARGETS backtrader_core
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# 打印配置信息
message(STATUS "=== Backtrader C++ Configuration ===")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Tests: ${BT_BUILD_TESTS}")
message(STATUS "Build Python Bindings: ${BT_BUILD_PYTHON}")
message(STATUS "Build Examples: ${BT_BUILD_EXAMPLES}")
message(STATUS "Enable SIMD: ${BT_ENABLE_SIMD}")
