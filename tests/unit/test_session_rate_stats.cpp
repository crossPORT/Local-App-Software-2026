#include "test_util.h"

#include "session_rate_stats.h"

FABRIC_TEST(session_rate_stats_empty) {
    const RateStats stats = compute_rate_stats({});
    CHECK_EQ(stats.count, 0);
    CHECK(stats.median == 0.0);
}

FABRIC_TEST(session_rate_stats_median_odd) {
    const RateStats stats = compute_rate_stats({30.0, 10.0, 20.0});
    CHECK_EQ(stats.count, 3);
    CHECK(stats.median == 20.0);
    CHECK(stats.max == 30.0);
    CHECK(stats.average == 20.0);
}

FABRIC_TEST(session_rate_stats_median_even) {
    const RateStats stats = compute_rate_stats({10.0, 20.0, 30.0, 40.0});
    CHECK_EQ(stats.count, 4);
    CHECK(stats.median == 25.0);
    CHECK(stats.max == 40.0);
}

FABRIC_TEST(session_rate_tracker_rising_edge) {
    SessionRateTracker tracker;
    tracker.observe_result_mbps(0.0);
    tracker.observe_result_mbps(12.5);
    tracker.observe_result_mbps(12.5);
    tracker.observe_result_mbps(0.0);
    tracker.observe_result_mbps(24.0);
    const RateStats stats = tracker.stats();
    CHECK_EQ(stats.count, 2);
    CHECK(stats.average == 18.25);
}
