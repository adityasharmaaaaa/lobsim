#include "lobsim/book/book.hpp"

#include <catch2/catch_test_macros.hpp>

namespace lobsim {

TEST_CASE("Empty book has no best prices") {
    Book book;

    REQUIRE_FALSE(book.best_bid().has_value());
    REQUIRE_FALSE(book.best_ask().has_value());
    REQUIRE_FALSE(book.spread_ticks().has_value());
    REQUIRE_FALSE(book.mid_x2().has_value());
    REQUIRE_FALSE(book.check_invariants().has_value());
}

TEST_CASE("Resting orders update best prices and depth") {
    Book book;

    REQUIRE(book.rest(Order{OrderId{1}, Side::Buy, Price{100}, Qty{10}, 0}));
    REQUIRE(book.rest(Order{OrderId{2}, Side::Buy, Price{101}, Qty{20}, 0}));
    REQUIRE(book.rest(Order{OrderId{3}, Side::Sell, Price{105}, Qty{15}, 0}));

    REQUIRE(book.best_bid() == Price{101});
    REQUIRE(book.best_ask() == Price{105});
    REQUIRE(book.spread_ticks() == 4);
    REQUIRE(book.mid_x2() == 206);
    REQUIRE(book.depth_at(Side::Buy, Price{101}) == Qty{20});
    REQUIRE(book.depth_at(Side::Buy, Price{100}) == Qty{10});
}

TEST_CASE("Rest permits crossing orders and invariant detects crossed book") {
    Book book;

    REQUIRE(book.rest(Order{OrderId{1}, Side::Sell, Price{105}, Qty{10}, 0}));
    REQUIRE(book.rest(Order{OrderId{2}, Side::Buy, Price{105}, Qty{5}, 0}));

    REQUIRE(book.check_invariants() == "Invariant: book is crossed or locked");
}

TEST_CASE("Rest rejects duplicate IDs") {
    Book book;

    REQUIRE(book.rest(Order{OrderId{1}, Side::Buy, Price{100}, Qty{10}, 0}));

    const auto result = book.rest(Order{OrderId{1}, Side::Buy, Price{99}, Qty{5}, 0});
    REQUIRE(result.error == RestError::DuplicateOrderId);
    REQUIRE(book.depth_at(Side::Buy, Price{100}) == Qty{10});
    REQUIRE_FALSE(book.check_invariants().has_value());
}

TEST_CASE("top_levels and snapshot query book structure") {
    Book book;
    REQUIRE(book.rest(Order{OrderId{1}, Side::Buy, Price{100}, Qty{10}, 0}));
    REQUIRE(book.rest(Order{OrderId{2}, Side::Buy, Price{99}, Qty{25}, 0}));
    REQUIRE(book.rest(Order{OrderId{3}, Side::Sell, Price{105}, Qty{15}, 0}));

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

}  // namespace lobsim
