# Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# Windows 上避免覆盖父项目的编译器/链接器设置
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# 测试文件
set(TEST_SOURCES
    test_linebuffer.cpp
    test_lineseries.cpp
    test_params.cpp
    test_indicators.cpp
    test_cerebro.cpp
    test_phase3.cpp
    test_phase4.cpp
)

add_executable(backtrader_tests ${TEST_SOURCES})

target_link_libraries(backtrader_tests
    PRIVATE
        backtrader_core
        GTest::gtest_main
        Threads::Threads
)

include(GoogleTest)
gtest_discover_tests(backtrader_tests)

