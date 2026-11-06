//integrated: conversions between the two sides' shared types

/*
Hides: how the two sides' basic types map onto each other
  - time:   Andrew: long long seconds; Inodh and Matthew: "YYYY-MM-DD HH:MM:SS"
  - no ADC: Andrew: 0; Inodh: -1
  - roles:  Andrew's Role and Inodh's UserRole have the same names but a
            different order, so they are mapped by name, never cast
Both time conversions work on the calendar with no time zone, so they are
exact inverses (formatTime and timeutil::toSeconds).
*/
#pragma once

#include "gui/integrated/sw_names.hpp"

#include <string>

#include "behaviour_hiding/interfaces/adc_id.hpp"
#include "behaviour_hiding/interfaces/user.hpp"
#include "behaviour_hiding/report_formatting.hpp"

namespace convert {

inline int toSwAdc(hms::behaviour::AdcId adc) { return adc == 0 ? -1 : adc; }
inline hms::behaviour::AdcId fromSwAdc(int adcId) { return adcId < 0 ? 0 : adcId; }

inline std::string toText(long long seconds) { return ReportFormatting().formatTime(seconds); }
inline bool fromText(const std::string& text, long long& seconds) { return timeutil::toSeconds(text, seconds); }

inline hms::behaviour::Role toRole(UserRole role) {
    switch (role) {
        case UserRole::Technician:    return hms::behaviour::Role::Technician;
        case UserRole::Pharmacist:    return hms::behaviour::Role::Pharmacist;
        case UserRole::Administrator: return hms::behaviour::Role::Administrator;
    }
    return hms::behaviour::Role::Technician;   // unreachable; least privileged
}

inline UserRole toUserRole(hms::behaviour::Role role) {
    switch (role) {
        case hms::behaviour::Role::Technician:    return UserRole::Technician;
        case hms::behaviour::Role::Pharmacist:    return UserRole::Pharmacist;
        case hms::behaviour::Role::Administrator: return UserRole::Administrator;
    }
    return UserRole::Technician;
}

}  // namespace convert
