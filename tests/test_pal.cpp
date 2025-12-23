// tests/test_pal.cpp
// Blueprint Reference: [TEST-04], [TEST-05], [TASK-04.02]
// Unit tests for Platform Abstraction Layer (PAL)

#include "doctest.h"
#include "pal/timer.h"

#include <chrono>
#include <thread>

TEST_SUITE("PAL Timer") {
    TEST_CASE("Monotonic clock returns increasing values" * doctest::test_suite("pal")) {
        // [TEST-04] Verify monotonic clock behavior
        auto t1 = vgcpu::pal::NowMonotonic();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto t2 = vgcpu::pal::NowMonotonic();

        CHECK(t2 > t1);
    }

    TEST_CASE("CPU timer returns non-negative values" * doctest::test_suite("pal")) {
        // [TEST-05] Verify CPU timer behavior
        auto cpu_time = vgcpu::pal::GetCpuTime();

        // CPU time should be non-negative
        CHECK(cpu_time.count() >= 0);
    }

    TEST_CASE("Timer elapsed calculation is correct" * doctest::test_suite("pal")) {
        auto start = vgcpu::pal::NowMonotonic();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto end = vgcpu::pal::NowMonotonic();

        auto elapsed = vgcpu::pal::Elapsed(start, end);
        auto elapsed_ns = vgcpu::pal::ToNanoseconds(elapsed);

        // Should be at least 10ms (10,000,000 ns), allowing some tolerance
        CHECK(elapsed_ns >= 5'000'000);     // At least 5ms
        CHECK(elapsed_ns < 1'000'000'000);  // Less than 1s
    }

    TEST_CASE("ToNanoseconds conversion works" * doctest::test_suite("pal")) {
        auto duration = std::chrono::nanoseconds(1234567890);
        auto ns = vgcpu::pal::ToNanoseconds(duration);
        CHECK(ns == 1234567890);
    }

    TEST_CASE("ToMilliseconds conversion works" * doctest::test_suite("pal")) {
        auto duration = std::chrono::nanoseconds(1'000'000);  // 1ms
        auto ms = vgcpu::pal::ToMilliseconds(duration);
        CHECK(ms == doctest::Approx(1.0));
    }
}
