//integrated: teammates' headers under safe names

/*
Hides: the name clash between Inodh's code and Andrew's. Both define global
       types called AlarmSeverity (his: an enum; Andrew's: a class) and
       ReportTypes (his: an interface; Andrew's: a class).

How: gui/makefile compiles Inodh's and Matthew's .cpp files with
     -DAlarmSeverity=SwAlarmSeverity -DReportTypes=SwReportTypes, so in their
     code those two names become SwAlarmSeverity and SwReportTypes. This header
     does the same for their headers, then removes the two macros so Andrew's
     headers keep the real names. No teammate file is edited.

Rule: in every integrated file, include this header FIRST, before any
behaviour_hiding header, and never include a teammate's header directly.
Never write "using namespace hms::behaviour" in these files either: Inodh has
global Clock and AlarmNotifier classes with the same short names.
*/
#pragma once

#define AlarmSeverity SwAlarmSeverity
#define ReportTypes SwReportTypes

#include "external_interfaces.hpp"
#include "adc_registry.hpp"
#include "report_generation.hpp"
#include "alarm_notification.hpp"
#include "user_information/user_registry.hpp"
#include "user_information/user_settings.hpp"
#include "startup/authorization.hpp"
#include "startup/encryption_key_storage.hpp"
#include "message_processing/encryption.hpp"
#include "message_processing/patient_info_stripping.hpp"
#include "events_logs/event_contents.hpp"
#include "events_logs/message_routing.hpp"
#include "hardware_hiding/long_term_storage.hpp"

#undef AlarmSeverity
#undef ReportTypes
