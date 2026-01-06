/**
 * @file analyzer.cpp
 * @brief Analyzer implementations
 */

#include "bt/analyzer.hpp"
#include "bt/broker.hpp"
#include "bt/strategy.hpp"
#include <cmath>
#include <numeric>
#include <algorithm>

namespace bt {

// Helper function for standard deviation
static Value stddev(const std::vector<Value>& data, bool sample = false) {
    if (data.size() < 2) return 0.0;
    
    Value mean = std::accumulate(data.begin(), data.end(), 0.0) / data.size();
    Value sq_sum = 0.0;
    for (Value v : data) {
        sq_sum += (v - mean) * (v - mean);
    }
    
    Size n = sample ? data.size() - 1 : data.size();
    return std::sqrt(sq_sum / n);
}

static Value average(const std::vector<Value>& data) {
    if (data.empty()) return 0.0;
    return std::accumulate(data.begin(), data.end(), 0.0) / data.size();
}

// ==================== TradeAnalyzer ====================

void TradeAnalyzer::start() {
    totalTrades_ = 0;
    wonTrades_ = 0;
    lostTrades_ = 0;
    grossProfit_ = 0;
    grossLoss_ = 0;
    currentStreak_ = 0;
    maxWinStreak_ = 0;
    maxLossStreak_ = 0;
}

void TradeAnalyzer::notifyTrade(Trade& trade) {
    if (trade.isOpen) return;  // Only count closed trades
    
    totalTrades_++;
    
    if (trade.pnlComm > 0) {
        wonTrades_++;
        grossProfit_ += trade.pnlComm;
        
        if (lastWasWin_) {
            currentStreak_++;
        } else {
            currentStreak_ = 1;
            lastWasWin_ = true;
        }
        maxWinStreak_ = std::max(maxWinStreak_, currentStreak_);
        
    } else if (trade.pnlComm < 0) {
        lostTrades_++;
        grossLoss_ += std::abs(trade.pnlComm);
        
        if (!lastWasWin_) {
            currentStreak_++;
        } else {
            currentStreak_ = 1;
            lastWasWin_ = false;
        }
        maxLossStreak_ = std::max(maxLossStreak_, currentStreak_);
    }
}

void TradeAnalyzer::stop() {
    analysis_["total_trades"] = static_cast<Value>(totalTrades_);
    analysis_["won_trades"] = static_cast<Value>(wonTrades_);
    analysis_["lost_trades"] = static_cast<Value>(lostTrades_);
    analysis_["gross_profit"] = grossProfit_;
    analysis_["gross_loss"] = grossLoss_;
    analysis_["net_profit"] = grossProfit_ - grossLoss_;
    
    if (totalTrades_ > 0) {
        analysis_["win_rate"] = static_cast<Value>(wonTrades_) / totalTrades_ * 100.0;
        analysis_["avg_trade"] = (grossProfit_ - grossLoss_) / totalTrades_;
    } else {
        analysis_["win_rate"] = 0.0;
        analysis_["avg_trade"] = 0.0;
    }
    
    if (wonTrades_ > 0) {
        analysis_["avg_win"] = grossProfit_ / wonTrades_;
    } else {
        analysis_["avg_win"] = 0.0;
    }
    
    if (lostTrades_ > 0) {
        analysis_["avg_loss"] = grossLoss_ / lostTrades_;
    } else {
        analysis_["avg_loss"] = 0.0;
    }
    
    if (grossLoss_ > 0) {
        analysis_["profit_factor"] = grossProfit_ / grossLoss_;
    } else {
        analysis_["profit_factor"] = grossProfit_ > 0 ? 999.99 : 0.0;
    }
    
    analysis_["max_win_streak"] = static_cast<Value>(maxWinStreak_);
    analysis_["max_loss_streak"] = static_cast<Value>(maxLossStreak_);
}

// ==================== ReturnsAnalyzer ====================

void ReturnsAnalyzer::start() {
    startValue_ = broker_ ? broker_->getValue() : 0;
    prevValue_ = startValue_;
    returns_.clear();
}

void ReturnsAnalyzer::next() {
    if (!broker_) return;
    
    Value currentValue = broker_->getValue();
    
    if (prevValue_ > 0) {
        Value ret = (currentValue - prevValue_) / prevValue_;
        returns_.push_back(ret);
    }
    
    prevValue_ = currentValue;
}

void ReturnsAnalyzer::stop() {
    if (!broker_) return;
    
    Value endValue = broker_->getValue();
    
    // Total return
    if (startValue_ > 0) {
        analysis_["total_return"] = (endValue - startValue_) / startValue_ * 100.0;
    } else {
        analysis_["total_return"] = 0.0;
    }
    
    // Average daily return
    if (!returns_.empty()) {
        analysis_["avg_return"] = average(returns_) * 100.0;
        analysis_["return_std"] = stddev(returns_) * 100.0;
    } else {
        analysis_["avg_return"] = 0.0;
        analysis_["return_std"] = 0.0;
    }
}

// ==================== SharpeRatio ====================

void SharpeRatio::start() {
    startValue_ = broker_ ? broker_->getValue() : 0;
    prevValue_ = startValue_;
    returns_.clear();
}

void SharpeRatio::next() {
    if (!broker_) return;
    
    Value currentValue = broker_->getValue();
    
    if (prevValue_ > 0) {
        Value ret = (currentValue - prevValue_) / prevValue_;
        returns_.push_back(ret);
    }
    
    prevValue_ = currentValue;
}

void SharpeRatio::stop() {
    if (returns_.size() < 2) {
        analysis_["sharpe_ratio"] = 0.0;
        return;
    }
    
    Value avgReturn = average(returns_);
    Value stdReturn = stddev(returns_, useSampleStdDev_);
    
    if (stdReturn == 0) {
        analysis_["sharpe_ratio"] = 0.0;
        return;
    }
    
    // Daily risk-free rate
    Value riskFree = p().get<double>("riskfreerate");
    int tradingDays = p().get<int>("tradingdays");
    Value dailyRiskFree = riskFree / tradingDays;
    
    // Sharpe ratio
    Value sharpe = (avgReturn - dailyRiskFree) / stdReturn;
    
    // Annualize if requested
    if (p().get<bool>("annualize")) {
        sharpe *= std::sqrt(static_cast<double>(tradingDays));
    }
    
    analysis_["sharpe_ratio"] = sharpe;
}

// ==================== DrawDown ====================

void DrawDown::start() {
    maxValue_ = broker_ ? broker_->getValue() : 0;
    currentDrawdown_ = 0;
    currentDrawdownPct_ = 0;
    maxDrawdown_ = 0;
    maxDrawdownPct_ = 0;
    drawdownLen_ = 0;
    maxDrawdownLen_ = 0;
}

void DrawDown::next() {
    if (!broker_) return;
    
    Value currentValue = broker_->getValue();
    
    // Update peak
    maxValue_ = std::max(maxValue_, currentValue);
    
    // Calculate drawdown
    currentDrawdown_ = maxValue_ - currentValue;
    currentDrawdownPct_ = maxValue_ > 0 ? currentDrawdown_ / maxValue_ : 0;
    
    // Update max drawdown
    maxDrawdown_ = std::max(maxDrawdown_, currentDrawdown_);
    maxDrawdownPct_ = std::max(maxDrawdownPct_, currentDrawdownPct_);
    
    // Track duration
    if (currentDrawdown_ > 0) {
        drawdownLen_++;
        maxDrawdownLen_ = std::max(maxDrawdownLen_, drawdownLen_);
    } else {
        drawdownLen_ = 0;
    }
}

void DrawDown::stop() {
    analysis_["drawdown"] = currentDrawdownPct_ * 100.0;
    analysis_["moneydown"] = currentDrawdown_;
    analysis_["len"] = static_cast<Value>(drawdownLen_);
    analysis_["max_drawdown"] = maxDrawdownPct_ * 100.0;
    analysis_["max_moneydown"] = maxDrawdown_;
    analysis_["max_len"] = static_cast<Value>(maxDrawdownLen_);
}

// ==================== AnnualReturn ====================

void AnnualReturn::start() {
    startValue_ = broker_ ? broker_->getValue() : 0;
    yearStartValue_ = startValue_;
    yearlyReturns_.clear();
    currentYear_ = 0;  // Will be set on first next()
}

void AnnualReturn::next() {
    // For simplicity, assume each bar is one day
    // In a real implementation, we'd track datetime
    // This is a simplified version
}

void AnnualReturn::stop() {
    if (!broker_ || startValue_ <= 0) {
        analysis_["annual_return"] = 0.0;
        return;
    }
    
    Value endValue = broker_->getValue();
    Value totalReturn = (endValue - startValue_) / startValue_;
    
    // Simple annualization (assuming we know the period)
    // In reality, this would use actual dates
    analysis_["total_return"] = totalReturn * 100.0;
}

// ==================== SQN ====================

void SQN::start() {
    tradePnLs_.clear();
}

void SQN::notifyTrade(Trade& trade) {
    if (!trade.isOpen) {
        tradePnLs_.push_back(trade.pnlComm);
    }
}

void SQN::stop() {
    if (tradePnLs_.size() < 2) {
        analysis_["sqn"] = 0.0;
        analysis_["trades"] = static_cast<Value>(tradePnLs_.size());
        return;
    }
    
    // SQN = sqrt(N) * (Average PnL / StdDev PnL)
    Value avgPnL = average(tradePnLs_);
    Value stdPnL = stddev(tradePnLs_, true);  // Sample std dev
    
    if (stdPnL == 0) {
        analysis_["sqn"] = 0.0;
    } else {
        analysis_["sqn"] = std::sqrt(static_cast<double>(tradePnLs_.size())) * avgPnL / stdPnL;
    }
    
    analysis_["trades"] = static_cast<Value>(tradePnLs_.size());
}

} // namespace bt
