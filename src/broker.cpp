/**
 * @file broker.cpp
 * @brief Broker implementation
 */

#include "bt/broker.hpp"
#include "bt/datafeed.hpp"
#include <algorithm>
#include <cmath>

namespace bt {

Value Broker::getValue() const {
    Value val = cash_;
    for (const auto& [name, pos] : positions_) {
        if (dataFeeds_.count(name) && pos.size != 0) {
            val += pos.size * dataFeeds_.at(name)->close()[0];
        }
    }
    return val;
}

Order* Broker::buy(const std::string& data, Size size, Value price, OrderType type) {
    auto order = std::make_unique<Order>();
    order->setRef(++orderId_);
    order->side_ = OrderSide::Buy;
    order->type_ = type;
    order->size_ = size;
    order->price_ = price;
    order->data_ = data;
    order->status_ = OrderStatus::Submitted;
    
    Order* ptr = order.get();
    orders_.push_back(std::move(order));
    return ptr;
}

Order* Broker::sell(const std::string& data, Size size, Value price, OrderType type) {
    auto order = std::make_unique<Order>();
    order->setRef(++orderId_);
    order->side_ = OrderSide::Sell;
    order->type_ = type;
    order->size_ = size;
    order->price_ = price;
    order->data_ = data;
    order->status_ = OrderStatus::Submitted;
    
    Order* ptr = order.get();
    orders_.push_back(std::move(order));
    return ptr;
}

void Broker::cancel(Size orderId) {
    for (auto& order : orders_) {
        if (order->ref() == orderId && order->isAlive()) {
            order->status_ = OrderStatus::Canceled;
            if (orderCb_) orderCb_(*order);
        }
    }
}

void Broker::next() {
    for (auto& order : orders_) {
        if (order->status() != OrderStatus::Submitted) continue;
        if (tryExecute(*order, false, false)) {
            if (orderCb_) orderCb_(*order);
        }
    }
}

bool Broker::tryExecute(Order& order, bool atOpen, bool atClose) {
    (void)atOpen; (void)atClose;  // For future use
    const auto& data = order.data();
    if (dataFeeds_.count(data) == 0) return false;
    
    DataFeed* feed = dataFeeds_.at(data);
    Value execPrice = 0;
    bool exec = false;
    
    switch (order.type()) {
        case OrderType::Market:
            execPrice = feed->open()[0];
            exec = true;
            break;
        case OrderType::Limit:
            if (order.isBuy() && feed->low()[0] <= order.price()) {
                execPrice = (feed->open()[0] < order.price()) ? feed->open()[0] : order.price();
                exec = true;
            } else if (!order.isBuy() && feed->high()[0] >= order.price()) {
                execPrice = (feed->open()[0] > order.price()) ? feed->open()[0] : order.price();
                exec = true;
            }
            break;
        default:
            break;
    }
    
    if (exec) {
        Size fillSize = static_cast<Size>(order.size());
        // Apply filler if available
        if (filler_) {
            fillSize = filler_->fill(order, execPrice, feed->volume()[0]);
        }
        // Apply slippage
        execPrice = applySlippage(execPrice, order.isBuy(), params_.slip);
        executeOrder(order, execPrice, fillSize);
        return true;
    }
    return false;
}

void Broker::executeOrder(Order& order, Value price, Size fillSize) {
    const auto& data = order.data();
    Size size = fillSize;
    
    // Commission
    Value comm = commInfo_ ? commInfo_->getCommission(size, price) : 0;
    
    // Update cash
    Value cost = price * size;
    if (order.isBuy()) {
        cash_ -= cost + comm;
    } else {
        cash_ += cost - comm;
    }
    
    // Update position
    auto& pos = positions_[data];
    Value oldSize = pos.size;
    
    if (order.isBuy()) {
        if (pos.size >= 0) {
            Value total = pos.price * pos.size + price * size;
            pos.size += size;
            pos.price = pos.size > 0 ? total / pos.size : 0;
        } else {
            pos.size += size;
            if (pos.size >= 0) pos.price = price;
        }
    } else {
        if (pos.size <= 0) {
            Value total = pos.price * std::abs(pos.size) + price * size;
            pos.size -= size;
            pos.price = pos.size < 0 ? total / std::abs(pos.size) : 0;
        } else {
            pos.size -= size;
            if (pos.size <= 0) pos.price = pos.size < 0 ? price : 0;
        }
    }
    
    // Trade record
    Trade trade;
    trade.ref = trades_.size() + 1;
    trade.dataName = data;
    trade.size = order.isBuy() ? size : -static_cast<Value>(size);
    trade.price = price;
    trade.commission = comm;
    
    bool wasFlat = (oldSize == 0);
    bool isFlat = (pos.size == 0);
    
    if (!wasFlat && isFlat) {
        trade.isOpen = false;
        trade.pnl = (price - pos.price) * std::abs(oldSize);
        if (oldSize < 0) trade.pnl = -trade.pnl;
        trade.pnlComm = trade.pnl - comm;
        if (tradeCb_) tradeCb_(trade);
    } else {
        trade.isOpen = true;
    }
    
    trades_.push_back(trade);
    
    order.status_ = OrderStatus::Completed;
    order.execInfo_.price = price;
    order.execInfo_.size = size;
    order.execInfo_.commission = comm;
}

} // namespace bt
