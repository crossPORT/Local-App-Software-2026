#include "booth_identity.h"

#include "booth_display.h"

namespace {

bool parse_bool_flag(const std::string& value, bool* out) {
    if (value == "1" || value == "true" || value == "yes" || value == "on") {
        *out = true;
        return true;
    }
    if (value == "0" || value == "false" || value == "no" || value == "off") {
        *out = false;
        return true;
    }
    return false;
}

}  // namespace

bool apply_booth_identity_key(IdentityProfile& cfg,
                              const std::string& key,
                              const std::string& value) {
    if (key != "booth_display_enabled") {
        return false;
    }
    bool parsed = false;
    if (parse_bool_flag(value, &parsed)) {
        cfg.booth_display_enabled = parsed;
        cfg.booth_display_enabled_explicit = true;
    }
    return true;
}

void apply_booth_display_rates(IdentityProfile& profile) {
    if (profile.booth_display_enabled) {
        profile.booth_display_mib_s = kBoothDisplayPresetMibS;
        profile.booth_display_jitter_pct = kBoothDisplayPresetJitterPct;
        return;
    }
    profile.booth_display_mib_s = 0.0;
    profile.booth_display_jitter_pct = 0.0;
}

void resolve_booth_display(IdentityProfile& out,
                           const IdentityProfile& port_cfg,
                           const IdentityProfile& global) {
    out.booth_display_mib_s = port_cfg.booth_display_mib_s > 0.0
                                 ? port_cfg.booth_display_mib_s
                                 : global.booth_display_mib_s;
    out.booth_display_jitter_pct = port_cfg.booth_display_jitter_pct > 0.0
                                      ? port_cfg.booth_display_jitter_pct
                                      : global.booth_display_jitter_pct;
    if (port_cfg.booth_display_enabled_explicit) {
        out.booth_display_enabled = port_cfg.booth_display_enabled;
    } else if (global.booth_display_enabled_explicit) {
        out.booth_display_enabled = global.booth_display_enabled;
    } else {
        out.booth_display_enabled = out.booth_display_mib_s > 0.0;
    }
    apply_booth_display_rates(out);
}

void write_booth_identity(std::ostream& file, const IdentityProfile& profile) {
    file << "booth_display_enabled=" << (profile.booth_display_enabled ? "true" : "false")
         << '\n';
    if (!profile.booth_display_enabled) {
        return;
    }
    file << "booth_display_mib_s=" << kBoothDisplayPresetMibS << '\n';
    file << "booth_display_jitter_pct=" << kBoothDisplayPresetJitterPct << '\n';
}
