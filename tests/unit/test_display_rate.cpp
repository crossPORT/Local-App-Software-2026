#include "test_util.h"

#include "display_rate.h"

RB_TEST(display_rate_jitter_bounds) {
    const double base = 7168.0;
    const double pct = 3.0;
    const double low = display_rate_mib_s_with_jitter(base, pct, 0.0);
    const double high = display_rate_mib_s_with_jitter(base, pct, 1.0);
    CHECK(low > 6952.0 && low < 6953.0);
    CHECK(high > 7382.0 && high < 7384.0);
    CHECK(display_rate_mib_s_with_jitter(base, pct, 0.5) > 7167.0
          && display_rate_mib_s_with_jitter(base, pct, 0.5) < 7169.0);
}

RB_TEST(display_rate_jitter_zero_pct_is_base) {
    CHECK(display_rate_mib_s_with_jitter(7168.0, 0.0, 0.42) > 7167.0
          && display_rate_mib_s_with_jitter(7168.0, 0.0, 0.42) < 7169.0);
}

RB_TEST(display_rate_jitter_zero_base_is_zero) {
    CHECK_EQ(display_rate_mib_s_with_jitter(0.0, 3.0, 0.5), 0.0);
}
