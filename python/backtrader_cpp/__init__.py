"""
Backtrader C++ Python Wrapper

提供对 C++ 核心库的 Python 接口
"""

try:
    from ._backtrader_cpp import *
except ImportError as e:
    import warnings
    warnings.warn(f"Could not import C++ extension: {e}. Using pure Python fallback.")

__version__ = "0.1.0"
