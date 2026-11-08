//ADC output table

/*
Hides: which lines the ADC (Subgroup A) prints mean something to the HMS, and
       which HMS message each one becomes. This is the ONLY place the
       integration depends on the ADC's print strings; tests/adc_output_table_test
       runs Subgroup A's real code and fails if any of those strings drift.

Interface
takes: one complete line the ADC printed (no newline), and the ADC's HMS id
sends: the plain-text HMS message for it, in Inodh's format
       "<adcId>|<keyword>|<fields...>" (sw_decisions/events_logs/event_formatting.h),
       or nothing if the line means nothing to the HMS
uses: nothing
*/
#pragma once

#include <optional>
#include <string>

namespace adc_output {

std::optional<std::string> translate(int adcId, const std::string& line);

// "|" separates message fields, so it never appears inside one.
std::string field(const std::string& text);

// The message with the ADC's real time added as a last field,
// "...|ADC_TIME=YYYY-MM-DD HH:MM:SS". Inodh's parser reads only the fields its
// rule names and ignores extra ones, so the HMS's own handling is unchanged.
std::string stamp(const std::string& message, const std::string& timeText);

// The ADC_TIME of a stamped message, or "" if it has none.
std::string stampOf(const std::string& message);

}  // namespace adc_output
