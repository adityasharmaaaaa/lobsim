# lobsim

A deterministic limit order book simulator. This initial milestone implements the book state only: resting orders, queries, and invariant checking. Matching and cancellation are intentionally not implemented yet.

## Design choices

- **Price:** signed 64-bit integer ticks. Prices are never represented as floating point; this version accepts only positive prices.
- **Qty:** signed 64-bit integer units so negative input can be represented and rejected explicitly. Resting quantities must be positive.
- **OrderId:** unsigned 64-bit identifier; zero is reserved as invalid.
- **Side:** `enum class` with an 8-bit underlying representation.
- **Sequence:** unsigned 64-bit engine-assigned arrival sequence. Caller-provided `Order::seq` values are ignored.
- **Price levels:** bids sorted descending and asks ascending; orders at each level are held in FIFO order.
- **Order index:** an ID-to-side/price/sequence index detects duplicates and prepares for cancellation. Efficient arbitrary removal within a level is a later phase.

## Edge-case policy

- Order ID zero is rejected.
- Price zero or a negative price is rejected.
- Zero and negative quantities are rejected.
- Duplicate order IDs are rejected.
- An empty side has no best price. Spread and doubled midpoint are unavailable unless both sides have a best price.
- `rest()` deliberately does **not** match and does **not** reject crossing orders. This permits construction of invalid states for testing; `check_invariants()` reports a crossed or locked book.
- `mid_x2()` returns `nullopt` if the exact sum cannot fit in `int64_t`.
- Sequence numbers are assigned by the book, starting at 1. Exhaustion is reported instead of wrapping.
- Allocation failures are propagated as exceptions; `rest()` rolls back its queue insertion if index insertion throws.

## Requirements

- CMake 3.25+
- Ninja
- A C++20 compiler (Apple Clang/Clang/GCC)
- Internet access on the first configure to fetch Catch2 v3.8.1

On macOS with Homebrew, install build tools with `brew install cmake ninja`.

## Build and test

Debug:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

AddressSanitizer + UndefinedBehaviorSanitizer:

```bash
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan
ctest --preset asan-ubsan
```

Release:

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
```

Format check (if `clang-format` is installed):

```bash
clang-format --dry-run --Werror include/lobsim/core/order.hpp include/lobsim/book/book_result.hpp include/lobsim/book/book.hpp src/book/book.cpp tests/unit/book_test.cpp
```

## Current scope and limitations

This phase does not implement matching, cancellation, event replay, or the full per-order conservation invariant. The invariant checker verifies current book structure, sequence ordering, order-index consistency, and the non-crossed-book condition. It returns the first violation in a deterministic traversal order.
