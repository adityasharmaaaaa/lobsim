#pragma once

#include <compare>
#include <cstdint>

namespace lobsim {

// prices are represented as integer ticks, never floating-point values.
struct Price {
    std::int64_t ticks{};
    auto operator<=>(const Price &) const = default;
};

// signed deliberately: rest() must be able to reject negative input safely.
struct Qty {
    std::int64_t value{};
    auto operator<=>(const Qty &) const = default;
};

struct OrderId {
    std::uint64_t value{};
    auto operator<=>(const OrderId &) const = default;
};

enum class Side : std::uint8_t { Buy, Sell };

struct Order {
    OrderId id{};
    Side side{};
    Price price{};
    Qty qty{};
    // Assigned by Book::rest; caller-provided values are ignored.
    std::uint64_t seq{};
};

} // namespace lobsim
