# ADR 0001: Test Framework

We need a test framework to express behavioral expectations, report failures
clearly, and run unit tests consistently in local builds and CI.

We choose Catch2 v3 because it provides readable test cases, simple assertions,
and CMake integration without requiring a separate test runner implementation.

We pin Catch2 to v3.8.1 to make dependency updates deliberate and reproducible.