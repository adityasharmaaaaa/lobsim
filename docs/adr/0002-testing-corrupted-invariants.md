# ADR 0002: Testing Invariant Checker Against Inaccessible Corrupted States

## Status
Accepted

## Context
`Book::check_invariants()` is designed to audit the internal data structures of the order book and guarantee that all state invariants hold (uncrossed book, no empty levels, strictly increasing sequence numbers, conservation of order quantity, ID index consistency, and lack of overflow).

However, `Book::rest()` validates order preconditions at admission:
- Positive prices
- Positive quantities
- Non-zero order IDs
- Valid side enum
- Prevention of duplicate order IDs
- Sequence number monotonically assigned internally

Because `rest()` defensively rejects invalid input and internally manages sequence numbers and level lifetimes, certain corrupt book states cannot be produced through the public API during normal execution (for example, empty price levels, duplicate sequence numbers, non-strictly increasing sequence numbers, or index location mismatches).

We must decide how to verify that `check_invariants()` correctly detects these corrupt states and returns the specific, informative error strings without compromising production encapsulation or introducing unsafe backdoors.

## Alternatives Considered

1. **Relax Public API Validation (`rest()`)**:
   - *Description*: Remove admission checks from `rest()` and allow callers to insert arbitrary orders with user-specified sequence numbers or invalid fields.
   - *Cost*: High risk of defects. A public matching engine or order book API that accepts malformed orders compromises fail-fast safety and allows bugs in callers or feeds to silently pollute book state.
   - *Verdict*: Rejected.

2. **Public / `#ifdef` Corruption Methods in `Book`**:
   - *Description*: Add methods such as `corrupt_duplicate_seq()` or `#ifdef TESTING` blocks inside `Book`.
   - *Cost*: Pollutes the public interface and class ABI with test-only code. Conditional compilation (`#ifdef`) also means test binaries run against different code than release binaries.
   - *Verdict*: Rejected.

3. **Subclassing / Protected Internals**:
   - *Description*: Make book state protected and subclass `Book` in tests.
   - *Cost*: Forces virtual dispatch or inheritance semantics onto value-oriented high-performance data structures, adding indirection and risking object slicing.
   - *Verdict*: Rejected.

4. **Friend Test Accessor (`BookTestAccess`)**:
   - *Description*: Declare `friend struct BookTestAccess;` inside `Book`. The struct definition resides strictly in test fixtures (`tests/fixtures/book_test_access.hpp`) and is never exposed in library installation headers.
   - *Pros*:
     - Keeps `Book`'s public API strictly encapsulated, safe, and minimal.
     - Zero runtime overhead in production and identical ABI across build configurations.
     - Provides test fixtures with precise, surgical capability to construct pathological internal states (e.g., injecting an empty level or swapping sequence numbers) to verify every error branch in `check_invariants()`.
   - *Verdict*: Accepted.

## Decision
We declare `friend struct BookTestAccess;` in `include/lobsim/book/book.hpp` and define `BookTestAccess` within `tests/fixtures/book_test_access.hpp`. 

Unit tests will use `BookTestAccess` exclusively to verify that `check_invariants()` detects every specific structural invariant violation (invariants 1–4 plus index and arithmetic bounds) and returns the corresponding diagnostic error string.
