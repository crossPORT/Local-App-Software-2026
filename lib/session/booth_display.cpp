#include "booth_display.h"

#include <random>

double booth_display_mib_s_with_jitter(double base_mib_s, double jitter_pct, double roll01) {
    if (base_mib_s <= 0.0) {
        return 0.0;
    }
    if (jitter_pct <= 0.0) {
        return base_mib_s;
    }
    if (roll01 < 0.0) {
        roll01 = 0.0;
    } else if (roll01 > 1.0) {
        roll01 = 1.0;
    }
    const double span = jitter_pct / 100.0;
    const double factor = 1.0 + (roll01 * 2.0 - 1.0) * span;
    return base_mib_s * factor;
}

double roll_booth_display_mib_s(double base_mib_s, double jitter_pct) {
    if (base_mib_s <= 0.0) {
        return 0.0;
    }
    if (jitter_pct <= 0.0) {
        return base_mib_s;
    }
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return booth_display_mib_s_with_jitter(base_mib_s, jitter_pct, dist(rng));
}
