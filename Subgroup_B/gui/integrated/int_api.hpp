//integrated api

/*
Hides: the integrated page's request names and their JSON answers, and which
       real module (Andrew's, Inodh's or Matthew's) each request reaches

Interface
takes: one request (method, path, URL parameters)
sends: an HTTP status and a JSON body; actions answer
       {"ok": true|false, "message": "...", "verdict": "<the module's own verdict>"}
uses: the integrated world

Same shape as the simulation-mode API (gui/api.*), which is unchanged, except
that the role switch is replaced by a real login (Inodh's authorization), plus:
  POST /api/login  user password | /api/logout
  POST /api/adc/register  id location | /api/adc/relocate  id location | /api/adc/remove  id
  POST /api/users/add  user password role | /api/users/remove  user | /api/users/unlock  user
  POST /api/message/send  text   (a raw ADC message, e.g. 1|Withdrawal|Morphine|5|U1|PT_NAME=Jane)
Withdrawals and deposits (/api/stock/withdraw, /api/stock/deposit) are ADC
uploads: they travel as messages through Inodh's encryption, patient info
stripping, event parsing and router before reaching inventory record.
User actions need a live session (401 otherwise). Simulation controls (clock,
simulated ADCs, scenarios, restart, failure switches) work without one.
*/
#pragma once

#include "gui/integrated/sw_names.hpp"

#include <map>
#include <memory>
#include <string>

#include "gui/integrated/int_world.hpp"

struct IntRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> params;
};

struct IntResponse {
    int status;          // 200; 400 bad parameters; 401 not logged in; 404 unknown; 405 wrong method
    std::string body;    // JSON
};

class IntegratedApi {
public:
    // dataDir holds the encrypted audit trail and the key (gui/data when run normally).
    explicit IntegratedApi(const std::string& dataDir = "data");

    // Runs one request against the integrated world and returns its JSON answer.
    IntResponse handle(const IntRequest& request);

    IntWorld& world() { return *current; }

private:
    IntResponse state();
    IntResponse historySearch(const IntRequest& request);
    IntResponse action(const IntRequest& request);
    IntResponse scenario(const std::string& name);

    IntResponse restartWorld();

    std::string dataDir;
    std::unique_ptr<IntWorld> current;
};
