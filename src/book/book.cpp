#include "lobsim/book/book.hpp"

#include <limits>
#include <sstream>
#include <unordered_set>

namespace lobsim {
namespace {

[[nodiscard]] bool add_would_overflow(std::int64_t lhs, std::int64_t rhs) noexcept {
    return rhs > 0 && lhs > std::numeric_limits<std::int64_t>::max() - rhs;
}

}  // namespace

std::optional<Price> Book::best_bid() const {
    if (bids_.empty()) {
        return std::nullopt;
    }
    return bids_.begin()->first;
}

std::optional<Price> Book::best_ask() const {
    if (asks_.empty()) {
        return std::nullopt;
    }
    return asks_.begin()->first;
}

std::optional<std::int64_t> Book::spread_ticks() const {
    const auto bid = best_bid();
    const auto ask = best_ask();
    if (!bid || !ask || bid->ticks >= ask->ticks) {
        return std::nullopt;
    }
    // Prices are validated as positive, so ask - bid cannot overflow here.
    return ask->ticks - bid->ticks;
}

std::optional<std::int64_t> Book::mid_x2() const {
    const auto bid = best_bid();
    const auto ask = best_ask();
    if (!bid || !ask) {
        return std::nullopt;
    }
    if (add_would_overflow(bid->ticks, ask->ticks)) {
        return std::nullopt;
    }
    return bid->ticks + ask->ticks;
}

Qty Book::depth_at(Side side, Price price) const {
    const OrderQueue* queue = nullptr;
    if (side == Side::Buy) {
        const auto it = bids_.find(price);
        if (it != bids_.end()) queue = &it->second;
    } else if (side == Side::Sell) {
        const auto it = asks_.find(price);
        if (it != asks_.end()) queue = &it->second;
    }

    if (queue == nullptr) return Qty{0};

    std::int64_t total = 0;
    for (const auto& order : *queue) {
        if (add_would_overflow(total, order.qty.value)) {
            // A valid book cannot reach this case; avoid undefined behavior if
            // internal state has been corrupted.
            return Qty{std::numeric_limits<std::int64_t>::max()};
        }
        total += order.qty.value;
    }
    return Qty{total};
}

std::vector<PriceLevel> Book::top_levels(Side side, std::size_t n) const {
    std::vector<PriceLevel> result;
    if (n == 0) return result;

    const auto append = [&result, n](const auto& levels) {
        result.reserve(n < levels.size() ? n : levels.size());
        for (const auto& [price, orders] : levels) {
            if (result.size() >= n) break;
            std::int64_t total = 0;
            for (const auto& order : orders) {
                if (add_would_overflow(total, order.qty.value)) {
                    total = std::numeric_limits<std::int64_t>::max();
                    break;
                }
                total += order.qty.value;
            }
            result.push_back(PriceLevel{price, Qty{total}, orders.size()});
        }
    };

    if (side == Side::Buy) append(bids_);
    else if (side == Side::Sell) append(asks_);
    return result;
}

std::string Book::snapshot() const {
    std::ostringstream out;
    const auto print = [&out](const char* heading, const auto& levels) {
        out << heading << '\n' << "Price(ticks)\tQuantity\tOrders\n";
        for (const auto& [price, orders] : levels) {
            std::int64_t total = 0;
            for (const auto& order : orders) {
                if (add_would_overflow(total, order.qty.value)) {
                    total = std::numeric_limits<std::int64_t>::max();
                    break;
                }
                total += order.qty.value;
            }
            out << price.ticks << '\t' << total << '\t' << orders.size() << '\n';
        }
    };
    print("ASKS (best to worst)", asks_);
    print("BIDS (best to worst)", bids_);
    return out.str();
}

RestResult Book::rest(Order order) {
    if (order.id.value == 0) return {RestError::InvalidOrderId};
    if (order.price.ticks <= 0) return {RestError::InvalidPrice};
    if (order.qty.value <= 0) return {RestError::InvalidQuantity};
    if (order.side != Side::Buy && order.side != Side::Sell) {
        return {RestError::InvalidSide};
    }
    if (order_index_.contains(order.id)) return {RestError::DuplicateOrderId};
    if (next_seq_ == std::numeric_limits<std::uint64_t>::max()) {
        return {RestError::InvalidSequence};
    }

    order.seq = next_seq_;

    // Keep insertion transactional: if indexing throws, remove the queue entry
    // and remove a newly-created empty level before propagating the exception.
    const auto insert_into = [this, &order](auto& levels) -> RestResult {
        auto [level_it, level_created] = levels.try_emplace(order.price);
        auto& queue = level_it->second;

        std::int64_t total = 0;
        for (const auto& existing : queue) {
            if (add_would_overflow(total, existing.qty.value)) {
                if (level_created) levels.erase(level_it);
                return {RestError::InvalidQuantity};
            }
            total += existing.qty.value;
        }
        if (add_would_overflow(total, order.qty.value)) {
            if (level_created) levels.erase(level_it);
            return {RestError::InvalidQuantity};
        }

        try {
            queue.push_back(order);
        } catch (...) {
            if (level_created && queue.empty()) levels.erase(level_it);
            throw;
        }

        try {
            const auto [index_it, inserted] = order_index_.emplace(
                order.id, OrderLocation{order.side, order.price, order.seq});
            (void)index_it;
            if (!inserted) {
                queue.pop_back();
                if (queue.empty()) levels.erase(level_it);
                return {RestError::DuplicateOrderId};
            }
        } catch (...) {
            queue.pop_back();
            if (queue.empty()) levels.erase(level_it);
            throw;
        }
        return {RestError::None};
    };

    const auto result = order.side == Side::Buy ? insert_into(bids_) : insert_into(asks_);
    if (!result) return result;

    ++next_seq_;
    return {RestError::None};
}

std::optional<std::string> Book::check_invariants() const {
    std::unordered_set<OrderId, OrderIdHash> seen_ids;
    std::unordered_set<std::uint64_t> seen_sequences;
    std::size_t order_count = 0;

    const auto check_side = [&](const auto& levels, Side expected_side)
        -> std::optional<std::string> {
        for (const auto& [price, queue] : levels) {
            if (price.ticks <= 0) return "Invariant: resting price is not positive";
            if (queue.empty()) return "Invariant: empty price level exists";

            std::int64_t total = 0;
            std::uint64_t previous_seq = 0;
            for (const auto& order : queue) {
                ++order_count;
                if (order.id.value == 0) return "Invariant: zero order ID";
                if (order.side != expected_side) return "Invariant: order stored on wrong side";
                if (order.price != price) return "Invariant: order price differs from its level";
                if (order.qty.value <= 0) return "Invariant: resting quantity is not positive";
                if (!seen_ids.insert(order.id).second) return "Invariant: duplicate resting order ID";
                if (order.seq == 0 || order.seq <= previous_seq) {
                    return "Invariant: sequence numbers are not strictly increasing within a level";
                }
                if (order.seq >= next_seq_) return "Invariant: order sequence is not below next sequence";
                if (!seen_sequences.insert(order.seq).second) return "Invariant: duplicate sequence number";
                previous_seq = order.seq;

                if (add_would_overflow(total, order.qty.value)) {
                    return "Invariant: price-level quantity overflows int64_t";
                }
                total += order.qty.value;

                const auto index_it = order_index_.find(order.id);
                if (index_it == order_index_.end()) return "Invariant: order missing from ID index";
                const auto& loc = index_it->second;
                if (loc.side != order.side || loc.price != order.price || loc.seq != order.seq) {
                    return "Invariant: ID index location mismatch";
                }
            }
        }
        return std::nullopt;
    };

    if (const auto error = check_side(bids_, Side::Buy)) return error;
    if (const auto error = check_side(asks_, Side::Sell)) return error;

    if (order_count != order_index_.size()) {
        return "Invariant: ID index size differs from resting order count";
    }

    const auto bid = best_bid();
    const auto ask = best_ask();
    if (bid && ask && bid->ticks >= ask->ticks) {
        return "Invariant: book is crossed or locked";
    }

    return std::nullopt;
}

}  // namespace lobsim
