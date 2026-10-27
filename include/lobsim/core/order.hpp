#pragma once 

#include <compare>
#include <cstdint>

namespace lobsim{

struct Price {
    std::int64_t ticks{};
    
    auto operator<=>(const Price&) const = default;
};

struct Qty{
    std::uint64_t value{};

    auto operator<=>(const Qty&) const = default;
};

struct OrderId{
    std::uint64_t value{};

    auto operator<=>(const OrderId&) const = default;
};

enum class Side: std::uint8_t{
    Buy,
    Sell
};

struct Order{
    OrderId id{};
    Side side{};
    Price price{};
    Qty qty{};
    std::uint64_t seq{};
};

} //namespace lobsim