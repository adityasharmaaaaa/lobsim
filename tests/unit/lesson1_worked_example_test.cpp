#include "../fixtures/lesson1_worked_example.hpp"
#include "lobsim/book/book.hpp"

#include <catch2/catch_test_macros.hpp>

namespace lobsim {

TEST_CASE("Lesson 1 Section E fixture preserves initial book state and FIFO") {
    const Book book = test_fixtures::make_lesson1_worked_example_book();

    // Dollar prices in the hand example are stored as integer cents/ticks.
    REQUIRE(book.best_bid() == Price{10002});
    REQUIRE(book.best_ask() == Price{10004});
    REQUIRE(book.spread_ticks() == 2);
    REQUIRE(book.mid_x2() == 20006); // 10002 + 10004; midpoint is 10003 ticks = $100.03.

    CHECK(book.depth_at(Side::Sell, Price{10004}) == Qty{400});
    CHECK(book.depth_at(Side::Sell, Price{10005}) == Qty{400});
    CHECK(book.depth_at(Side::Sell, Price{10007}) == Qty{600});
    CHECK(book.depth_at(Side::Buy, Price{10002}) == Qty{300});
    CHECK(book.depth_at(Side::Buy, Price{10001}) == Qty{500});
    CHECK(book.depth_at(Side::Buy, Price{10000}) == Qty{300});

    const auto asks = book.top_levels(Side::Sell, 3);
    REQUIRE(asks.size() == 3);
    CHECK(asks[0].price == Price{10004});
    CHECK(asks[0].total_qty == Qty{400});
    CHECK(asks[0].order_count == 2);
    CHECK(asks[1].price == Price{10005});
    CHECK(asks[2].price == Price{10007});

    const auto bids = book.top_levels(Side::Buy, 3);
    REQUIRE(bids.size() == 3);
    CHECK(bids[0].price == Price{10002});
    CHECK(bids[0].total_qty == Qty{300});
    CHECK(bids[0].order_count == 2);
    CHECK(bids[1].price == Price{10001});
    CHECK(bids[2].price == Price{10000});

    // Aggregate depth alone cannot prove FIFO; inspect the queue IDs directly
    // through the test-only friend accessor.
    const auto ask_queue = BookTestAccess::order_ids_at(book, Side::Sell, Price{10004});
    REQUIRE(ask_queue.size() == 2);
    CHECK(ask_queue[0] == OrderId{1}); // A1 arrived before A2.
    CHECK(ask_queue[1] == OrderId{2});

    const auto bid_queue = BookTestAccess::order_ids_at(book, Side::Buy, Price{10002});
    REQUIRE(bid_queue.size() == 2);
    CHECK(bid_queue[0] == OrderId{11}); // B1 arrived before B2.
    CHECK(bid_queue[1] == OrderId{12});

    CHECK_FALSE(book.check_invariants().has_value());
}

} // namespace lobsim
