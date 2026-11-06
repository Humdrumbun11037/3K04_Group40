// Tests for alarm severity: one test per row of the FR-ALM-03 table,
// plus the rejection of an unknown type.
#include <cassert>
#include <cstdio>

#include "behaviour_hiding/alarm_severity.hpp"

static void row1_unauthorized_access_is_critical() {
    AlarmSeverity table;
    SeverityResult r = table.severityOf(AlarmType::UnauthorizedAccess);
    assert(r.verdict == SeverityVerdict::Ok);
    assert(r.severity == Severity::Critical);
}

static void row2_adc_offline_is_critical() {
    AlarmSeverity table;
    SeverityResult r = table.severityOf(AlarmType::AdcOffline);
    assert(r.verdict == SeverityVerdict::Ok);
    assert(r.severity == Severity::Critical);
}

static void row3_incorrect_message_format_is_critical() {
    AlarmSeverity table;
    SeverityResult r = table.severityOf(AlarmType::IncorrectMessageFormat);
    assert(r.verdict == SeverityVerdict::Ok);
    assert(r.severity == Severity::Critical);
}

static void row4_inventory_low_is_high() {
    AlarmSeverity table;
    SeverityResult r = table.severityOf(AlarmType::InventoryLow);
    assert(r.verdict == SeverityVerdict::Ok);
    assert(r.severity == Severity::High);
}

static void row5_connection_loss_is_low() {
    AlarmSeverity table;
    SeverityResult r = table.severityOf(AlarmType::ConnectionLoss);
    assert(r.verdict == SeverityVerdict::Ok);
    assert(r.severity == Severity::Low);
}

static void row6_hospital_low_stock_is_high() {
    AlarmSeverity table;
    SeverityResult r = table.severityOf(AlarmType::HospitalLowStock);
    assert(r.verdict == SeverityVerdict::Ok);
    assert(r.severity == Severity::High);
}

static void row7_inventory_discrepancy_is_high() {
    AlarmSeverity table;
    SeverityResult r = table.severityOf(AlarmType::InventoryDiscrepancy);
    assert(r.verdict == SeverityVerdict::Ok);
    assert(r.severity == Severity::High);
}

static void unknown_type_is_rejected() {
    AlarmSeverity table;
    SeverityResult r = table.severityOf(static_cast<AlarmType>(99));
    assert(r.verdict == SeverityVerdict::UnknownType);
}

int main() {
    row1_unauthorized_access_is_critical();
    row2_adc_offline_is_critical();
    row3_incorrect_message_format_is_critical();
    row4_inventory_low_is_high();
    row5_connection_loss_is_low();
    row6_hospital_low_stock_is_high();
    row7_inventory_discrepancy_is_high();
    unknown_type_is_rejected();
    std::printf("alarm_severity_test: 8 tests passed\n");
    return 0;
}
