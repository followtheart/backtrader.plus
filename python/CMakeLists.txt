# pybind11
include(FetchContent)
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v2.11.1
)
FetchContent_MakeAvailable(pybind11)

# Python 模块
pybind11_add_module(_backtrader_cpp
    bindings.cpp
)

target_link_libraries(_backtrader_cpp PRIVATE backtrader_core)

# 安装到 Python 包目录
install(TARGETS _backtrader_cpp DESTINATION .)
