//report sender (interface only)

/*
Owned by whoever delivers reports (dashboards or messaging; to be agreed). This
file only states what report types needs: a way to hand over a finished daily
report and learn whether it was delivered. The sender gets the report data and
uses report formatting to lay it out.

Interface
takes: a daily report
sends: whether it was delivered
*/
#pragma once

struct DailyReport;   // defined in behaviour_hiding/report_types.hpp

namespace hms::behaviour {

enum class SendResult { Sent, SendFailure };

class ReportSender {
public:
    virtual ~ReportSender() = default;

    // { Pre: none }
    //   send(report)
    // { Post: Sent        => the report was delivered
    //         SendFailure => it was not delivered }
    virtual SendResult send(const DailyReport& report) = 0;
};

}
