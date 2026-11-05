//adc id (shared type only)

/*
How an ADC is identified across the HMS: a positive whole number, as in the
ADC registry. 0 is never a real ADC, so it can mean "no ADC" in records and
"any ADC" in filters. An alarm or record for the whole hospital (not one ADC)
uses 0, and is shown as "Hospital". Owned by the ADC registry; defined here until agreed.
*/
#pragma once

#include <string>

namespace hms::behaviour {

using AdcId = int;

// How an ADC is named wherever it is shown to people, e.g. "ADC-3",
// or "Hospital" for 0 (the whole hospital).
inline std::string adcName(AdcId adc) {
    if (adc == 0) return "Hospital";
    return "ADC-" + std::to_string(adc);
}

}
