/**
 * @file bindings.cpp
 * @brief Python 绑定 (pybind11)
 * 
 * 将 C++ 核心库暴露给 Python
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/operators.h>

#include "bt/backtrader.hpp"

namespace py = pybind11;

PYBIND11_MODULE(_backtrader_cpp, m) {
    m.doc() = "Backtrader C++ Core Library";
    
    // 版本信息
    m.def("version", &bt::version, "Get library version");
    
    // ==================== LineBuffer ====================
    py::class_<bt::LineBuffer>(m, "LineBuffer")
        .def(py::init<>(), "Create unbounded buffer")
        .def(py::init<bt::Size>(), py::arg("qbuffer"), "Create fixed-size buffer")
        .def("__getitem__", [](const bt::LineBuffer& self, bt::Index idx) {
            return self[idx];
        })
        .def("__setitem__", [](bt::LineBuffer& self, bt::Index idx, bt::Value v) {
            self[idx] = v;
        })
        .def("push", &bt::LineBuffer::push, "Add a new value")
        .def("extend", &bt::LineBuffer::extend, "Add multiple values")
        .def("advance", &bt::LineBuffer::advance, "Move to next bar")
        .def("rewind", &bt::LineBuffer::rewind, "Move to previous bar")
        .def("home", &bt::LineBuffer::home, "Reset to start")
        .def("position", &bt::LineBuffer::position, "Get current position")
        .def("size", &bt::LineBuffer::size, "Get buffer size")
        .def("length", &bt::LineBuffer::length, "Get total data length")
        .def("minperiod", &bt::LineBuffer::minperiod, "Get minimum period")
        .def("set_minperiod", &bt::LineBuffer::setMinperiod, "Set minimum period")
        .def("current", &bt::LineBuffer::current, "Get current value")
        .def("ready", &bt::LineBuffer::ready, "Check if buffer is ready");
    
    // ==================== Params ====================
    py::class_<bt::Params>(m, "Params")
        .def(py::init<>())
        .def("set_int", [](bt::Params& self, const std::string& name, int val) {
            self.set<int>(name, val);
        })
        .def("set_double", [](bt::Params& self, const std::string& name, double val) {
            self.set<double>(name, val);
        })
        .def("set_string", [](bt::Params& self, const std::string& name, const std::string& val) {
            self.set<std::string>(name, val);
        })
        .def("set_bool", [](bt::Params& self, const std::string& name, bool val) {
            self.set<bool>(name, val);
        })
        .def("get_int", [](const bt::Params& self, const std::string& name) {
            return self.get<int>(name);
        })
        .def("get_double", [](const bt::Params& self, const std::string& name) {
            return self.get<double>(name);
        })
        .def("get_string", [](const bt::Params& self, const std::string& name) {
            return self.get<std::string>(name);
        })
        .def("get_bool", [](const bt::Params& self, const std::string& name) {
            return self.get<bool>(name);
        })
        .def("has", &bt::Params::has)
        .def("keys", &bt::Params::keys);
    
    // ==================== LineSeries ====================
    py::class_<bt::LineSeries>(m, "LineSeries")
        .def(py::init<>())
        .def(py::init<bt::Size>(), py::arg("qbuffer"))
        .def("add_line", &bt::LineSeries::addLine)
        .def("line", py::overload_cast<bt::Size>(&bt::LineSeries::line), 
             py::return_value_policy::reference)
        .def("line_by_name", py::overload_cast<const std::string&>(&bt::LineSeries::line),
             py::return_value_policy::reference)
        .def("__getitem__", [](const bt::LineSeries& self, bt::Index idx) {
            return self[idx];
        })
        .def("num_lines", &bt::LineSeries::numLines)
        .def("has_line", &bt::LineSeries::hasLine)
        .def("advance", &bt::LineSeries::advance)
        .def("minperiod", &bt::LineSeries::minperiod)
        .def("ready", &bt::LineSeries::ready);
    
    // ==================== OHLCVData ====================
    py::class_<bt::OHLCVData, bt::LineSeries>(m, "OHLCVData")
        .def(py::init<>())
        .def("open", py::overload_cast<>(&bt::OHLCVData::open), 
             py::return_value_policy::reference)
        .def("high", py::overload_cast<>(&bt::OHLCVData::high),
             py::return_value_policy::reference)
        .def("low", py::overload_cast<>(&bt::OHLCVData::low),
             py::return_value_policy::reference)
        .def("close", py::overload_cast<>(&bt::OHLCVData::close),
             py::return_value_policy::reference)
        .def("volume", py::overload_cast<>(&bt::OHLCVData::volume),
             py::return_value_policy::reference)
        .def("add_bar", &bt::OHLCVData::addBar,
             py::arg("o"), py::arg("h"), py::arg("l"), py::arg("c"),
             py::arg("v") = 0.0, py::arg("oi") = 0.0);
    
    // ==================== Indicator Base ====================
    py::class_<bt::Indicator, bt::LineSeries>(m, "Indicator")
        .def("bind_data", py::overload_cast<bt::LineSeries*>(&bt::Indicator::bindData))
        .def("bind_line", py::overload_cast<bt::LineBuffer*>(&bt::Indicator::bindData))
        .def("init", &bt::Indicator::init)
        .def("next", &bt::Indicator::next)
        .def("precompute", &bt::Indicator::precompute);
    
    // ==================== SMA ====================
    py::class_<bt::indicators::SMA, bt::Indicator>(m, "SMA")
        .def(py::init<>())
        .def(py::init<const bt::Params&>())
        .def(py::init<bt::LineBuffer*, int>(), py::arg("input"), py::arg("period"))
        .def(py::init<bt::LineSeries*, int>(), py::arg("data"), py::arg("period"))
        .def("value", &bt::indicators::SMA::value, py::arg("idx") = 0);
    
    // ==================== EMA ====================
    py::class_<bt::indicators::EMA, bt::Indicator>(m, "EMA")
        .def(py::init<>())
        .def(py::init<const bt::Params&>())
        .def(py::init<bt::LineBuffer*, int>(), py::arg("input"), py::arg("period"))
        .def(py::init<bt::LineSeries*, int>(), py::arg("data"), py::arg("period"))
        .def("value", &bt::indicators::EMA::value, py::arg("idx") = 0);
    
    // ==================== MACD ====================
    py::class_<bt::indicators::MACD, bt::Indicator>(m, "MACD")
        .def(py::init<>())
        .def(py::init<const bt::Params&>())
        .def(py::init<bt::LineBuffer*, int, int, int>(),
             py::arg("input"), py::arg("fast") = 12, py::arg("slow") = 26, py::arg("signal") = 9)
        .def("macd", py::overload_cast<>(&bt::indicators::MACD::macd),
             py::return_value_policy::reference)
        .def("signal", py::overload_cast<>(&bt::indicators::MACD::signal),
             py::return_value_policy::reference)
        .def("histogram", py::overload_cast<>(&bt::indicators::MACD::histogram),
             py::return_value_policy::reference);
    
    // ==================== RSI ====================
    py::class_<bt::indicators::RSI, bt::Indicator>(m, "RSI")
        .def(py::init<>())
        .def(py::init<const bt::Params&>())
        .def(py::init<bt::LineBuffer*, int>(), py::arg("input"), py::arg("period") = 14)
        .def("value", &bt::indicators::RSI::value, py::arg("idx") = 0)
        .def("is_overbought", &bt::indicators::RSI::isOverbought, py::arg("idx") = 0)
        .def("is_oversold", &bt::indicators::RSI::isOversold, py::arg("idx") = 0);
    
    // ==================== BollingerBands ====================
    py::class_<bt::indicators::BollingerBands, bt::Indicator>(m, "BollingerBands")
        .def(py::init<>())
        .def(py::init<const bt::Params&>())
        .def(py::init<bt::LineBuffer*, int, double>(),
             py::arg("input"), py::arg("period") = 20, py::arg("devfactor") = 2.0)
        .def("mid", py::overload_cast<>(&bt::indicators::BollingerBands::mid),
             py::return_value_policy::reference)
        .def("top", py::overload_cast<>(&bt::indicators::BollingerBands::top),
             py::return_value_policy::reference)
        .def("bot", py::overload_cast<>(&bt::indicators::BollingerBands::bot),
             py::return_value_policy::reference)
        .def("percent_b", &bt::indicators::BollingerBands::percentB,
             py::arg("price"), py::arg("idx") = 0)
        .def("bandwidth", &bt::indicators::BollingerBands::bandwidth, py::arg("idx") = 0);
    
    // ==================== StdDev ====================
    py::class_<bt::indicators::StdDev, bt::Indicator>(m, "StdDev")
        .def(py::init<>())
        .def(py::init<const bt::Params&>())
        .def(py::init<bt::LineBuffer*, int>(), py::arg("input"), py::arg("period"))
        .def("value", &bt::indicators::StdDev::value, py::arg("idx") = 0);
}
