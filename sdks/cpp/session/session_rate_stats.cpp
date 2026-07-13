#include "session_rate_stats.h"

#include <algorithm>
#include <cmath>
#include <numeric>

RateStats compute_rate_stats(const std::vector<double>& rates) {
    std::vector<double> values;
    values.reserve(rates.size());
    for (double rate : rates) {
        if (rate > 0.0) {
            values.push_back(rate);
        }
    }
    if (values.empty()) {
        return {};
    }

    std::sort(values.begin(), values.end());
    const int count = static_cast<int>(values.size());
    const int mid = count / 2;
    const double median = (count % 2 == 0)
        ? (values[static_cast<size_t>(mid - 1)] + values[static_cast<size_t>(mid)]) / 2.0
        : values[static_cast<size_t>(mid)];
    const double max = values.back();
    const double average =
        std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(count);
    return {count, median, max, average};
}

void SessionRateTracker::observe_result_mbps(double result_mbps) {
    if (result_mbps > 0.0 && prev_result_mbps_ <= 0.0) {
        rates_.push_back(result_mbps);
    }
    prev_result_mbps_ = result_mbps;
}

RateStats SessionRateTracker::stats() const {
    return compute_rate_stats(rates_);
}
