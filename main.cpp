#include <iostream>
#include <map>
#include <list>
#include <random>
#include <chrono>
#include <memory>

// ---------------------------------------------
// A simple order
// ---------------------------------------------
struct Order {
    using ID = int;
    enum Side { BUY, SELL };

    ID id;
    Side side;
    double price;
    int quantity;
    std::chrono::steady_clock::time_point timestamp;

    Order(ID id, Side side, double price, int quantity)
        : id(id), side(side), price(price), quantity(quantity), timestamp(std::chrono::steady_clock::now()) {}
};

// ---------------------------------------------
// A very basic order book
// ---------------------------------------------
class OrderBook {
public:
    using OrderPtr = std::shared_ptr<Order>;

    void add(OrderPtr order) {
        if (order->side == Order::BUY)
            bids_[order->price].push_back(order);
        else
            asks_[order->price].push_back(order);
    }

    // Cancel an order by ID (simple linear search, not efficient!)
    bool cancel(int id) {
        for (auto& [_, list] : bids_) {
            for (auto it = list.begin(); it != list.end(); ++it) {
                if ((*it)->id == id) {
                    list.erase(it);
                    if (list.empty()) bids_.erase(_);
                    return true;
                }
            }
        }
        for (auto& [_, list] : asks_) {
            for (auto it = list.begin(); it != list.end(); ++it) {
                if ((*it)->id == id) {
                    list.erase(it);
                    if (list.empty()) asks_.erase(_);
                    return true;
                }
            }
        }
        return false;
    }

    // Match a marketable order (simplest price-time priority)
    // Returns number of shares executed
    int match(Order::Side side, int quantity) {
        int remaining = quantity;
        if (side == Order::BUY) {
            // match against asks (lowest price first)
            while (remaining > 0 && !asks_.empty()) {
                auto best = asks_.begin();
                auto& level = best->second;
                while (remaining > 0 && !level.empty()) {
                    auto& order = level.front();
                    int trade = std::min(remaining, order->quantity);
                    order->quantity -= trade;
                    remaining -= trade;
                    if (order->quantity == 0)
                        level.pop_front();
                }
                if (level.empty())
                    asks_.erase(best);
            }
        } else {
            // match against bids (highest price first)
            while (remaining > 0 && !bids_.empty()) {
                auto best = std::prev(bids_.end());  // highest price
                auto& level = best->second;
                while (remaining > 0 && !level.empty()) {
                    auto& order = level.front();
                    int trade = std::min(remaining, order->quantity);
                    order->quantity -= trade;
                    remaining -= trade;
                    if (order->quantity == 0)
                        level.pop_front();
                }
                if (level.empty())
                    bids_.erase(best);
            }
        }
        return quantity - remaining;  // executed shares
    }

    void print() {
        std::cout << "\n--- Order Book ---\n";
        std::cout << "Bids (price : quantity)\n";
        for (auto it = bids_.rbegin(); it != bids_.rend(); ++it) {
            int total = 0;
            for (auto& o : it->second) total += o->quantity;
            std::cout << it->first << " : " << total << "\n";
        }
        std::cout << "Asks (price : quantity)\n";
        for (auto& [price, list] : asks_) {
            int total = 0;
            for (auto& o : list) total += o->quantity;
            std::cout << price << " : " << total << "\n";
        }
    }

private:
    std::map<double, std::list<OrderPtr>> bids_;  // price -> orders (ascending)
    std::map<double, std::list<OrderPtr>> asks_;  // price -> orders (ascending)
};

// ---------------------------------------------
// Main simulation loop (very basic)
// ---------------------------------------------
int main() {
    std::cout << "=== Limit Order Book Simulation (Ongoing) ===\n";

    OrderBook book;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> priceDist(100.0, 200.0);
    std::uniform_int_distribution<> qtyDist(1, 100);
    std::uniform_int_distribution<> sideDist(0, 1);
    std::uniform_real_distribution<> eventDist(0.0, 1.0);

    int nextId = 1;
    int numEvents = 1000;
    int arrivals = 0, cancellations = 0, matches = 0;

    for (int i = 0; i < numEvents; ++i) {
        double r = eventDist(gen);

        // Simple event selection (not based on real rates)
        if (r < 0.6) {  // 60% new order
            auto side = sideDist(gen) == 0 ? Order::BUY : Order::SELL;
            double price = priceDist(gen);
            int qty = qtyDist(gen);
            auto order = std::make_shared<Order>(nextId++, side, price, qty);
            book.add(order);
            arrivals++;
        }
        else if (r < 0.8) {  // 20% cancellation
            // pick a random ID to cancel (simplistic)
            if (nextId > 1) {
                int id = std::uniform_int_distribution<>(1, nextId-1)(gen);
                if (book.cancel(id))
                    cancellations++;
            }
        }
        else {  // 20% market order
            auto side = sideDist(gen) == 0 ? Order::BUY : Order::SELL;
            int qty = qtyDist(gen);
            int executed = book.match(side, qty);
            if (executed > 0) matches++;
        }
    }

    std::cout << "\n--- Simulation Results ---\n";
    std::cout << "Events processed: " << numEvents << "\n";
    std::cout << "New orders: " << arrivals << "\n";
    std::cout << "Cancellations: " << cancellations << "\n";
    std::cout << "Market order matches: " << matches << "\n";
    book.print();

    return 0;
}
