//gui api

/*
Hides: the GUI's request names and their JSON answers: which module call each
       request becomes, how parameters are read, and how verdicts are reported

Interface
takes: one request: method ("GET" or "POST"), path (e.g. "/api/stock/withdraw")
       and its parameters (e.g. adc=1, med=Morphine, amount=5)
sends: an HTTP status and a JSON body. Every action answers
       {"ok": true|false, "message": "...", "verdict": "<the module's verdict>"}
uses: the GUI world (the real behaviour modules and the fakes)

It knows nothing about HTTP or sockets, so it can be tested on its own;
server_main.cpp only passes requests in and answers out.
Requests (all parameters are URL parameters):
  GET  /api/state                         everything the page shows
  GET  /api/history   hours adc med actor kind
  POST /api/role      role=pharmacist|technician|administrator
  POST /api/restart | /api/scenario name=offline|maintenance|lowstock|discrepancy|escalation|daily
  POST /api/advance   seconds | /api/advance-to-six
  POST /api/heartbeat | /api/silence | /api/resume | /api/adc/add | /api/adc/decommission   adc
  POST /api/maintenance/schedule  adc startMin lengthMin | /api/maintenance/cancel  adc start
  POST /api/alarm/raise  adc type detail | /api/alarm/ack  id | /api/alarm/resolve  id note
  POST /api/stock/withdraw | /api/stock/deposit  adc med amount
  POST /api/stock/set  adc med count explanation | /api/reserve/set  med count explanation
  POST /api/report/daily | /api/report/summary  hours adc med actor
  POST /api/fail/history | /api/fail/sender  on=1|0
*/
#pragma once

#include <map>
#include <memory>
#include <string>

#include "gui/hms_world.hpp"

struct ApiRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> params;
};

struct ApiResponse {
    int status;          // 200, or 400 bad parameters, 404 unknown request, 405 wrong method
    std::string body;    // JSON
};

class HmsApi {
public:
    // Starts with a fresh world (the same starting state as the console demo).
    HmsApi();

    // Runs one request against the world and returns its JSON answer.
    ApiResponse handle(const ApiRequest& request);

    GuiWorld& world() { return *current; }

private:
    ApiResponse state();
    ApiResponse historySearch(const ApiRequest& request);
    ApiResponse action(const ApiRequest& request);
    ApiResponse scenario(const std::string& name);

    std::unique_ptr<GuiWorld> current;
};
