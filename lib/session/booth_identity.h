#pragma once

#include "identity_profile.h"

#include <ostream>
#include <string>

bool apply_booth_identity_key(IdentityProfile& cfg,
                              const std::string& key,
                              const std::string& value);

void resolve_booth_display(IdentityProfile& out,
                           const IdentityProfile& port_cfg,
                           const IdentityProfile& global);

void apply_booth_display_rates(IdentityProfile& profile);

void write_booth_identity(std::ostream& file, const IdentityProfile& profile);
