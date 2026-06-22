#pragma once

/** Preset used when demo mode is enabled in settings (UI only). */
inline constexpr double kDemoDisplayPresetMibS = 7168.0;
inline constexpr double kDemoDisplayPresetJitterPct = 3.0;

// Effective MiB/s for one demo transfer run. roll01 must be in [0, 1].
double demo_display_mib_s_with_jitter(double base_mib_s, double jitter_pct, double roll01);

// Random roll in ±jitter_pct around base_mib_s (e.g. 3 => 0.97..1.03 × base).
double roll_demo_display_mib_s(double base_mib_s, double jitter_pct);
