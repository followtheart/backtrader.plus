# 源文件
set(BACKTRADER_SOURCES
    # Core
    ${CMAKE_CURRENT_SOURCE_DIR}/core/linebuffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/lineseries.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/params.cpp
    # Indicators
    ${CMAKE_CURRENT_SOURCE_DIR}/indicators/sma.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/indicators/ema.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/indicators/operations.cpp
    # Engine (Phase 2)
    ${CMAKE_CURRENT_SOURCE_DIR}/datafeed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/broker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/strategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cerebro.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/analyzer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/observer.cpp
)

# 核心库
add_library(backtrader_core STATIC ${BACKTRADER_SOURCES})

target_include_directories(backtrader_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# 链接线程库
target_link_libraries(backtrader_core PUBLIC Threads::Threads)

# SIMD 支持
if(BT_ENABLE_SIMD)
    if(MSVC)
        target_compile_options(backtrader_core PRIVATE /arch:AVX2)
    else()
        target_compile_options(backtrader_core PRIVATE -mavx2 -mfma)
    endif()
    target_compile_definitions(backtrader_core PRIVATE BT_ENABLE_SIMD=1)
endif()

# 导出符号（用于动态库）
target_compile_definitions(backtrader_core PRIVATE BT_BUILDING_LIBRARY)
