//HW-1 transceiver, networked: start-up and inspection

/*
For hw1_transceiver_net.cpp. adc_hms_link calls prepare() before main()
(hw1_autostart.cpp); the cabinet selection and the link then start at Subgroup
A's first output. A test sets HMS_KEY / HMS_PORT / ADC_ID and calls start().
*/
#pragma once

class AdcLink;

namespace hw1_net {

// Reads the settings and puts in the console hooks; uses nothing of Subgroup A's.
void prepare();

// prepare(), then the cabinet selection and the link, now. Once only; later
// calls do nothing.
void start();

// The running link, or nullptr if there is no key (the ADC then runs on its own).
AdcLink* link();

// The ADC this screen is (demo cabinet selection).
int adcId();

}  // namespace hw1_net
