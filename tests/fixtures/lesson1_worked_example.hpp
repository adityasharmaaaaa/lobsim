#pragma once

#include "book_test_access.hpp"
#include "lobsim/book/book.hpp"

#include <stdexcept>
#include <string>
#include <vector>

namespace lobsim {
namespace test_fixtures {

// Prices are represented as integer ticks. With a $0.01 tick size, $100.04
// is Price{10004}, not a floating-point dollar value.
inline void rest_checked(Book &book, Order order) {
    const RestResult result = book.rest(order);
    if (!result) {
        throw std::logic_error("Could not construct Lesson 1 worked-example fixture: " +
                               std::string(to_string(result.error)));
    }

    // The fixture intentionally checks the invariant checker after every
    // mutation, not just once after the whole initial book has been built.
    if (const auto error = book.check_invariants()) {
        throw std::logic_error("Lesson 1 fixture violated a book invariant: " + *error);
    }
}

[[nodiscard]] inline Book make_lesson1_worked_example_book() {
    Book book;

    // Asks: low to high. The order within each level is FIFO.
    rest_checked(book, Order{OrderId{4}, Side::Sell, Price{10007}, Qty{600}, 0}); // A4 @ 100.07
    rest_checked(book, Order{OrderId{3}, Side::Sell, Price{10005}, Qty{400}, 0}); // A3 @ 100.05
    rest_checked(book, Order{OrderId{1}, Side::Sell, Price{10004}, Qty{150}, 0}); // A1 @ 100.04
    rest_checked(book, Order{OrderId{2}, Side::Sell, Price{10004}, Qty{250}, 0}); // A2 @ 100.04

    // Bids: high to low. B1 must precede B2 at the 100.02 level.
    rest_checked(book, Order{OrderId{11}, Side::Buy, Price{10002}, Qty{200}, 0}); // B1 @ 100.02
    rest_checked(book, Order{OrderId{12}, Side::Buy, Price{10002}, Qty{100}, 0}); // B2 @ 100.02
    rest_checked(book, Order{OrderId{13}, Side::Buy, Price{10001}, Qty{500}, 0}); // B3 @ 100.01
    rest_checked(book, Order{OrderId{14}, Side::Buy, Price{10000}, Qty{300}, 0}); // B4 @ 100.00

    return book;
}

} // namespace test_fixtures
} // namespace lobsim
