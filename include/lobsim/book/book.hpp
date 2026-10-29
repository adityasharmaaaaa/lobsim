#pragma once

#include "lobsim/book/book_result.hpp"
#include "lobsim/core/order.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace lobsim {

struct PriceLevel {
    Price price{};
    Qty total_qty{};
    std::size_t order_count{};
};

struct OrderIdHash {
    [[nodiscard]] std::size_t operator()(OrderId id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

class Book {
public:
    Book() = default;

    //inserts without matching. Crossed books are permitted for invariant tests;
    //check_invariants() reports them. Sequence numbers are assigned internally.
    [[nodiscard]] RestResult rest(Order order);

    [[nodiscard]] std::optional<Price> best_bid() const;
    [[nodiscard]] std::optional<Price> best_ask() const;
    [[nodiscard]] std::optional<std::int64_t> spread_ticks() const;
    [[nodiscard]] std::optional<std::int64_t> mid_x2() const;
    [[nodiscard]] Qty depth_at(Side side, Price price) const;
    [[nodiscard]] std::vector<PriceLevel> top_levels(Side side, std::size_t n) const;
    [[nodiscard]] std::string snapshot() const;

    // Returns nullopt when valid, otherwise the first detected violation.
    [[nodiscard]] std::optional<std::string> check_invariants() const;

private:
    // Test-only friend: keeps per-level FIFO inspection out of the public API.
    friend struct BookTestAccess;

    using OrderQueue = std::deque<Order>;
    using BidLevels = std::map<Price, OrderQueue, std::greater<Price>>;
    using AskLevels = std::map<Price, OrderQueue>;

    struct OrderLocation {
        Side side{};
        Price price{};
        std::uint64_t seq{};
    };
    using OrderIndex = std::unordered_map<OrderId, OrderLocation, OrderIdHash>;

    BidLevels bids_;
    AskLevels asks_;
    OrderIndex order_index_;
    std::uint64_t next_seq_{1};
};

}  // namespace lobsim
