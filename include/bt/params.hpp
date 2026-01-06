/**
 * @file params.hpp
 * @brief 参数系统 - 对应 Python 的 metabase.py 中的 MetaParams
 * 
 * 使用 C++ 模板和宏模拟 Python 的参数元类系统：
 * - 支持默认值
 * - 支持参数继承
 * - 支持运行时参数修改
 */

#pragma once

#include "bt/common.hpp"
#include <string>
#include <unordered_map>
#include <any>
#include <optional>
#include <variant>
#include <stdexcept>
#include <type_traits>

namespace bt {

/**
 * @brief 参数值类型 - 支持常见类型
 */
using ParamValue = std::variant<
    bool,
    int,
    long,
    double,
    std::string,
    std::nullptr_t
>;

/**
 * @brief 参数存储容器
 */
class Params {
public:
    Params() = default;
    
    /**
     * @brief 设置参数
     */
    template<typename T>
    void set(const std::string& name, T value) {
        params_[name] = ParamValue(value);
    }
    
    /**
     * @brief 获取参数
     */
    template<typename T>
    T get(const std::string& name) const {
        auto it = params_.find(name);
        if (it == params_.end()) {
            throw std::runtime_error("Parameter not found: " + name);
        }
        return std::get<T>(it->second);
    }
    
    /**
     * @brief 获取参数（带默认值）
     */
    template<typename T>
    T get(const std::string& name, T defaultValue) const {
        auto it = params_.find(name);
        if (it == params_.end()) {
            return defaultValue;
        }
        try {
            return std::get<T>(it->second);
        } catch (...) {
            return defaultValue;
        }
    }
    
    /**
     * @brief 检查参数是否存在
     */
    bool has(const std::string& name) const {
        return params_.find(name) != params_.end();
    }
    
    /**
     * @brief 合并另一个参数集（用于继承）
     */
    void merge(const Params& other) {
        for (const auto& [key, value] : other.params_) {
            if (params_.find(key) == params_.end()) {
                params_[key] = value;
            }
        }
    }
    
    /**
     * @brief 覆盖参数
     */
    void override(const Params& other) {
        for (const auto& [key, value] : other.params_) {
            params_[key] = value;
        }
    }
    
    /**
     * @brief 获取所有参数名
     */
    std::vector<std::string> keys() const {
        std::vector<std::string> result;
        result.reserve(params_.size());
        for (const auto& [key, _] : params_) {
            result.push_back(key);
        }
        return result;
    }

private:
    std::unordered_map<std::string, ParamValue> params_;
};

/**
 * @brief 参数化基类 - 所有可配置对象的基类
 * 
 * 使用 CRTP 模式实现静态多态
 */
template<typename Derived>
class Parametrized {
public:
    using ParamsType = Params;
    
    Parametrized() {
        // 调用派生类的静态方法获取默认参数
        initDefaultParams();
    }
    
    explicit Parametrized(const Params& userParams) {
        initDefaultParams();
        params_.override(userParams);
    }
    
    /**
     * @brief 获取参数引用
     */
    Params& p() { return params_; }
    const Params& p() const { return params_; }
    
    // 别名（兼容 Python 风格）
    Params& params() { return params_; }
    const Params& params() const { return params_; }

protected:
    Params params_;
    
private:
    // SFINAE helper to detect getDefaultParams
    template<typename T, typename = void>
    struct has_default_params : std::false_type {};
    
    template<typename T>
    struct has_default_params<T, std::void_t<decltype(T::getDefaultParams())>> : std::true_type {};
    
    template<typename T = Derived>
    typename std::enable_if<has_default_params<T>::value>::type
    initDefaultParamsImpl() {
        params_ = T::getDefaultParams();
    }
    
    template<typename T = Derived>
    typename std::enable_if<!has_default_params<T>::value>::type
    initDefaultParamsImpl() {
        // No default params defined
    }
    
    void initDefaultParams() {
        initDefaultParamsImpl();
    }
};

/**
 * @brief 参数构建器 - 流式 API 设置参数
 */
class ParamsBuilder {
public:
    ParamsBuilder() = default;
    
    template<typename T>
    ParamsBuilder& add(const std::string& name, T value) {
        params_.set(name, value);
        return *this;
    }
    
    Params build() const { return params_; }
    
    operator Params() const { return params_; }

private:
    Params params_;
};

// 便捷宏：定义默认参数
#define BT_PARAMS_BEGIN() \
    static Params getDefaultParams() { \
        return ParamsBuilder()

#define BT_PARAM(name, value) \
        .add(#name, value)

#define BT_PARAMS_END() \
        .build(); \
    }

// 示例用法：
// class MyIndicator : public Parametrized<MyIndicator> {
// public:
//     BT_PARAMS_BEGIN()
//         BT_PARAM(period, 14)
//         BT_PARAM(devfactor, 2.0)
//     BT_PARAMS_END()
// };

} // namespace bt
