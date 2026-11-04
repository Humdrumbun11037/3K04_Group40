//adc registry

/*
Hides: implementation of the adc registry, add, remove, and relocate functions

Sends: registry changes to report_generation
Receives: administrator inputs from the dashboard module
*/

#include <algorithm>
#include <cctype>
#include "adc_registry.hpp"
#include "report_generation.hpp"

namespace {

bool isBlank(const std::string& s) {
    return std::all_of(s.begin(), s.end(),
                       [](unsigned char c) { return std::isspace(c) != 0; });
}

} // namespace

ADClist::Result ADClist::addADC(int id, const std::string& location, const std::string& byUser) {
    if (id <= 0) return Result::InvalidId;
    if (isBlank(location)) return Result::InvalidLocation;
    if (ADCs_.find(id) != ADCs_.end()) return Result::DuplicateId;

    ADC adc{id, location};
    ADCs_[id] = adc;
    log("registered", adc, byUser);
    return Result::Ok;
}

ADClist::Result ADClist::removeADC(int id, const std::string& byUser) {
    auto it = ADCs_.find(id);
    if (it == ADCs_.end()) return Result::NotFound;

    ADC removed = it->second;
    ADCs_.erase(it);
    log("removed", removed, byUser);
    return Result::Ok;
}

ADClist::Result ADClist::relocateADC(int id, const std::string& newLocation, const std::string& byUser) {
    if (isBlank(newLocation)) return Result::InvalidLocation;
    auto it = ADCs_.find(id);
    if (it == ADCs_.end()) return Result::NotFound;

    const std::string oldLocation = it->second.location;
    it->second.location = newLocation;
    log("relocated from " + oldLocation, it->second, byUser);
    return Result::Ok;
}

bool ADClist::exists(int id) const {
    return ADCs_.find(id) != ADCs_.end();
}

std::optional<ADC> ADClist::find(int id) const {
    auto it = ADCs_.find(id);
    if (it == ADCs_.end()) return std::nullopt;
    return it->second;
}

std::vector<ADC> ADClist::all() const {
    std::vector<ADC> list;
    list.reserve(ADCs_.size());
    for (const auto& entry : ADCs_) list.push_back(entry.second);
    std::sort(list.begin(), list.end(), [](const ADC& a, const ADC& b) { return a.id < b.id; });
    return list;
}

std::string ADClist::describe(Result result) {
    switch (result) {
        case Result::Ok:              return "Done.";
        case Result::DuplicateId:     return "An ADC with that id is already registered.";
        case Result::NotFound:        return "No ADC with that id is registered.";
        case Result::InvalidId:       return "ADC ids must be positive whole numbers.";
        case Result::InvalidLocation: return "A location must be given.";
    }
    return "";
}

void ADClist::log(const std::string& action, const ADC& adc, const std::string& by) {
    if (recorder_ == nullptr) return;
    Event event;
    event.type = EventType::AdcRegistry;
    event.adcId = adc.id;   // timestamp is filled in by report_generation
    event.fields["action"] = action;
    event.fields["location"] = adc.location;
    event.fields["user"] = by;
    recorder_->record(event);
}
