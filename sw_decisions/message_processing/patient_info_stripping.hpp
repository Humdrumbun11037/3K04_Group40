//patient info stripping

/*
Hides: how patient identifying information is recognised and removed from
       ADC messages

Receives: decrypted ADC messages from message_routing
Sends: the same messages with all patient information removed, back to
       message_routing

Patient information is carried in fields whose name starts with "PT_",
e.g. "PT_NAME=Jane Doe", "PT_MRN=00012345", "PT_DOB=1980-01-01".
*/
#pragma once

#include <string>
#include <vector>

class PatientInfoStripper {
public:
    PatientInfoStripper();

    // Message fields are separated by '|'. Every field that holds patient
    // information is dropped; all other fields keep their order.
    std::string strip(const std::string& message) const;

    // True if the field holds patient information.
    bool isPatientField(const std::string& field) const;

private:
    std::vector<std::string> patientPrefixes_;
};
