#pragma once

#include "lobsim/book/book.hpp"

#include <cstdint>
#include <deque>
#include <vector>

namespace lobsim {

// Test-only access shim. Declared as a friend in Book to allow unit tests
// to inspect internal queues and inject corrupted internal states for
// testing check_invariants() without exposing dangerous backdoors in the public API.
struct BookTestAccess {
    [[nodiscard]] static std::vector<OrderId> order_ids_at(const Book &book, Side side,
                                                           Price price) {
        std::vector<OrderId> ids;
        if (side == Side::Buy) {
            const auto it = book.bids_.find(price);
            if (it != book.bids_.end()) {
                ids.reserve(it->second.size());
                for (const auto &order : it->second) {
                    ids.push_back(order.id);
                }
            }
        } else if (side == Side::Sell) {
            const auto it = book.asks_.find(price);
            if (it != book.asks_.end()) {
                ids.reserve(it->second.size());
                for (const auto &order : it->second) {
                    ids.push_back(order.id);
                }
            }
        }
        return ids;
    }

    [[nodiscard]] static std::vector<std::uint64_t> order_seqs_at(const Book &book, Side side,
                                                                  Price price) {
        std::vector<std::uint64_t> seqs;
        if (side == Side::Buy) {
            const auto it = book.bids_.find(price);
            if (it != book.bids_.end()) {
                seqs.reserve(it->second.size());
                for (const auto &order : it->second) {
                    seqs.push_back(order.seq);
                }
            }
        } else if (side == Side::Sell) {
            const auto it = book.asks_.find(price);
            if (it != book.asks_.end()) {
                seqs.reserve(it->second.size());
                for (const auto &order : it->second) {
                    seqs.push_back(order.seq);
                }
            }
        }
        return seqs;
    }

    // Mutators for simulating corrupted states to verify check_invariants()

    static void insert_empty_level(Book &book, Side side, Price price) {
        if (side == Side::Buy) {
            book.bids_[price] = {};
        } else {
            book.asks_[price] = {};
        }
    }

    static void set_order_price(Book &book, Side side, Price level_price, std::size_t idx,
                                Price new_price) {
        auto &queue = (side == Side::Buy) ? book.bids_.at(level_price) : book.asks_.at(level_price);
        queue.at(idx).price = new_price;
    }

    static void set_order_side(Book &book, Side side, Price level_price, std::size_t idx,
                               Side new_side) {
        auto &queue = (side == Side::Buy) ? book.bids_.at(level_price) : book.asks_.at(level_price);
        queue.at(idx).side = new_side;
    }

    static void set_order_qty(Book &book, Side side, Price level_price, std::size_t idx,
                              Qty new_qty) {
        auto &queue = (side == Side::Buy) ? book.bids_.at(level_price) : book.asks_.at(level_price);
        queue.at(idx).qty = new_qty;
    }

    static void set_order_id(Book &book, Side side, Price level_price, std::size_t idx,
                             OrderId new_id) {
        auto &queue = (side == Side::Buy) ? book.bids_.at(level_price) : book.asks_.at(level_price);
        queue.at(idx).id = new_id;
    }

    static void set_order_seq(Book &book, Side side, Price level_price, std::size_t idx,
                              std::uint64_t new_seq) {
        auto &queue = (side == Side::Buy) ? book.bids_.at(level_price) : book.asks_.at(level_price);
        queue.at(idx).seq = new_seq;
    }

    static void set_next_seq(Book &book, std::uint64_t next_seq) {
        book.next_seq_ = next_seq;
    }

    static void remove_from_index(Book &book, OrderId id) {
        book.order_index_.erase(id);
    }

    static void set_index_location(Book &book, OrderId id, Side side, Price price,
                                   std::uint64_t seq) {
        book.order_index_[id] = Book::OrderLocation{side, price, seq};
    }

    static void insert_index_extra(Book &book, OrderId id, Side side, Price price,
                                   std::uint64_t seq) {
        book.order_index_[id] = Book::OrderLocation{side, price, seq};
    }

    static void insert_corrupt_level(Book &book, Side side, Price price, Order order) {
        if (side == Side::Buy) {
            book.bids_[price].push_back(order);
        } else {
            book.asks_[price].push_back(order);
        }
        book.order_index_[order.id] = Book::OrderLocation{order.side, order.price, order.seq};
    }
};

} // namespace lobsim
