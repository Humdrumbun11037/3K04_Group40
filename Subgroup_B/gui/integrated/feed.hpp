//integrated feed entry

/*
Hides: nothing much; the shape of one activity-feed line, shared by the
       integrated world and the adapters that report what they did
*/
#pragma once

#include <string>
#include <vector>

struct FeedEntry {
    long long time;
    std::string kind;   // "alarm", "escalation", "status", "report", "action", "message" or "scenario"
    std::string text;
};

using Feed = std::vector<FeedEntry>;
