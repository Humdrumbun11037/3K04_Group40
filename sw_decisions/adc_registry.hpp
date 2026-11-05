//adc registry

/*
Hides: implementation of the adc registry, add, remove, and relocate functions

Sends: registry changes to report_generation
Receives: administrator inputs from the dashboard module
*/
#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class EventRecorder;

struct ADC {
    int id;
    std::string location;
};

class ADClist {
private:
    std::unordered_map<int, ADC> ADCs_;
    EventRecorder* recorder_;   // optional; registry changes are logged if set

    void log(const std::string& action, const ADC& adc, const std::string& by);

public:
    explicit ADClist(EventRecorder* recorder = nullptr) : recorder_(recorder) {}

    enum class Result { Ok, DuplicateId, NotFound, InvalidId, InvalidLocation };

    // function for adding ADC (id must be positive and unused)
    Result addADC(int id, const std::string& location, const std::string& byUser = "");
    // function for removing ADC
    Result removeADC(int id, const std::string& byUser = "");
    // function for relocating ADC
    Result relocateADC(int id, const std::string& newLocation, const std::string& byUser = "");

    bool exists(int id) const;
    std::optional<ADC> find(int id) const;
    std::vector<ADC> all() const;   // sorted by id

    static std::string describe(Result result);
};
