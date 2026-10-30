#pragma once

#include <string_view>

namespace lobsim {

enum class RestError {
    None,
    InvalidOrderId,
    InvalidPrice,
    InvalidQuantity,
    DuplicateOrderId,
    InvalidSide,
    InvalidSequence
};

[[nodiscard]] constexpr std::string_view to_string(RestError error) noexcept {
    switch (error) {
    case RestError::None:
        return "None";
    case RestError::InvalidOrderId:
        return "Invalid order ID";
    case RestError::InvalidPrice:
        return "Invalid price";
    case RestError::InvalidQuantity:
        return "Invalid quantity";
    case RestError::DuplicateOrderId:
        return "Duplicate order ID";
    case RestError::InvalidSide:
        return "Invalid side";
    case RestError::InvalidSequence:
        return "Sequence number exhausted";
    }
    return "Unknown error";
}

struct RestResult {
    RestError error{RestError::None};

    [[nodiscard]] constexpr bool ok() const noexcept {
        return error == RestError::None;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return ok();
    }
};

} // namespace lobsim
