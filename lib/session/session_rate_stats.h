#pragma once

#include <vector>

struct RateStats {
    int count = 0;
    double median = 0.0;
    double max = 0.0;
    double average = 0.0;
};

RateStats compute_rate_stats(const std::vector<double>& rates);

/** Accumulates completed-transfer rates for the current app session. */
class SessionRateTracker {
public:
    void observe_result_mbps(double result_mbps);
    RateStats stats() const;

private:
    std::vector<double> rates_;
    double prev_result_mbps_ = 0.0;
};
