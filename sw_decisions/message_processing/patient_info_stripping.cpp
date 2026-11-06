//patient info stripping

/*
Hides: how patient identifying information is recognised and removed from
       ADC messages

Receives: decrypted ADC messages from message_routing
Sends: the same messages with all patient information removed, back to
       message_routing
*/

#include "patient_info_stripping.hpp"

#include <algorithm>
#include <cctype>

namespace {

std::string upper(const std::string& s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

std::string trimLeft(const std::string& s) {
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    return s.substr(i);
}

} // namespace

PatientInfoStripper::PatientInfoStripper()
    : patientPrefixes_{"PT_"} {}

bool PatientInfoStripper::isPatientField(const std::string& field) const {
    // Case and leading whitespace are ignored so "pt_name=" or " PT_MRN="
    // cannot slip through.
    const std::string normalised = upper(trimLeft(field));
    for (const std::string& prefix : patientPrefixes_) {
        if (normalised.compare(0, prefix.size(), prefix) == 0) return true;
    }
    return false;
}

std::string PatientInfoStripper::strip(const std::string& message) const {
    std::string result;
    std::string field;
    bool first = true;

    auto keep = [&](const std::string& f) {
        if (isPatientField(f)) return;
        if (!first) result += '|';
        result += f;
        first = false;
    };

    for (char c : message) {
        if (c == '|') {
            keep(field);
            field.clear();
        } else {
            field += c;
        }
    }
    keep(field);

    // Overwrite the copy that held patient data before it is freed.
    std::fill(field.begin(), field.end(), '\0');
    return result;
}
