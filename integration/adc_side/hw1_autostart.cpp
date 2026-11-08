//HW-1 transceiver, networked: set up before main()

// Linked into adc_hms_link only. Puts in the console hooks before Subgroup A's
// main() (unchanged) runs; the cabinet selection and the link start at its
// first output (see hw1_transceiver_net.cpp). Tests leave this file out and
// call hw1_net::start() themselves.

#include <iostream>

#include "hw1_net.hpp"

namespace {
const bool prepared = (hw1_net::prepare(), true);
}
