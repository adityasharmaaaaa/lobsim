#include "../fixtures/book_test_access.hpp"
#include "lobsim/book/book.hpp"

#include <catch2/catch_test_macros.hpp>
#include <limits>

namespace lobsim {

TEST_CASE("Empty book has no best prices and satisfies invariants") {
    Book book;

    REQUIRE_FALSE(book.best_bid().has_value());
    REQUIRE_FALSE(book.best_ask().has_value());
    REQUIRE_FALSE(book.spread_ticks().has_value());
    REQUIRE_FALSE(book.mid_x2().has_value());
    REQUIRE_FALSE(book.check_invariants().has_value());
}

TEST_CASE("Resting orders update best prices, spread, mid, and depth") {
    Book book;

    REQUIRE(book.rest(Order{OrderId{1}, Side::Buy, Price{100}, Qty{10}, 0}));
    REQUIRE_FALSE(book.check_invariants().has_value());

    REQUIRE(book.rest(Order{OrderId{2}, Side::Buy, Price{101}, Qty{20}, 0}));
    REQUIRE_FALSE(book.check_invariants().has_value());

    REQUIRE(book.rest(Order{OrderId{3}, Side::Sell, Price{105}, Qty{15}, 0}));
    REQUIRE_FALSE(book.check_invariants().has_value());

    REQUIRE(book.best_bid() == Price{101});
    REQUIRE(book.best_ask() == Price{105});
    REQUIRE(book.spread_ticks() == 4);
    REQUIRE(book.mid_x2() == 206);
    REQUIRE(book.depth_at(Side::Buy, Price{101}) == Qty{20});
    REQUIRE(book.depth_at(Side::Buy, Price{100}) == Qty{10});
    REQUIRE(book.depth_at(Side::Sell, Price{105}) == Qty{15});
    REQUIRE(book.depth_at(Side::Sell, Price{110}) == Qty{0});
}

TEST_CASE("FIFO priority is strictly maintained within price levels") {
    Book book;

    REQUIRE(book.rest(Order{OrderId{10}, Side::Buy, Price{5000}, Qty{100}, 0}));
    REQUIRE_FALSE(book.check_invariants().has_value());

    REQUIRE(book.rest(Order{OrderId{20}, Side::Buy, Price{5000}, Qty{300}, 0}));
    REQUIRE_FALSE(book.check_invariants().has_value());

    REQUIRE(book.rest(Order{OrderId{30}, Side::Buy, Price{5000}, Qty{50}, 0}));
    REQUIRE_FALSE(book.check_invariants().has_value());

    // Check depth and level count
    REQUIRE(book.depth_at(Side::Buy, Price{5000}) == Qty{450});
    const auto top = book.top_levels(Side::Buy, 1);
    REQUIRE(top.size() == 1);
    REQUIRE(top[0].price == Price{5000});
    REQUIRE(top[0].total_qty == Qty{450});
    REQUIRE(top[0].order_count == 3);

    // Verify exact arrival order in queue
    const auto ids = BookTestAccess::order_ids_at(book, Side::Buy, Price{5000});
    REQUIRE(ids.size() == 3);
    REQUIRE(ids[0] == OrderId{10});
    REQUIRE(ids[1] == OrderId{20});
    REQUIRE(ids[2] == OrderId{30});

    // Verify engine-assigned sequence numbers are strictly monotonically increasing
    const auto seqs = BookTestAccess::order_seqs_at(book, Side::Buy, Price{5000});
    REQUIRE(seqs.size() == 3);
    REQUIRE(seqs[0] == 1);
    REQUIRE(seqs[1] == 2);
    REQUIRE(seqs[2] == 3);
    REQUIRE_FALSE(book.check_invariants().has_value());
}

TEST_CASE("Rest admission preconditions reject invalid orders safely") {
    Book book;

    // Invalid Order ID
    const auto err_id = book.rest(Order{OrderId{0}, Side::Buy, Price{100}, Qty{10}, 0});
    REQUIRE(err_id.error == RestError::InvalidOrderId);
    REQUIRE_FALSE(book.check_invariants().has_value());

    // Zero or negative price
    const auto err_p0 = book.rest(Order{OrderId{1}, Side::Buy, Price{0}, Qty{10}, 0});
    REQUIRE(err_p0.error == RestError::InvalidPrice);
    const auto err_p_neg = book.rest(Order{OrderId{1}, Side::Buy, Price{-50}, Qty{10}, 0});
    REQUIRE(err_p_neg.error == RestError::InvalidPrice);
    REQUIRE_FALSE(book.check_invariants().has_value());

    // Zero or negative quantity
    const auto err_q0 = book.rest(Order{OrderId{1}, Side::Buy, Price{100}, Qty{0}, 0});
    REQUIRE(err_q0.error == RestError::InvalidQuantity);
    const auto err_q_neg = book.rest(Order{OrderId{1}, Side::Buy, Price{100}, Qty{-10}, 0});
    REQUIRE(err_q_neg.error == RestError::InvalidQuantity);
    REQUIRE_FALSE(book.check_invariants().has_value());

    // Invalid side enum cast
    const auto err_side =
        book.rest(Order{OrderId{1}, static_cast<Side>(99), Price{100}, Qty{10}, 0});
    REQUIRE(err_side.error == RestError::InvalidSide);
    REQUIRE_FALSE(book.check_invariants().has_value());

    // Valid order succeeds
    REQUIRE(book.rest(Order{OrderId{1}, Side::Buy, Price{100}, Qty{10}, 0}));
    REQUIRE_FALSE(book.check_invariants().has_value());

    // Duplicate Order ID rejected without corrupting book
    const auto err_dup = book.rest(Order{OrderId{1}, Side::Sell, Price{105}, Qty{5}, 0});
    REQUIRE(err_dup.error == RestError::DuplicateOrderId);
    REQUIRE(book.depth_at(Side::Buy, Price{100}) == Qty{10});
    REQUIRE(book.depth_at(Side::Sell, Price{105}) == Qty{0});
    REQUIRE_FALSE(book.check_invariants().has_value());

    // Price level quantity overflow rejected
    REQUIRE(book.rest(Order{OrderId{2}, Side::Buy, Price{100},
                            Qty{std::numeric_limits<std::int64_t>::max() - 20}, 0}));
    REQUIRE_FALSE(book.check_invariants().has_value());
    const auto err_overflow = book.rest(Order{OrderId{3}, Side::Buy, Price{100}, Qty{50}, 0});
    REQUIRE(err_overflow.error == RestError::InvalidQuantity);
    REQUIRE_FALSE(book.check_invariants().has_value());
}

TEST_CASE("top_levels and snapshot query book structure correctly") {
    Book book;
    REQUIRE(book.rest(Order{OrderId{1}, Side::Buy, Price{100}, Qty{10}, 0}));
    REQUIRE_FALSE(book.check_invariants().has_value());

    REQUIRE(book.rest(Order{OrderId{2}, Side::Buy, Price{99}, Qty{25}, 0}));
    REQUIRE_FALSE(book.check_invariants().has_value());

    REQUIRE(book.rest(Order{OrderId{3}, Side::Sell, Price{105}, Qty{15}, 0}));
    REQUIRE_FALSE(book.check_invariants().has_value());

    const auto bids = book.top_levels(Side::Buy, 2);
    REQUIRE(bids.size() == 2);
    REQUIRE(bids[0].price == Price{100});
    REQUIRE(bids[0].total_qty == Qty{10});
    REQUIRE(bids[0].order_count == 1);
    REQUIRE(bids[1].price == Price{99});
    REQUIRE(bids[1].total_qty == Qty{25});

    const auto snap = book.snapshot();
    REQUIRE(snap.find("ASKS (best to worst)") != std::string::npos);
    REQUIRE(snap.find("BIDS (best to worst)") != std::string::npos);
    REQUIRE_FALSE(book.check_invariants().has_value());
}

TEST_CASE("Invariant 1: check_invariants detects crossed and locked books") {
    // Crossed book (bid > ask)
    {
        Book book;
        REQUIRE(book.rest(Order{OrderId{1}, Side::Sell, Price{100}, Qty{10}, 0}));
        REQUIRE(book.rest(Order{OrderId{2}, Side::Buy, Price{105}, Qty{5}, 0}));
        REQUIRE(book.check_invariants() == "Invariant: book is crossed or locked");
    }

    // Locked book (bid == ask)
    {
        Book book;
        REQUIRE(book.rest(Order{OrderId{1}, Side::Sell, Price{100}, Qty{10}, 0}));
        REQUIRE(book.rest(Order{OrderId{2}, Side::Buy, Price{100}, Qty{5}, 0}));
        REQUIRE(book.check_invariants() == "Invariant: book is crossed or locked");
    }
}

TEST_CASE("Invariant 2: check_invariants detects empty levels and non-positive resting quantity") {
    // Empty price level
    {
        Book book;
        REQUIRE(book.rest(Order{OrderId{1}, Side::Buy, Price{100}, Qty{10}, 0}));
        REQUIRE_FALSE(book.check_invariants().has_value());

        BookTestAccess::insert_empty_level(book, Side::Buy, Price{99});
        REQUIRE(book.check_invariants() == "Invariant: empty price level exists");
    }

    // Non-positive resting quantity
    {
        Book book;
        REQUIRE(book.rest(Order{OrderId{1}, Side::Buy, Price{100}, Qty{10}, 0}));
        REQUIRE_FALSE(book.check_invariants().has_value());

        BookTestAccess::set_order_qty(book, Side::Buy, Price{100}, 0, Qty{0});
        REQUIRE(book.check_invariants() == "Invariant: resting quantity is not positive");

        BookTestAccess::set_order_qty(book, Side::Buy, Price{100}, 0, Qty{-5});
        REQUIRE(book.check_invariants() == "Invariant: resting quantity is not positive");
    }
}

TEST_CASE("Invariant 3: check_invariants detects unique ID violations and index corruption") {
    // Non-positive price level
    {
        Book book;
        BookTestAccess::insert_corrupt_level(book, Side::Buy, Price{-10},
                                             Order{OrderId{1}, Side::Buy, Price{-10}, Qty{10}, 1});
        BookTestAccess::set_next_seq(book, 2);
        REQUIRE(book.check_invariants() == "Invariant: resting price is not positive");
    }

    // Zero order ID
    {
        Book book;
        REQUIRE(book.rest(Order{OrderId{1}, Side::Buy, Price{100}, Qty{10}, 0}));
        BookTestAccess::set_order_id(book, Side::Buy, Price{100}, 0, OrderId{0});
        REQUIRE(book.check_invariants() == "Invariant: zero order ID");
    }

    // Order stored on wrong side
    {
        Book book;
        REQUIRE(book.rest(Order{OrderId{1}, Side::Buy, Price{100}, Qty{10}, 0}));
        BookTestAccess::set_order_side(book, Side::Buy, Price{100}, 0, Side::Sell);
        REQUIRE(book.check_invariants() == "Invariant: order stored on wrong side");
    }

    // Order price differs from level price
    {
        Book book;
        REQUIRE(book.rest(Order{OrderId{1}, Side::Buy, Price{100}, Qty{10}, 0}));
        BookTestAccess::set_order_price(book, Side::Buy, Price{100}, 0, Price{101});
        REQUIRE(book.check_invariants() == "Invariant: order price differs from its level");
    }

    // Duplicate order ID across levels
    {
        Book book;
        REQUIRE(book.rest(Order{OrderId{1}, Side::Buy, Price{100}, Qty{10}, 0}));
        REQUIRE(book.rest(Order{OrderId{2}, Side::Buy, Price{99}, Qty{10}, 0}));
        BookTestAccess::set_order_id(book, Side::Buy, Price{99}, 0, OrderId{1});
        REQUIRE(book.check_invariants() == "Invariant: duplicate resting order ID");
    }

    // Order missing from ID index
    {
        Book book;
        REQUIRE(book.rest(Order{OrderId{1}, Side::Buy, Price{100}, Qty{10}, 0}));
        REQUIRE(book.rest(Order{OrderId{2}, Side::Buy, Price{99}, Qty{10}, 0}));
        BookTestAccess::remove_from_index(book, OrderId{1});
        REQUIRE(book.check_invariants() == "Invariant: order missing from ID index");
    }

    // ID index location mismatch
    {
        Book book;
        REQUIRE(book.rest(Order{OrderId{1}, Side::Buy, Price{100}, Qty{10}, 0}));
        BookTestAccess::set_index_location(book, OrderId{1}, Side::Sell, Price{100}, 1);
        REQUIRE(book.check_invariants() == "Invariant: ID index location mismatch");
    }

    // ID index size differs from resting order count (extra dangling index entry)
    {
        Book book;
        REQUIRE(book.rest(Order{OrderId{1}, Side::Buy, Price{100}, Qty{10}, 0}));
        BookTestAccess::insert_index_extra(book, OrderId{999}, Side::Buy, Price{100}, 1);
        REQUIRE(book.check_invariants() ==
                "Invariant: ID index size differs from resting order count");
    }

    // Price level quantity overflow
    {
        Book book;
        REQUIRE(book.rest(Order{OrderId{1}, Side::Buy, Price{100},
                                Qty{std::numeric_limits<std::int64_t>::max() - 10}, 0}));
        REQUIRE(book.rest(Order{OrderId{2}, Side::Buy, Price{100}, Qty{5}, 0}));
        REQUIRE_FALSE(book.check_invariants().has_value());

        // Corrupt second order to exceed max
        BookTestAccess::set_order_qty(book, Side::Buy, Price{100}, 1, Qty{20});
        REQUIRE(book.check_invariants() == "Invariant: price-level quantity overflows int64_t");
    }
}

TEST_CASE("Invariant 4: check_invariants detects sequence number priority violations") {
    // Non-increasing sequence within level
    {
        Book book;
        REQUIRE(book.rest(Order{OrderId{1}, Side::Buy, Price{100}, Qty{10}, 0}));
        REQUIRE(book.rest(Order{OrderId{2}, Side::Buy, Price{100}, Qty{10}, 0}));
        REQUIRE_FALSE(book.check_invariants().has_value());

        // Invert sequence numbers: seq 2 before seq 1
        BookTestAccess::set_order_seq(book, Side::Buy, Price{100}, 0, 2);
        BookTestAccess::set_order_seq(book, Side::Buy, Price{100}, 1, 1);
        BookTestAccess::set_index_location(book, OrderId{1}, Side::Buy, Price{100}, 2);
        BookTestAccess::set_index_location(book, OrderId{2}, Side::Buy, Price{100}, 1);
        REQUIRE(book.check_invariants() ==
                "Invariant: sequence numbers are not strictly increasing within a level");
    }

    // Order sequence >= next_seq_
    {
        Book book;
        REQUIRE(book.rest(Order{OrderId{1}, Side::Buy, Price{100}, Qty{10}, 0}));
        REQUIRE_FALSE(book.check_invariants().has_value());

        BookTestAccess::set_next_seq(book, 1); // order.seq is 1, so seq == next_seq_
        REQUIRE(book.check_invariants() == "Invariant: order sequence is not below next sequence");
    }

    // Duplicate sequence number across different levels
    {
        Book book;
        REQUIRE(book.rest(Order{OrderId{1}, Side::Buy, Price{100}, Qty{10}, 0}));
        REQUIRE(book.rest(Order{OrderId{2}, Side::Buy, Price{99}, Qty{10}, 0}));
        REQUIRE_FALSE(book.check_invariants().has_value());

        // Corrupt order 2 to have duplicate sequence 1
        BookTestAccess::set_order_seq(book, Side::Buy, Price{99}, 0, 1);
        BookTestAccess::set_index_location(book, OrderId{2}, Side::Buy, Price{99}, 1);
        REQUIRE(book.check_invariants() == "Invariant: duplicate sequence number");
    }
}

} // namespace lobsim
