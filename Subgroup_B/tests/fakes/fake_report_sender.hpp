//fake report sender

/*
Keeps every report it was sent instead of delivering it.
failSends(true) makes every send() fail until set back to false.
*/
#pragma once

#include <vector>

#include "behaviour_hiding/report_types.hpp"
#include "behaviour_hiding/interfaces/report_sender.hpp"

class FakeReportSender : public hms::behaviour::ReportSender {
public:
    hms::behaviour::SendResult send(const DailyReport& report) override {
        if (sendsFail) return hms::behaviour::SendResult::SendFailure;
        sent.push_back(report);
        return hms::behaviour::SendResult::Sent;
    }

    void failSends(bool fail) { sendsFail = fail; }

    std::vector<DailyReport> sent;

private:
    bool sendsFail = false;
};
