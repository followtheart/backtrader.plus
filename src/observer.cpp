/**
 * @file observer.cpp
 * @brief Observer implementations
 */

#include "bt/observer.hpp"
#include "bt/broker.hpp"
#include "bt/strategy.hpp"
#include <cmath>

namespace bt {

// ==================== CashObserver ====================

void CashObserver::next() {
    if (broker_) {
        cash().push(broker_->getCash());
    } else {
        cash().push(0);
    }
}

// ==================== ValueObserver ====================

void ValueObserver::next() {
    if (broker_) {
        value().push(broker_->getValue());
    } else {
        value().push(0);
    }
}

// ==================== BrokerObserver ====================

void BrokerObserver::next() {
    if (broker_) {
        line(0).push(broker_->getCash());
        line(1).push(broker_->getValue());
    } else {
        line(0).push(0);
        line(1).push(0);
    }
}

// ==================== DrawDownObserver ====================

void DrawDownObserver::start() {
    maxValue_ = broker_ ? broker_->getValue() : 0;
}

void DrawDownObserver::next() {
    if (!broker_) {
        drawdown().push(0);
        maxdrawdown().push(0);
        return;
    }
    
    Value currentValue = broker_->getValue();
    
    // Update peak value
    maxValue_ = std::max(maxValue_, currentValue);
    
    // Calculate current drawdown percentage
    Value dd = 0;
    if (maxValue_ > 0) {
        dd = (maxValue_ - currentValue) / maxValue_ * 100.0;
    }
    
    // Get previous max drawdown (or 0 if first bar)
    Value prevMaxDD = maxdrawdown().length() > 0 ? maxdrawdown()[0] : 0;
    Value maxDD = std::max(prevMaxDD, dd);
    
    drawdown().push(dd);
    maxdrawdown().push(maxDD);
}

// ==================== BuySellObserver ====================

void BuySellObserver::next() {
    // Default: no signal
    Value buySignal = std::nan("");
    Value sellSignal = std::nan("");
    
    // Check pending orders that were executed
    for (auto* order : pendingOrders_) {
        if (order && order->status() == OrderStatus::Completed) {
            if (order->isBuy()) {
                buySignal = order->executedPrice();
            } else {
                sellSignal = order->executedPrice();
            }
        }
    }
    pendingOrders_.clear();
    
    buy().push(buySignal);
    sell().push(sellSignal);
}

void BuySellObserver::notifyOrder(Order& order) {
    if (order.status() == OrderStatus::Completed) {
        pendingOrders_.push_back(&order);
    }
}

// ==================== TradesObserver ====================

void TradesObserver::next() {
    // Default: no trade
    Value pnlVal = std::nan("");
    Value pnlCommVal = std::nan("");
    
    // Check pending trades
    for (auto* trade : pendingTrades_) {
        if (trade && !trade->isOpen) {
            pnlVal = trade->pnl;
            pnlCommVal = trade->pnlComm;
        }
    }
    pendingTrades_.clear();
    
    pnl().push(pnlVal);
    pnlcomm().push(pnlCommVal);
}

void TradesObserver::notifyTrade(Trade& trade) {
    if (!trade.isOpen) {
        pendingTrades_.push_back(&trade);
    }
}

// ==================== ReturnsObserver ====================

void ReturnsObserver::start() {
    prevValue_ = broker_ ? broker_->getValue() : 0;
}

void ReturnsObserver::next() {
    if (!broker_) {
        returns().push(0);
        return;
    }
    
    Value currentValue = broker_->getValue();
    Value ret = 0;
    
    if (prevValue_ > 0) {
        ret = (currentValue - prevValue_) / prevValue_;
    }
    
    returns().push(ret);
    prevValue_ = currentValue;
}

// ==================== LogReturnsObserver ====================

void LogReturnsObserver::start() {
    prevValue_ = broker_ ? broker_->getValue() : 0;
}

void LogReturnsObserver::next() {
    if (!broker_) {
        logreturns().push(0);
        return;
    }
    
    Value currentValue = broker_->getValue();
    Value logRet = 0;
    
    if (prevValue_ > 0 && currentValue > 0) {
        logRet = std::log(currentValue / prevValue_);
    }
    
    logreturns().push(logRet);
    prevValue_ = currentValue;
}

} // namespace bt
