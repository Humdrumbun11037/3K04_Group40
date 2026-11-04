//adc id (shared type only)

/*
How an ADC is named across the HMS, e.g. "ADC-1". Owned by the ADC registry;
defined here until the team agrees on it.
*/
#pragma once

#include <string>

namespace hms::behaviour {

using AdcId = std::string;

}
