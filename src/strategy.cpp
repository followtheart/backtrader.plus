/**
 * @file strategy.cpp
 * @brief Strategy implementation
 */

#include "bt/strategy.hpp"
#include "bt/broker.hpp"
#include "bt/datafeed.hpp"
#include <algorithm>

namespace bt {

Order* Strategy::buy(DataFeed* data, Size size, Value price, OrderType exectype) {
    if (!broker_) return nullptr;
    
    DataFeed* d = data ? data : (datas_.empty() ? nullptr : datas_[0]);
    if (!d) return nullptr;
    
    // Determine size
    Size orderSize = size > 0 ? size : getSizing(d, true);
    if (orderSize == 0) return nullptr;
    
    // Get data name for broker
    std::string dataName = d->name();
    
    return broker_->buy(dataName, orderSize, price, exectype);
}

Order* Strategy::sell(DataFeed* data, Size size, Value price, OrderType exectype) {
    if (!broker_) return nullptr;
    
    DataFeed* d = data ? data : (datas_.empty() ? nullptr : datas_[0]);
    if (!d) return nullptr;
    
    // Determine size
    Size orderSize = size > 0 ? size : getSizing(d, false);
    if (orderSize == 0) return nullptr;
    
    // Get data name for broker
    std::string dataName = d->name();
    
    return broker_->sell(dataName, orderSize, price, exectype);
}

Order* Strategy::closePosition(DataFeed* data, Size size) {
    if (!broker_) return nullptr;
    
    DataFeed* d = data ? data : (datas_.empty() ? nullptr : datas_[0]);
    if (!d) return nullptr;
    
    std::string dataName = d->name();
    
    // Get current position
    Value posSize = broker_->getPosition(dataName);
    
    if (posSize == 0) return nullptr;
    
    // Calculate close size
    Size closeSize = size > 0 ? size : static_cast<Size>(std::abs(posSize));
    
    // Close in opposite direction
    if (posSize > 0) {
        return broker_->sell(dataName, closeSize, 0, OrderType::Market);
    } else {
        return broker_->buy(dataName, closeSize, 0, OrderType::Market);
    }
}

void Strategy::cancel(Order* order) {
    if (broker_ && order) {
        broker_->cancel(order->id());
    }
}

std::array<Order*, 3> Strategy::buyBracket(DataFeed* data, const BracketConfig& config) {
    std::array<Order*, 3> result = {nullptr, nullptr, nullptr};
    
    if (!broker_) return result;
    
    DataFeed* d = data ? data : (datas_.empty() ? nullptr : datas_[0]);
    if (!d) return result;
    
    std::string dataName = d->name();
    
    // Determine size
    Size orderSize = config.size > 0 ? static_cast<Size>(config.size) : getSizing(d, true);
    if (orderSize == 0) return result;
    
    // Create main buy order
    Order* mainOrder = nullptr;
    if (config.exectype == OrderType::Market) {
        mainOrder = broker_->buy(dataName, orderSize, 0, OrderType::Market);
    } else {
        mainOrder = broker_->buy(dataName, orderSize, config.price, config.exectype);
    }
    
    if (!mainOrder) return result;
    
    mainOrder->setTradeId(config.tradeId);
    mainOrder->setTransmit(config.stopPrice == 0 && config.limitPrice == 0);
    if (config.valid > 0) mainOrder->setValidUntil(config.valid);
    
    result[0] = mainOrder;
    
    // Create stop loss order (low side - sell)
    if (config.stopPrice > 0 || config.trailAmount > 0 || config.trailPercent > 0) {
        Order* stopOrder = nullptr;
        
        if (config.trailAmount > 0 || config.trailPercent > 0) {
            // Trailing stop
            stopOrder = broker_->sell(dataName, orderSize, config.stopPrice, OrderType::StopTrail);
            if (stopOrder) {
                stopOrder->setTrailAmount(config.trailAmount);
                stopOrder->setTrailPercent(config.trailPercent);
            }
        } else {
            // Regular stop
            stopOrder = broker_->sell(dataName, orderSize, config.stopPrice, config.stopExec);
        }
        
        if (stopOrder) {
            stopOrder->setParent(mainOrder);
            stopOrder->setTradeId(config.tradeId);
            stopOrder->setTransmit(config.limitPrice == 0);
            if (config.valid > 0) stopOrder->setValidUntil(config.valid);
            mainOrder->addChild(stopOrder);
            result[1] = stopOrder;
        }
    }
    
    // Create take profit order (high side - sell)
    if (config.limitPrice > 0) {
        Order* limitOrder = broker_->sell(dataName, orderSize, config.limitPrice, config.limitExec);
        
        if (limitOrder) {
            limitOrder->setParent(mainOrder);
            limitOrder->setTradeId(config.tradeId);
            limitOrder->setTransmit(true);  // Last order triggers transmission
            if (config.valid > 0) limitOrder->setValidUntil(config.valid);
            mainOrder->addChild(limitOrder);
            
            // Set up OCO between stop and limit
            if (result[1]) {
                result[1]->setOco(limitOrder);
                limitOrder->setOco(result[1]);
            }
            result[2] = limitOrder;
        }
    }
    
    return result;
}

std::array<Order*, 3> Strategy::sellBracket(DataFeed* data, const BracketConfig& config) {
    std::array<Order*, 3> result = {nullptr, nullptr, nullptr};
    
    if (!broker_) return result;
    
    DataFeed* d = data ? data : (datas_.empty() ? nullptr : datas_[0]);
    if (!d) return result;
    
    std::string dataName = d->name();
    
    // Determine size
    Size orderSize = config.size > 0 ? static_cast<Size>(config.size) : getSizing(d, false);
    if (orderSize == 0) return result;
    
    // Create main sell order
    Order* mainOrder = nullptr;
    if (config.exectype == OrderType::Market) {
        mainOrder = broker_->sell(dataName, orderSize, 0, OrderType::Market);
    } else {
        mainOrder = broker_->sell(dataName, orderSize, config.price, config.exectype);
    }
    
    if (!mainOrder) return result;
    
    mainOrder->setTradeId(config.tradeId);
    mainOrder->setTransmit(config.stopPrice == 0 && config.limitPrice == 0);
    if (config.valid > 0) mainOrder->setValidUntil(config.valid);
    
    result[0] = mainOrder;
    
    // Create stop loss order (high side - buy)
    if (config.stopPrice > 0 || config.trailAmount > 0 || config.trailPercent > 0) {
        Order* stopOrder = nullptr;
        
        if (config.trailAmount > 0 || config.trailPercent > 0) {
            // Trailing stop
            stopOrder = broker_->buy(dataName, orderSize, config.stopPrice, OrderType::StopTrail);
            if (stopOrder) {
                stopOrder->setTrailAmount(config.trailAmount);
                stopOrder->setTrailPercent(config.trailPercent);
            }
        } else {
            // Regular stop
            stopOrder = broker_->buy(dataName, orderSize, config.stopPrice, config.stopExec);
        }
        
        if (stopOrder) {
            stopOrder->setParent(mainOrder);
            stopOrder->setTradeId(config.tradeId);
            stopOrder->setTransmit(config.limitPrice == 0);
            if (config.valid > 0) stopOrder->setValidUntil(config.valid);
            mainOrder->addChild(stopOrder);
            result[1] = stopOrder;
        }
    }
    
    // Create take profit order (low side - buy)
    if (config.limitPrice > 0) {
        Order* limitOrder = broker_->buy(dataName, orderSize, config.limitPrice, config.limitExec);
        
        if (limitOrder) {
            limitOrder->setParent(mainOrder);
            limitOrder->setTradeId(config.tradeId);
            limitOrder->setTransmit(true);  // Last order triggers transmission
            if (config.valid > 0) limitOrder->setValidUntil(config.valid);
            mainOrder->addChild(limitOrder);
            
            // Set up OCO between stop and limit
            if (result[1]) {
                result[1]->setOco(limitOrder);
                limitOrder->setOco(result[1]);
            }
            result[2] = limitOrder;
        }
    }
    
    return result;
}

Value Strategy::getPosition(DataFeed* data) const {
    if (!broker_) return 0;
    
    DataFeed* d = data ? data : (datas_.empty() ? nullptr : datas_[0]);
    if (!d) return 0;
    
    return broker_->getPosition(d->name());
}

DataFeed* Strategy::getDataByName(const std::string& name) const {
    for (Size i = 0; i < datas_.size(); ++i) {
        if (dataNames_[i] == name) {
            return datas_[i];
        }
    }
    return nullptr;
}

} // namespace bt
