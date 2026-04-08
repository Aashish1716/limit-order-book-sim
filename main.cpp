
//Poisson Arrival Process
// changed: Replaced flat random event selection with proper Poisson processes for arrivals, cancellations, and executions.
// Each event type now has a rate (lambda) and inter-arrival times are exponentially distributed — standard in market microstructure.


#include <iostream>
#include <map>
#include <list>
#include <random>
#include <chrono>
#include <memory>

struct Order {
    using ID = int;
    enum Side { BUY, SELL };

    ID id;
    Side side;
    double price;
    int quantity;
    double sim_time; // simulation time (seconds) instead of wall clock

    Order(ID id, Side side, double price, int quantity, double t)
        : id(id), side(side), price(price), quantity(quantity), sim_time(t) {}
};

class OrderBook {
public:
    using OrderPtr = std::shared_ptr<Order>;

    void add(OrderPtr order) {
        if (order->side == Order::BUY)
            bids_[order->price].push_back(order);
        else
            asks_[order->price].push_back(order);
        all_orders_[order->id] = order;
    }

    bool cancel(int id) {
        auto it = all_orders_.find(id);
        if (it == all_orders_.end()) return false;
        auto& order = it->second;

        auto& book_side = (order->side == Order::BUY) ? bids_ : asks_;
        auto level_it = book_side.find(order->price);
        if (level_it != book_side.end()) {
            auto& lst = level_it->second;
            lst.remove_if([id](const OrderPtr& o) { return o->id == id; });
            if (lst.empty()) book_side.erase(level_it);
        }
        all_orders_.erase(it);
        return true;
    }

    // Returns a random live order ID (for realistic cancellation)
    int randomLiveOrderId(std::mt19937& gen) {
        if (all_orders_.empty()) return -1;
        int idx = std::uniform_int_distribution<>(0, (int)all_orders_.size()-1)(gen);
        auto it = all_orders_.begin();
        std::advance(it, idx);
        return it->first;
    }

    int match(Order::Side side, int quantity) {
        int remaining = quantity;
        if (side == Order::BUY) {
            while (remaining > 0 && !asks_.empty()) {
                auto best = asks_.begin();
                auto& level = best->second;
                while (remaining > 0 && !level.empty()) {
                    auto& o = level.front();
                    int trade = std::min(remaining, o->quantity);
                    o->quantity -= trade;
                    remaining -= trade;
                    if (o->quantity == 0) { all_orders_.erase(o->id); level.pop_front(); }
                }
                if (level.empty()) asks_.erase(best);
            }
        } else {
            while (remaining > 0 && !bids_.empty()) {
                auto best = std::prev(bids_.end());
                auto& level = best->second;
                while (remaining > 0 && !level.empty()) {
                    auto& o = level.front();
                    int trade = std::min(remaining, o->quantity);
                    o->quantity -= trade;
                    remaining -= trade;
                    if (o->quantity == 0) { all_orders_.erase(o->id); level.pop_front(); }
                }
                if (level.empty()) bids_.erase(best);
            }
        }
        return quantity - remaining;
    }

    void print() const {
        std::cout << "\n--- Order Book (top 5 levels) ---\n";
        std::cout << "Asks:\n";
        int cnt = 0;
        for (auto& [price, lst] : asks_) {
            if (cnt++ >= 5) break;
            int total = 0; for (auto& o : lst) total += o->quantity;
            std::cout << "  " << price << " : " << total << "\n";
        }
        std::cout << "Bids:\n";
        cnt = 0;
        for (auto it = bids_.rbegin(); it != bids_.rend(); ++it) {
            if (cnt++ >= 5) break;
            int total = 0; for (auto& o : it->second) total += o->quantity;
            std::cout << "  " << it->first << " : " << total << "\n";
        }
    }

    size_t liveOrderCount() const { return all_orders_.size(); }

private:
    std::map<double, std::list<OrderPtr>> bids_;
    std::map<double, std::list<OrderPtr>> asks_;
    std::map<int, OrderPtr> all_orders_; // fast lookup for cancellation
};

// Poisson process: next event time = current + Exp(1/lambda)
double nextArrival(double lambda, std::mt19937& gen) {
    std::exponential_distribution<> exp_dist(lambda);
    return exp_dist(gen);
}

int main() {
    std::cout << "=== LOB Simulation — Commit 1: Poisson Arrivals ===\n\n";

    OrderBook book;
    std::mt19937 gen(42);

    std::uniform_real_distribution<> priceDist(99.0, 101.0); // near mid-price
    std::uniform_int_distribution<>  qtyDist(1, 50);
    std::uniform_int_distribution<>  sideDist(0, 1);

    // Rates (events per second) — typical for a liquid stock
    const double lambda_arrival     = 10.0;  // limit orders arrive at 10/sec
    const double lambda_cancel      = 5.0;   // cancellations at 5/sec
    const double lambda_market      = 2.0;   // market orders at 2/sec

    const double SIM_DURATION = 60.0; // 60 seconds of simulated time

    double t_arrival = nextArrival(lambda_arrival, gen);
    double t_cancel  = nextArrival(lambda_cancel, gen);
    double t_market  = nextArrival(lambda_market, gen);

    int nextId = 1;
    int arrivals = 0, cancellations = 0, market_orders = 0;

    // Event-driven loop: always process the earliest event
    while (true) {
        double t_next = std::min({t_arrival, t_cancel, t_market});
        if (t_next > SIM_DURATION) break;

        if (t_next == t_arrival) {
            auto side  = sideDist(gen) == 0 ? Order::BUY : Order::SELL;
            double price = priceDist(gen);
            int qty = qtyDist(gen);
            book.add(std::make_shared<Order>(nextId++, side, price, qty, t_next));
            arrivals++;
            t_arrival += nextArrival(lambda_arrival, gen);

        } else if (t_next == t_cancel) {
            int id = book.randomLiveOrderId(gen);
            if (id != -1 && book.cancel(id)) cancellations++;
            t_cancel += nextArrival(lambda_cancel, gen);

        } else {
            auto side = sideDist(gen) == 0 ? Order::BUY : Order::SELL;
            int qty = qtyDist(gen);
            int exec = book.match(side, qty);
            if (exec > 0) market_orders++;
            t_market += nextArrival(lambda_market, gen);
        }
    }

    std::cout << "Simulation time: " << SIM_DURATION << "s\n";
    std::cout << "Limit order arrivals:  " << arrivals << "\n";
    std::cout << "Cancellations:         " << cancellations << "\n";
    std::cout << "Market order fills:    " << market_orders << "\n";
    std::cout << "Live orders remaining: " << book.liveOrderCount() << "\n";
    book.print();

    return 0;
}

