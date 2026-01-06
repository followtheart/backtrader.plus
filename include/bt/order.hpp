/**
 * @file order.hpp
 * @brief Order System - Order types and execution
 * 
 * Corresponds to Python's order.py
 * Complete order system with execution bits, OCO, bracket orders, etc.
 */

#pragma once

#include "bt/common.hpp"
#include <string>
#include <chrono>
#include <optional>
#include <functional>
#include <cmath>
#include <vector>
#include <deque>

namespace bt {

// Forward declarations
class Strategy;
class Broker;
class Order;

/**
 * @brief Order execution type
 */
enum class OrderType {
    Market,         // Execute at market price
    Close,          // Execute at session close
    Limit,          // Execute at limit price or better
    Stop,           // Trigger at stop price, then market
    StopLimit,      // Trigger at stop price, then limit
    StopTrail,      // Trailing stop
    StopTrailLimit, // Trailing stop with limit
    Historical      // Historical order (backtesting only)
};

/**
 * @brief Order side
 */
enum class OrderSide {
    Buy,
    Sell
};

/**
 * @brief Order status
 */
enum class OrderStatus {
    Created,     // Order created but not submitted
    Submitted,   // Order submitted to broker
    Accepted,    // Order accepted by broker
    Partial,     // Partially filled
    Completed,   // Fully filled
    Canceled,    // Canceled by user
    Expired,     // Order expired
    Margin,      // Margin call
    Rejected     // Rejected by broker
};

/**
 * @brief Order execution bit - single execution event
 * 
 * Corresponds to Python's OrderExecutionBit
 * Stores details of a single (partial) execution of an order
 */
struct OrderExecutionBit {
    double dt = 0.0;           ///< DateTime of execution (as float)
    Value size = 0.0;          ///< Size executed in this event
    Value price = 0.0;         ///< Price of execution
    Value closed = 0.0;        ///< Contracts closed
    Value closedValue = 0.0;   ///< Value of closed contracts
    Value closedComm = 0.0;    ///< Commission for closed
    Value opened = 0.0;        ///< Contracts opened
    Value openedValue = 0.0;   ///< Value of opened contracts
    Value openedComm = 0.0;    ///< Commission for opened
    Value pnl = 0.0;           ///< Profit/loss
    Value psize = 0.0;         ///< Current position size after exec
    Value pprice = 0.0;        ///< Current position price after exec
    
    OrderExecutionBit() = default;
    
    OrderExecutionBit(double datetime, Value sz, Value pr,
                      Value cl, Value clVal, Value clComm,
                      Value op, Value opVal, Value opComm,
                      Value profit, Value posSize, Value posPrice)
        : dt(datetime), size(sz), price(pr)
        , closed(cl), closedValue(clVal), closedComm(clComm)
        , opened(op), openedValue(opVal), openedComm(opComm)
        , pnl(profit), psize(posSize), pprice(posPrice) {}
};

/**
 * @brief Order execution data - accumulated execution info
 * 
 * Corresponds to Python's OrderData
 * Contains all execution bits and aggregate execution data
 */
struct OrderData {
    double dt = 0.0;              ///< DateTime of last execution
    Value size = 0.0;             ///< Total size executed
    Value remsize = 0.0;          ///< Remaining size to execute
    Value price = 0.0;            ///< Average execution price
    Value value = 0.0;            ///< Total value
    Value margin = 0.0;           ///< Margin used
    Value pnl = 0.0;              ///< Total PnL
    Value psize = 0.0;            ///< Final position size
    Value pprice = 0.0;           ///< Final position price
    
    Value closed = 0.0;           ///< Total closed
    Value closedValue = 0.0;      ///< Total closed value
    Value closedComm = 0.0;       ///< Total closed commission
    Value opened = 0.0;           ///< Total opened
    Value openedValue = 0.0;      ///< Total opened value
    Value openedComm = 0.0;       ///< Total opened commission
    Value comm = 0.0;             ///< Total commission
    
    std::deque<OrderExecutionBit> exbits;  ///< Execution bits
    
    /**
     * @brief Add an execution bit
     */
    void add(double datetime, Value sz, Value pr,
             Value cl, Value clVal, Value clComm,
             Value op, Value opVal, Value opComm,
             Value profit, Value posSize, Value posPrice) {
        
        exbits.emplace_back(datetime, sz, pr, cl, clVal, clComm,
                           op, opVal, opComm, profit, posSize, posPrice);
        
        // Update aggregate values
        if (!std::isnan(sz) && sz != 0) {
            if (size == 0.0) {
                // First execution
                price = pr;
            } else {
                // Average price weighted by size
                price = (size * price + sz * pr) / (size + sz);
            }
            size += sz;
        }
        
        remsize = std::fabs(remsize - sz);
        dt = datetime;
        
        closed += cl;
        closedValue += clVal;
        closedComm += clComm;
        opened += op;
        openedValue += opVal;
        openedComm += opComm;
        comm = closedComm + openedComm;
        
        pnl += profit;
        psize = posSize;
        pprice = posPrice;
        
        value = size * price;
    }
    
    /**
     * @brief Create a clone for pending state
     */
    OrderData clone() const {
        OrderData result = *this;
        result.exbits.clear();
        return result;
    }
    
    /**
     * @brief Mark as pending (for bracket orders)
     */
    void markPending() {
        // Clear execution data but keep creation info
        size = 0;
        price = 0;
        value = 0;
        closed = 0;
        closedValue = 0;
        closedComm = 0;
        opened = 0;
        openedValue = 0;
        openedComm = 0;
        comm = 0;
        margin = 0;
        pnl = 0;
        psize = 0;
        pprice = 0;
        exbits.clear();
    }
};

/**
 * @brief Legacy order execution info (for simpler access)
 */
struct OrderExecInfo {
    Value price = 0.0;       // Execution price
    Value size = 0.0;        // Executed size
    Value value = 0.0;       // Total value (price * size)
    Value commission = 0.0;  // Commission paid
    Value pnl = 0.0;         // Profit/loss (for closing trades)
    Size barIndex = 0;       // Bar index when executed
};

/**
 * @brief Order class
 * 
 * Corresponds to Python's Order class in order.py
 * Supports market, limit, stop, stop-limit, trailing stop orders
 * with OCO (One-Cancels-Other) and bracket order support.
 */
class Order {
    friend class Broker;  // Allow Broker to access private members
    
public:
    // Unique order ID
    using OrderId = size_t;
    
    Order() = default;
    
    Order(OrderId id, OrderSide side, OrderType type, Value size)
        : id_(id)
        , side_(side)
        , type_(type)
        , size_(size)
        , status_(OrderStatus::Created)
    {
        created_.remsize = size;
    }
    
    // Static factory methods for creating orders
    static Order createMarket(Size size, Value price = 0.0) {
        Order order;
        order.type_ = OrderType::Market;
        order.side_ = size >= 0 ? OrderSide::Buy : OrderSide::Sell;
        order.size_ = std::fabs(static_cast<Value>(size));
        order.price_ = price;
        order.status_ = OrderStatus::Submitted;
        order.created_.remsize = order.size_;
        return order;
    }
    
    static Order createClose(Size size) {
        Order order;
        order.type_ = OrderType::Close;
        order.side_ = size >= 0 ? OrderSide::Buy : OrderSide::Sell;
        order.size_ = std::fabs(static_cast<Value>(size));
        order.status_ = OrderStatus::Submitted;
        order.created_.remsize = order.size_;
        return order;
    }
    
    static Order createLimit(int size, Value price) {
        Order order;
        order.type_ = OrderType::Limit;
        order.side_ = size >= 0 ? OrderSide::Buy : OrderSide::Sell;
        order.size_ = std::fabs(static_cast<Value>(size));
        order.price_ = price;
        order.status_ = OrderStatus::Submitted;
        order.created_.remsize = order.size_;
        return order;
    }
    
    static Order createStop(Size size, Value price) {
        Order order;
        order.type_ = OrderType::Stop;
        order.side_ = size >= 0 ? OrderSide::Buy : OrderSide::Sell;
        order.size_ = std::fabs(static_cast<Value>(size));
        order.stopPrice_ = price;
        order.status_ = OrderStatus::Submitted;
        order.created_.remsize = order.size_;
        return order;
    }
    
    static Order createStopLimit(Size size, Value price, Value stopPrice) {
        Order order;
        order.type_ = OrderType::StopLimit;
        order.side_ = size >= 0 ? OrderSide::Buy : OrderSide::Sell;
        order.size_ = std::fabs(static_cast<Value>(size));
        order.price_ = price;
        order.stopPrice_ = stopPrice;
        order.status_ = OrderStatus::Submitted;
        order.created_.remsize = order.size_;
        return order;
    }
    
    static Order createStopTrail(Size size, Value trailAmount, Value trailPercent = 0.0) {
        Order order;
        order.type_ = OrderType::StopTrail;
        order.side_ = size >= 0 ? OrderSide::Buy : OrderSide::Sell;
        order.size_ = std::fabs(static_cast<Value>(size));
        order.trailAmount_ = trailAmount;
        order.trailPercent_ = trailPercent;
        order.status_ = OrderStatus::Submitted;
        order.created_.remsize = order.size_;
        return order;
    }
    
    static Order createStopTrailLimit(Size size, Value price, Value trailAmount, Value trailPercent = 0.0) {
        Order order;
        order.type_ = OrderType::StopTrailLimit;
        order.side_ = size >= 0 ? OrderSide::Buy : OrderSide::Sell;
        order.size_ = std::fabs(static_cast<Value>(size));
        order.price_ = price;
        order.trailAmount_ = trailAmount;
        order.trailPercent_ = trailPercent;
        order.status_ = OrderStatus::Submitted;
        order.created_.remsize = order.size_;
        return order;
    }
    
    // Getters
    OrderId id() const { return id_; }
    Size ref() const { return ref_; }
    OrderSide side() const { return side_; }
    OrderType type() const { return type_; }
    OrderStatus status() const { return status_; }
    Value size() const { return size_; }
    Value price() const { return price_; }
    Value stopPrice() const { return stopPrice_; }
    Value limitPrice() const { return limitPrice_; }
    Value trailAmount() const { return trailAmount_; }
    Value trailPercent() const { return trailPercent_; }
    const std::string& data() const { return data_; }
    
    // Order data access
    const OrderData& created() const { return created_; }
    const OrderData& executed() const { return executed_; }
    OrderData& executed() { return executed_; }
    
    // Legacy execution info (for backward compatibility)
    const OrderExecInfo& execInfo() const { return execInfo_; }
    Value executedSize() const { return executed_.size; }
    Value executedPrice() const { return executed_.price; }
    Value executedValue() const { return executed_.value; }
    Value remainingSize() const { return executed_.remsize; }
    
    // OCO/Bracket order support
    Order* oco() const { return oco_; }
    Order* parent() const { return parent_; }
    const std::vector<Order*>& children() const { return children_; }
    bool isTransmit() const { return transmit_; }
    int tradeId() const { return tradeId_; }
    
    // Valid until
    double validUntil() const { return validUntil_; }
    bool hasValidUntil() const { return validUntil_ > 0; }
    
    // Status checks
    bool isAlive() const {
        return status_ == OrderStatus::Created ||
               status_ == OrderStatus::Submitted ||
               status_ == OrderStatus::Accepted ||
               status_ == OrderStatus::Partial;
    }
    
    bool isActive() const { return active_; }
    bool isBuy() const { return side_ == OrderSide::Buy; }
    bool isSell() const { return side_ == OrderSide::Sell; }
    bool isCompleted() const { return status_ == OrderStatus::Completed; }
    bool isCanceled() const { return status_ == OrderStatus::Canceled; }
    bool isExpired() const { return status_ == OrderStatus::Expired; }
    bool isRejected() const { return status_ == OrderStatus::Rejected; }
    
    // Status modifiers
    void submit(Broker* broker = nullptr) {
        status_ = OrderStatus::Submitted;
        broker_ = broker;
    }
    
    void accept(Broker* broker = nullptr) {
        status_ = OrderStatus::Accepted;
        if (broker) broker_ = broker;
    }
    
    bool reject(Broker* broker = nullptr) {
        if (status_ == OrderStatus::Rejected) return false;
        status_ = OrderStatus::Rejected;
        if (broker) broker_ = broker;
        return true;
    }
    
    void cancel() {
        status_ = OrderStatus::Canceled;
    }
    
    void margin() {
        status_ = OrderStatus::Margin;
    }
    
    void complete() {
        status_ = OrderStatus::Completed;
    }
    
    void partial() {
        status_ = OrderStatus::Partial;
    }
    
    bool expire(double currentDt = 0.0) {
        if (type_ == OrderType::Market) return false;
        if (hasValidUntil() && currentDt > validUntil_) {
            status_ = OrderStatus::Expired;
            executed_.dt = currentDt;
            return true;
        }
        return false;
    }
    
    void activate() { active_ = true; }
    void deactivate() { active_ = false; }
    
    // Execute order
    void execute(double dt, Value sz, Value pr,
                 Value closed, Value closedValue, Value closedComm,
                 Value opened, Value openedValue, Value openedComm,
                 Value marginUsed, Value pnl,
                 Value posSize, Value posPrice) {
        if (sz == 0) return;
        
        executed_.add(dt, sz, pr, closed, closedValue, closedComm,
                     opened, openedValue, openedComm, pnl, posSize, posPrice);
        executed_.margin = marginUsed;
        
        // Update legacy exec info
        execInfo_.price = executed_.price;
        execInfo_.size = executed_.size;
        execInfo_.value = executed_.value;
        execInfo_.commission = executed_.comm;
        execInfo_.pnl = executed_.pnl;
        
        // Update status
        if (executed_.remsize > 0) {
            status_ = OrderStatus::Partial;
        } else {
            status_ = OrderStatus::Completed;
        }
    }
    
    // Trail adjustment for trailing stops
    void trailAdjust(Value currentPrice) {
        Value pamount = 0.0;
        if (trailAmount_ > 0) {
            pamount = trailAmount_;
        } else if (trailPercent_ > 0) {
            pamount = currentPrice * trailPercent_;
        }
        
        if (pamount == 0.0) return;
        
        if (isBuy()) {
            Value newPrice = currentPrice + pamount;
            if (created_.price == 0 || newPrice < created_.price) {
                created_.price = newPrice;
                if (type_ == OrderType::StopTrailLimit) {
                    limitPrice_ = newPrice - limitOffset_;
                }
            }
        } else {
            Value newPrice = currentPrice - pamount;
            if (created_.price == 0 || newPrice > created_.price) {
                created_.price = newPrice;
                if (type_ == OrderType::StopTrailLimit) {
                    limitPrice_ = newPrice - limitOffset_;
                }
            }
        }
    }
    
    // Setters (used by Broker)
    void setStatus(OrderStatus status) { status_ = status; }
    void setSide(OrderSide s) { side_ = s; }
    void setPrice(Value p) { price_ = p; }
    void setStopPrice(Value p) { stopPrice_ = p; }
    void setLimitPrice(Value p) { limitPrice_ = p; }
    void setTrailAmount(Value a) { trailAmount_ = a; }
    void setTrailPercent(Value p) { trailPercent_ = p; }
    void setExecInfo(const OrderExecInfo& info) { execInfo_ = info; }
    void setDataIndex(Size idx) { dataIndex_ = idx; }
    void setRef(Size r) { ref_ = r; }
    void setData(const std::string& d) { data_ = d; }
    void setOco(Order* o) { oco_ = o; }
    void setParent(Order* p) { parent_ = p; }
    void addChild(Order* c) { children_.push_back(c); }
    void setTransmit(bool t) { transmit_ = t; }
    void setTradeId(int id) { tradeId_ = id; }
    void setValidUntil(double v) { validUntil_ = v; }
    void setLimitOffset(Value o) { limitOffset_ = o; }
    Size dataIndex() const { return dataIndex_; }

private:
    OrderId id_ = 0;
    Size ref_ = 0;                // Order reference number
    OrderSide side_ = OrderSide::Buy;
    OrderType type_ = OrderType::Market;
    OrderStatus status_ = OrderStatus::Created;
    
    Value size_ = 0.0;            // Order size
    Value price_ = 0.0;           // Limit/trigger price
    Value stopPrice_ = 0.0;       // Stop trigger price
    Value limitPrice_ = 0.0;      // Limit price for StopLimit
    Value trailAmount_ = 0.0;     // Trail amount
    Value trailPercent_ = 0.0;    // Trail percent
    Value limitOffset_ = 0.0;     // Offset for trail limit
    
    Size dataIndex_ = 0;          // Which data feed
    std::string data_;            // Data feed name
    
    bool active_ = true;          // Is order active
    bool transmit_ = true;        // Should transmit immediately
    
    double validUntil_ = 0.0;     // Valid until datetime (0 = GTC)
    int tradeId_ = 0;             // Trade ID for grouping
    
    // OCO and bracket order support
    Order* oco_ = nullptr;        // One-Cancels-Other order
    Order* parent_ = nullptr;     // Parent order (for bracket)
    std::vector<Order*> children_; // Child orders (for bracket)
    
    Broker* broker_ = nullptr;    // Associated broker
    
    OrderData created_;           // Creation data
    OrderData executed_;          // Execution data
    OrderExecInfo execInfo_;      // Legacy execution info
};

/**
 * @brief Position tracking
 */
class Position {
public:
    Position() = default;
    
    Value size() const { return size_; }
    Value price() const { return price_; }
    Value value() const { return size_ * price_; }
    
    void update(Value deltaSize, Value execPrice) {
        if (size_ == 0) {
            price_ = execPrice;
            size_ = deltaSize;
        } else if ((size_ > 0 && deltaSize > 0) || (size_ < 0 && deltaSize < 0)) {
            // Adding to position - calculate average price
            Value totalValue = size_ * price_ + deltaSize * execPrice;
            size_ += deltaSize;
            price_ = totalValue / size_;
        } else {
            // Reducing position
            size_ += deltaSize;
            if (std::abs(size_) < 1e-10) {
                size_ = 0;
                price_ = 0;
            }
        }
    }
    
    void close() {
        size_ = 0;
        price_ = 0;
    }
    
    bool isLong() const { return size_ > 0; }
    bool isShort() const { return size_ < 0; }
    bool isOpen() const { return size_ != 0; }

private:
    Value size_ = 0.0;
    Value price_ = 0.0;
};

/**
 * @brief Trade record
 */
struct Trade {
    Size ref = 0;                 // Trade reference number
    std::string dataName;         // Data feed name
    Size barOpen = 0;
    Size barClose = 0;
    Value priceOpen = 0.0;
    Value priceClose = 0.0;
    Value price = 0.0;            // Trade price
    Value size = 0.0;
    Value pnl = 0.0;
    Value pnlComm = 0.0;          // PnL after commission
    Value commission = 0.0;
    bool isLong = true;
    bool isOpen = true;
    
    void close(Size bar, Value closePrice, Value comm) {
        barClose = bar;
        priceClose = closePrice;
        commission += comm;
        isOpen = false;
        
        if (isLong) {
            pnl = (priceClose - priceOpen) * std::abs(size);
        } else {
            pnl = (priceOpen - priceClose) * std::abs(size);
        }
        pnlComm = pnl - commission;
    }
};

} // namespace bt
