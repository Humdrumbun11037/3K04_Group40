# Integration build: Subgroup A's ADC and Subgroup B's HMS joined by the
# adapters in this folder. Run make from this folder (integration/):
#   mingw32-make test        build and run every integration test
#   mingw32-make             build the demo programs (hms_live, adc_hms_link, adc_stub)
#   mingw32-make clean       delete build/
# Every object and program goes into build/. Nothing is written into
# Subgroup_A/, Subgroup_B/ or sw_decisions/, whose files are compiled as they are.

CXX = g++
SA = ../Subgroup_A
SB = ../Subgroup_B
SW = ../sw_decisions
BUILD = build
.DEFAULT_GOAL := all
# A failed link must not leave a half-written program that looks up to date.
.DELETE_ON_ERROR:

CXXFLAGS = -std=c++17 -Wall -Wextra -MMD -MP -I.
# Same flags Subgroup A's own makefile uses for its files.
ADC_FLAGS = $(CXXFLAGS) -I$(SA)/inc

ifeq ($(OS),Windows_NT)
EXE = .exe
# cpp-httplib on MinGW needs a Windows 10 target and Winsock; -static as in
# Subgroup_B/gui/makefile (an older libstdc++-6.dll on PATH kills the program).
NET_FLAGS = -D_WIN32_WINNT=0x0A00
NET_LIBS = -lws2_32
LINK = -static
else
EXE =
NET_FLAGS =
NET_LIBS = -pthread
LINK = -pthread
endif

# ---- Subgroup A: every source file, as it is ----
# Its two hardware stubs (HW-0 drawers, HW-1 transceiver) are kept apart: the
# integrated ADC swaps them for adc_side/hw0_drawer_sim.cpp and hw1_transceiver_net.cpp.
ADC_LIB_SOURCES = $(filter-out $(SA)/src/main.cpp $(SA)/src/HW0_DrawerController.cpp $(SA)/src/HW1_Transceiver.cpp,$(wildcard $(SA)/src/*.cpp))
ADC_LIB_OBJECTS = $(patsubst $(SA)/src/%.cpp,$(BUILD)/adc/%.o,$(ADC_LIB_SOURCES))
ADC_MAIN_OBJECT = $(BUILD)/adc/main.o
ADC_STUB_HW0_OBJECT = $(BUILD)/adc/HW0_DrawerController.o   # Subgroup A's own drawer stub, for tests
ADC_STUB_HW1_OBJECT = $(BUILD)/adc/HW1_Transceiver.o        # Subgroup A's own (printing) transceiver, for tests
ADC_SIM_HW0_OBJECT = $(BUILD)/adc_side/hw0_drawer_sim.o
ADC_NET_HW1_OBJECTS = $(BUILD)/adc_side/hw1_transceiver_net.o $(BUILD)/adc_side/cabinet.o $(BUILD)/adc_side/console_input.o

$(BUILD)/adc/%.o: $(SA)/src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(ADC_FLAGS) -c $< -o $@

# ---- Subgroup B's integrated HMS, unchanged ----
# The same files and the same name-clash fix as Subgroup_B/gui/makefile
# ("Integrated mode"): Inodh's and Matthew's files are compiled with
# AlarmSeverity and ReportTypes renamed; gui/integrated/sw_names.hpp does the
# same for their headers.
HMS_FLAGS = $(CXXFLAGS) -I$(SB) -I$(SW)
SW_RENAME = -DAlarmSeverity=SwAlarmSeverity -DReportTypes=SwReportTypes
SW_SOURCES = adc_registry.cpp alarm_notification.cpp user_information/user_registry.cpp \
             user_information/user_settings.cpp startup/authorization.cpp \
             startup/encryption_key_storage.cpp message_processing/encryption.cpp \
             message_processing/patient_info_stripping.cpp events_logs/event_contents.cpp \
             events_logs/message_routing.cpp
SW_OBJECTS = $(patsubst %.cpp,$(BUILD)/hms/sw/%.o,$(SW_SOURCES))
BEHAVIOUR_OBJECTS = $(patsubst $(SB)/behaviour_hiding/%.cpp,$(BUILD)/hms/behaviour_hiding/%.o,$(wildcard $(SB)/behaviour_hiding/*.cpp))
HW_OBJECTS = $(BUILD)/hms/hw/long_term_storage.o
INT_SOURCES = $(filter-out $(SB)/gui/integrated/int_server_main.cpp,$(wildcard $(SB)/gui/integrated/*.cpp $(SB)/gui/integrated/adapters/*.cpp))
INT_OBJECTS = $(patsubst $(SB)/gui/integrated/%.cpp,$(BUILD)/hms/integrated/%.o,$(INT_SOURCES))
HMS_OBJECTS = $(BEHAVIOUR_OBJECTS) $(SW_OBJECTS) $(HW_OBJECTS) $(INT_OBJECTS)
ENCRYPTION_OBJECT = $(BUILD)/hms/sw/message_processing/encryption.o   # also used by the ADC side

$(BUILD)/hms/sw/%.o: $(SW)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(HMS_FLAGS) $(SW_RENAME) -c $< -o $@

$(BUILD)/hms/hw/%.o: $(SB)/hardware_hiding/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(HMS_FLAGS) $(SW_RENAME) -c $< -o $@

$(BUILD)/hms/behaviour_hiding/%.o: $(SB)/behaviour_hiding/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(HMS_FLAGS) -c $< -o $@

$(BUILD)/hms/integrated/%.o: $(SB)/gui/integrated/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(HMS_FLAGS) -c $< -o $@

# ---- Adapters in this folder ----
TABLE_OBJECT = $(BUILD)/adc_side/adc_output_table.o
LIVE_OBJECT = $(BUILD)/hms_side/live_adc.o

$(BUILD)/adc_side/%.o: adc_side/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(ADC_FLAGS) -c $< -o $@

$(BUILD)/hms_side/%.o: hms_side/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(HMS_FLAGS) -c $< -o $@

# ---- Network pieces (cpp-httplib) ----
HTTPLIB_DIR = $(SB)/third_party
NET_ADC_FLAGS = $(ADC_FLAGS) -I$(SW) -I$(HTTPLIB_DIR) $(NET_FLAGS)
CLIENT_OBJECT = $(BUILD)/net/hms_client.o
LINK_OBJECTS = $(BUILD)/adc_side/adc_link.o $(BUILD)/adc_side/console_observer.o $(CLIENT_OBJECT) $(ENCRYPTION_OBJECT)

$(CLIENT_OBJECT): adc_side/hms_client.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(NET_ADC_FLAGS) -c $< -o $@

$(BUILD)/net/%.o: tools/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(NET_ADC_FLAGS) -c $< -o $@

$(BUILD)/net/hms_live_main.o: hms_side/hms_live_main.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(HMS_FLAGS) $(NET_FLAGS) -c $< -o $@

$(BUILD)/net/%_test.o: tests/%_test.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(NET_ADC_FLAGS) -c $< -o $@

# ---- Tests ----
TABLE_TEST = $(BUILD)/adc_output_table_test$(EXE)
LIVE_TEST = $(BUILD)/live_adc_test$(EXE)
LINK_TEST = $(BUILD)/adc_link_test$(EXE)
PING_TEST = $(BUILD)/adc_ping_test$(EXE)
DRAWER_TEST = $(BUILD)/hw0_drawer_test$(EXE)
CABINET_TEST = $(BUILD)/cabinet_test$(EXE)

# The demo cabinet selection against Subgroup A's real stock database (SW1).
$(CABINET_TEST): tests/cabinet_test.cpp $(ADC_LIB_OBJECTS) $(ADC_STUB_HW0_OBJECT) $(ADC_STUB_HW1_OBJECT) \
                 $(BUILD)/adc_side/cabinet.o $(BUILD)/adc_side/console_input.o
	$(CXX) $(ADC_FLAGS) $^ -o $@ $(LINK)

$(TABLE_TEST): tests/adc_output_table_test.cpp $(TABLE_OBJECT) $(ADC_LIB_OBJECTS) $(ADC_STUB_HW0_OBJECT) $(ADC_STUB_HW1_OBJECT)
	$(CXX) $(ADC_FLAGS) $^ -o $@ $(LINK)

$(LIVE_TEST): tests/live_adc_test.cpp $(LIVE_OBJECT) $(TABLE_OBJECT) $(HMS_OBJECTS)
	$(CXX) $(HMS_FLAGS) $^ -o $@ $(LINK)

$(LINK_TEST): $(BUILD)/net/adc_link_test.o $(LINK_OBJECTS)
	$(CXX) $^ -o $@ $(NET_LIBS) $(LINK)

# Subgroup A's real BH0 (Ping) and BH3 with the networked transceiver.
$(PING_TEST): $(BUILD)/net/adc_ping_test.o $(ADC_LIB_OBJECTS) $(ADC_STUB_HW0_OBJECT) $(ADC_NET_HW1_OBJECTS) $(TABLE_OBJECT) $(LINK_OBJECTS)
	$(CXX) $^ -o $@ $(NET_LIBS) $(LINK)

# Subgroup A's real BH3 and BH5 with the simulated drawers (about 35 s).
$(DRAWER_TEST): tests/hw0_drawer_test.cpp $(ADC_LIB_OBJECTS) $(ADC_SIM_HW0_OBJECT) $(ADC_STUB_HW1_OBJECT) $(TABLE_OBJECT) $(BUILD)/adc_side/console_observer.o
	$(CXX) $(ADC_FLAGS) $^ -o $@ $(LINK)

TESTS = $(TABLE_TEST) $(LIVE_TEST) $(LINK_TEST) $(PING_TEST) $(CABINET_TEST) $(DRAWER_TEST)

test: $(TESTS)
	@for t in $(TESTS); do ./$$t || exit 1; done

# ---- Programs ----
# hms_live      the HMS (Subgroup B, unchanged) with the ADC endpoint
# adc_hms_link  the ADC (Subgroup A) with its hardware stubs swapped:
#               HW1_Transceiver.cpp -> adc_side/hw1_transceiver_net.cpp (+ hw1_autostart.cpp),
#               HW0_DrawerController.cpp -> adc_side/hw0_drawer_sim.cpp
# adc_stub      a stand-in ADC (tests and demo backup)
HMS_LIVE = $(BUILD)/hms_live$(EXE)
ADC_LINKED = $(BUILD)/adc_hms_link$(EXE)
ADC_STUB = $(BUILD)/adc_stub$(EXE)

$(HMS_LIVE): $(BUILD)/net/hms_live_main.o $(LIVE_OBJECT) $(TABLE_OBJECT) $(HMS_OBJECTS)
	$(CXX) $^ -o $@ $(NET_LIBS) $(LINK)

$(ADC_LINKED): $(ADC_MAIN_OBJECT) $(ADC_LIB_OBJECTS) $(ADC_SIM_HW0_OBJECT) $(ADC_NET_HW1_OBJECTS) \
               $(BUILD)/adc_side/hw1_autostart.o $(TABLE_OBJECT) $(LINK_OBJECTS)
	$(CXX) $^ -o $@ $(NET_LIBS) $(LINK)

$(ADC_STUB): $(BUILD)/net/adc_stub.o $(CLIENT_OBJECT) $(ENCRYPTION_OBJECT)
	$(CXX) $^ -o $@ $(NET_LIBS) $(LINK)

PROGRAMS = $(HMS_LIVE) $(ADC_LINKED) $(ADC_STUB)

all: $(TESTS) $(PROGRAMS)

# ---- Running ----
e2e: $(PROGRAMS)
	bash tests/e2e.sh

run-hms: $(HMS_LIVE)
	./$(HMS_LIVE)

run-adc: $(ADC_LINKED)
	./$(ADC_LINKED)

# ---- Regression: both parts, as their owners build them, still pass ----
# Subgroup A: its own sources and flags (as Subgroup_A/makefile), its built-in
# test suite (menu option 2). Subgroup B: its behaviour tests (as
# Subgroup_B/makefile) and its GUI API and integrated tests (as
# Subgroup_B/gui/makefile). All built and run in build/regress/.
REGRESS = $(BUILD)/regress
ADC_STANDALONE = $(REGRESS)/adc_system$(EXE)
B_TESTS = $(patsubst $(SB)/tests/%.cpp,$(REGRESS)/%$(EXE),$(wildcard $(SB)/tests/*.cpp))
GUI_SIM_OBJECTS = $(BUILD)/hms/gui/hms_world.o $(BUILD)/hms/gui/api.o

$(ADC_STANDALONE): $(wildcard $(SA)/src/*.cpp) $(wildcard $(SA)/inc/*.hpp)
	@mkdir -p $(dir $@)
	$(CXX) -std=c++17 -Wall -Wextra -I$(SA)/inc $(wildcard $(SA)/src/*.cpp) -o $@ $(LINK)

$(REGRESS)/%$(EXE): $(SB)/tests/%.cpp $(BEHAVIOUR_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I$(SB) $< $(BEHAVIOUR_OBJECTS) -o $@ $(LINK)

$(BUILD)/hms/gui/%.o: $(SB)/gui/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I$(SB) -c $< -o $@

$(REGRESS)/api_test$(EXE): $(SB)/gui/tests/api_test.cpp $(BEHAVIOUR_OBJECTS) $(GUI_SIM_OBJECTS)
	$(CXX) $(CXXFLAGS) -I$(SB) $^ -o $@ $(LINK)

$(REGRESS)/integrated_test$(EXE): $(SB)/gui/tests/integrated_test.cpp $(HMS_OBJECTS)
	$(CXX) $(HMS_FLAGS) $^ -o $@ $(LINK)

regression: $(ADC_STANDALONE) $(B_TESTS) $(REGRESS)/api_test$(EXE) $(REGRESS)/integrated_test$(EXE)
	@printf '2\n3\n' | ./$(ADC_STANDALONE) | grep -q "TESTS COMPLETE" && echo "Subgroup A: built standalone; its test suite ran to completion"
	@for t in $(B_TESTS); do ./$$t > /dev/null || { echo "Subgroup B FAILED: $$t"; exit 1; }; done; echo "Subgroup B: $(words $(B_TESTS)) behaviour test programs pass"
	@cd $(REGRESS) && ./api_test$(EXE) && ./integrated_test$(EXE)

clean:
	rm -rf $(BUILD)

-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)

.PHONY: all test clean e2e run-hms run-adc regression
