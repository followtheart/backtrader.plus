/**
 * @file optimizer.hpp
 * @brief 策略参数优化系统 - 对应 Python 的 optstrategy
 * 
 * 实现功能：
 * - 参数网格搜索
 * - 多线程并行优化
 * - 优化进度回调
 * - 结果排序和筛选
 */

#pragma once

#include "bt/common.hpp"
#include "bt/params.hpp"
#include "bt/threadpool.hpp"
#include <vector>
#include <functional>
#include <chrono>
#include <algorithm>
#include <atomic>
#include <mutex>

namespace bt {

/**
 * @brief ParamValue 比较器
 * 
 * 自定义比较器，用于在 std::map 中使用 ParamValue 作为键。
 * 处理 std::variant 类型，包括 nullptr_t。
 */
struct ParamValueComparator {
    bool operator()(const ParamValue& a, const ParamValue& b) const {
        // 先按类型索引比较
        if (a.index() != b.index()) {
            return a.index() < b.index();
        }
        
        // 相同类型时，按值比较
        return std::visit([&b](const auto& valA) -> bool {
            using T = std::decay_t<decltype(valA)>;
            const auto& valB = std::get<T>(b);
            
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                // nullptr_t 都相等
                return false;
            } else {
                return valA < valB;
            }
        }, a);
    }
};

// 前向声明
class Cerebro;
class Strategy;
struct RunResult;

/**
 * @brief 优化结果
 */
struct OptResult {
    std::map<std::string, ParamValue> params;  // 参数组合
    Value finalValue = 0;                       // 最终资产价值
    Value pnl = 0;                              // 盈亏
    Value pnlPct = 0;                           // 盈亏百分比
    Value sharpeRatio = NaN;                    // 夏普比率
    Value maxDrawdown = NaN;                    // 最大回撤
    Size totalTrades = 0;                       // 总交易数
    Size winningTrades = 0;                     // 盈利交易数
    Value winRate = 0;                          // 胜率
    
    // 比较运算符（按 PnL% 排序）
    bool operator<(const OptResult& other) const {
        return pnlPct < other.pnlPct;
    }
    
    bool operator>(const OptResult& other) const {
        return pnlPct > other.pnlPct;
    }
};

/**
 * @brief 优化配置
 */
struct OptConfig {
    Size maxCpus = 0;               // 最大 CPU 数（0 = 自动检测）
    bool preload = true;            // 预加载数据
    bool optDatas = true;           // 优化时共享数据
    bool optReturn = true;          // 返回精简结果
    bool verbose = false;           // 详细输出
};

/**
 * @brief 优化排序标准
 */
enum class OptSortBy {
    PnlPct,         // 盈亏百分比
    PnlAbs,         // 绝对盈亏
    SharpeRatio,    // 夏普比率
    MaxDrawdown,    // 最大回撤
    WinRate,        // 胜率
    TotalTrades     // 交易次数
};

/**
 * @brief 策略优化器
 * 
 * 用于对策略参数进行网格搜索优化
 */
class Optimizer {
public:
    using ResultCallback = std::function<void(const OptResult&)>;
    using ProgressCallback = std::function<void(const OptimizationProgress&)>;
    
    /**
     * @brief 构造函数
     * @param config 优化配置
     */
    explicit Optimizer(const OptConfig& config = {})
        : config_(config) {
        
        Size cpus = config_.maxCpus;
        if (cpus == 0) {
            cpus = static_cast<Size>(std::thread::hardware_concurrency());
            if (cpus == 0) cpus = 1;
        }
        pool_ = std::make_unique<ThreadPool>(cpus);
    }
    
    /**
     * @brief 添加参数优化范围
     */
    void addParam(const std::string& name, const std::vector<ParamValue>& values) {
        grid_.addParam(name, values);
    }
    
    void addParam(const std::string& name, Value start, Value end, Value step = 1.0) {
        grid_.addParam(name, start, end, step);
    }
    
    void addParamInt(const std::string& name, int start, int end, int step = 1) {
        grid_.addParamInt(name, start, end, step);
    }
    
    /**
     * @brief 设置结果回调（每完成一个参数组合时调用）
     */
    void setResultCallback(ResultCallback cb) {
        resultCallback_ = std::move(cb);
    }
    
    /**
     * @brief 设置进度回调
     */
    void setProgressCallback(ProgressCallback cb) {
        progressCallback_ = std::move(cb);
    }
    
    /**
     * @brief 执行优化
     * @tparam StrategyT 策略类型
     * @tparam DataFeedT 数据源类型
     * @param data 数据源
     * @param initialCash 初始资金
     * @return 优化结果列表
     */
    template<typename StrategyT, typename DataFeedT>
    std::vector<OptResult> optimize(std::shared_ptr<DataFeedT> data, Value initialCash);
    
    /**
     * @brief 获取优化结果
     */
    const std::vector<OptResult>& results() const { return results_; }
    
    /**
     * @brief 按指定标准排序结果
     * @param sortBy 排序标准
     * @param descending 是否降序
     */
    void sortResults(OptSortBy sortBy = OptSortBy::PnlPct, bool descending = true) {
        std::sort(results_.begin(), results_.end(), [sortBy, descending](const OptResult& a, const OptResult& b) {
            Value valA, valB;
            switch (sortBy) {
                case OptSortBy::PnlPct:
                    valA = a.pnlPct; valB = b.pnlPct; break;
                case OptSortBy::PnlAbs:
                    valA = a.pnl; valB = b.pnl; break;
                case OptSortBy::SharpeRatio:
                    valA = std::isnan(a.sharpeRatio) ? -1e10 : a.sharpeRatio;
                    valB = std::isnan(b.sharpeRatio) ? -1e10 : b.sharpeRatio;
                    break;
                case OptSortBy::MaxDrawdown:
                    valA = std::isnan(a.maxDrawdown) ? 1e10 : a.maxDrawdown;
                    valB = std::isnan(b.maxDrawdown) ? 1e10 : b.maxDrawdown;
                    // 回撤越小越好
                    return descending ? (valA < valB) : (valA > valB);
                case OptSortBy::WinRate:
                    valA = a.winRate; valB = b.winRate; break;
                case OptSortBy::TotalTrades:
                    valA = static_cast<Value>(a.totalTrades);
                    valB = static_cast<Value>(b.totalTrades);
                    break;
            }
            return descending ? (valA > valB) : (valA < valB);
        });
    }
    
    /**
     * @brief 获取最佳结果
     * @param topN 返回前 N 个结果
     */
    std::vector<OptResult> topResults(Size topN = 10) const {
        Size n = std::min(topN, results_.size());
        return std::vector<OptResult>(results_.begin(), results_.begin() + n);
    }
    
    /**
     * @brief 获取总参数组合数
     */
    Size totalCombinations() const {
        return grid_.totalCombinations();
    }
    
    /**
     * @brief 清空参数和结果
     */
    void clear() {
        grid_.clear();
        results_.clear();
    }

private:
    OptConfig config_;
    ParameterGrid grid_;
    std::vector<OptResult> results_;
    std::unique_ptr<ThreadPool> pool_;
    
    ResultCallback resultCallback_;
    ProgressCallback progressCallback_;
    
    std::mutex resultsMutex_;
};

/**
 * @brief 优化结果分析器
 */
class OptResultAnalyzer {
public:
    explicit OptResultAnalyzer(const std::vector<OptResult>& results)
        : results_(results) {}
    
    /**
     * @brief 计算参数敏感性
     * @param paramName 参数名
     * @return 参数值到平均 PnL% 的映射
     */
    std::map<ParamValue, Value, ParamValueComparator> parameterSensitivity(const std::string& paramName) const {
        std::map<ParamValue, std::vector<Value>, ParamValueComparator> grouped;
        
        for (const auto& result : results_) {
            auto it = result.params.find(paramName);
            if (it != result.params.end()) {
                grouped[it->second].push_back(result.pnlPct);
            }
        }
        
        std::map<ParamValue, Value, ParamValueComparator> sensitivity;
        for (const auto& [param, pnls] : grouped) {
            Value sum = 0;
            for (Value v : pnls) sum += v;
            sensitivity[param] = sum / static_cast<Value>(pnls.size());
        }
        
        return sensitivity;
    }
    
    /**
     * @brief 计算统计摘要
     */
    struct Summary {
        Size totalRuns = 0;
        Size profitableRuns = 0;
        Value avgPnlPct = 0;
        Value maxPnlPct = -1e10;
        Value minPnlPct = 1e10;
        Value stdPnlPct = 0;
        Value avgWinRate = 0;
        Value avgTrades = 0;
    };
    
    Summary summary() const {
        Summary s;
        s.totalRuns = results_.size();
        
        if (s.totalRuns == 0) return s;
        
        Value sumPnl = 0;
        Value sumWinRate = 0;
        Value sumTrades = 0;
        
        for (const auto& r : results_) {
            sumPnl += r.pnlPct;
            sumWinRate += r.winRate;
            sumTrades += static_cast<Value>(r.totalTrades);
            
            if (r.pnlPct > 0) ++s.profitableRuns;
            if (r.pnlPct > s.maxPnlPct) s.maxPnlPct = r.pnlPct;
            if (r.pnlPct < s.minPnlPct) s.minPnlPct = r.pnlPct;
        }
        
        s.avgPnlPct = sumPnl / static_cast<Value>(s.totalRuns);
        s.avgWinRate = sumWinRate / static_cast<Value>(s.totalRuns);
        s.avgTrades = sumTrades / static_cast<Value>(s.totalRuns);
        
        // 计算标准差
        Value sumSqDiff = 0;
        for (const auto& r : results_) {
            Value diff = r.pnlPct - s.avgPnlPct;
            sumSqDiff += diff * diff;
        }
        s.stdPnlPct = std::sqrt(sumSqDiff / static_cast<Value>(s.totalRuns));
        
        return s;
    }

private:
    const std::vector<OptResult>& results_;
};

} // namespace bt
