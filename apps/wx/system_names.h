#pragma once

inline const char* system_name_for_leg(int leg) {
    static const char* const kNames[] = {"Alice", "Bob", "Carol", "Dave"};
    if (leg >= 0 && leg < 4) {
        return kNames[leg];
    }
    return "System";
}
