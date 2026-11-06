# GUI build for the HMS (Subgroup_B/gui). Kept separate from the shared
# Subgroup_B/makefile on purpose. Run make from this folder (Subgroup_B/gui):
#   mingw32-make test     build and run the API tests (no httplib needed)
#   mingw32-make          build the GUI server (needs ../third_party/httplib.h)
#   mingw32-make run      build and start it, then open http://127.0.0.1:8080
#   mingw32-make clean    delete gui/obj and gui/bin
# It compiles Andrew's behaviour_hiding sources itself, into gui/obj, so it
# never touches the shared obj/ and bin/ folders. Nobody else's code is used.

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I.. -MMD -MP
# cpp-httplib (0.65.0) on MinGW needs a Windows 10 target and Winsock. The
# servers are linked with a static C++ runtime (-static): otherwise Windows may
# load an older libstdc++-6.dll that comes first on PATH (e.g. /mingw64/bin) and
# the program dies at start (exit 127 / 0xC0000139).
SERVER_FLAGS = -D_WIN32_WINNT=0x0A00
SERVER_LIBS = -lws2_32 -static

OBJ_DIR = obj
BIN_DIR = bin
HTTPLIB = ../third_party/httplib.h

BEHAVIOUR_SOURCES = $(wildcard ../behaviour_hiding/*.cpp)
BEHAVIOUR_OBJECTS = $(patsubst ../%.cpp,$(OBJ_DIR)/%.o,$(BEHAVIOUR_SOURCES))
GUI_OBJECTS = $(OBJ_DIR)/gui/hms_world.o $(OBJ_DIR)/gui/api.o
OBJECTS = $(BEHAVIOUR_OBJECTS) $(GUI_OBJECTS)

SERVER = $(BIN_DIR)/hms_gui
API_TEST = $(BIN_DIR)/api_test

all: $(SERVER)

$(OBJ_DIR)/behaviour_hiding/%.o: ../behaviour_hiding/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/gui/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(API_TEST): tests/api_test.cpp $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< $(OBJECTS) -o $@

test: $(API_TEST)
	./$(API_TEST)

$(HTTPLIB):
	@echo "Missing $(HTTPLIB): put a pinned cpp-httplib header there (see ../handoff/GUI_PLAN.md)."
	@exit 1

$(SERVER): server_main.cpp $(HTTPLIB) $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(SERVER_FLAGS) $< $(OBJECTS) -o $@ $(SERVER_LIBS)

run: $(SERVER)
	./$(SERVER)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# ---- Integrated mode: Andrew's, Inodh's and Matthew's real code together ----
#   mingw32-make integrated-test   build and run the integrated API tests
#   mingw32-make integrated        build the integrated server (needs httplib.h)
#   mingw32-make run-integrated    build and start it, then open http://127.0.0.1:8081
# Name clash: Inodh's code and Andrew's both define global AlarmSeverity and
# ReportTypes. Inodh's and Matthew's files are compiled with the two names
# renamed (SW_RENAME) so the program has one of each; their files are not
# edited. gui/integrated/sw_names.hpp does the same for their headers.
# Check with: mingw32-make check-symbols (no symbol may be defined twice).

SW_DIR = ../../sw_decisions
SW_RENAME = -DAlarmSeverity=SwAlarmSeverity -DReportTypes=SwReportTypes
INT_FLAGS = $(CXXFLAGS) -I$(SW_DIR)
# Static runtime: the integrated programs then run from any shell, even when an
# older libstdc++-6.dll (e.g. /mingw64/bin) comes first on PATH.
INT_LINK = -static

# Only the teammate files the integrated program uses. Not used on purpose:
# Inodh's maintenance_scheduler, manual_inventory_changes, report_generation,
# initialize and daily_tasks; Matthew's clock, comms link, dashboard and input.
SW_SOURCES = adc_registry.cpp alarm_notification.cpp user_information/user_registry.cpp \
             user_information/user_settings.cpp startup/authorization.cpp \
             startup/encryption_key_storage.cpp message_processing/encryption.cpp \
             message_processing/patient_info_stripping.cpp events_logs/event_contents.cpp \
             events_logs/message_routing.cpp
SW_OBJECTS = $(patsubst %.cpp,$(OBJ_DIR)/sw/%.o,$(SW_SOURCES))
HW_OBJECTS = $(OBJ_DIR)/hw/long_term_storage.o
INT_SOURCES = $(wildcard integrated/*.cpp) $(wildcard integrated/adapters/*.cpp)
INT_SOURCES := $(filter-out integrated/int_server_main.cpp,$(INT_SOURCES))
INT_OBJECTS = $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(INT_SOURCES))
INTEGRATED_OBJECTS = $(BEHAVIOUR_OBJECTS) $(SW_OBJECTS) $(HW_OBJECTS) $(INT_OBJECTS)

INT_SERVER = $(BIN_DIR)/hms_integrated
INT_TEST = $(BIN_DIR)/integrated_test

$(OBJ_DIR)/sw/%.o: $(SW_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(INT_FLAGS) $(SW_RENAME) -c $< -o $@

$(OBJ_DIR)/hw/%.o: ../hardware_hiding/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(INT_FLAGS) $(SW_RENAME) -c $< -o $@

$(OBJ_DIR)/integrated/%.o: integrated/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(INT_FLAGS) -c $< -o $@

$(INT_TEST): tests/integrated_test.cpp $(INTEGRATED_OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(INT_FLAGS) $< $(INTEGRATED_OBJECTS) -o $@ $(INT_LINK)

integrated-test: $(INT_TEST)
	./$(INT_TEST)

$(INT_SERVER): integrated/int_server_main.cpp $(HTTPLIB) $(INTEGRATED_OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(INT_FLAGS) $(SERVER_FLAGS) $< $(INTEGRATED_OBJECTS) -o $@ $(SERVER_LIBS) $(INT_LINK)

integrated: $(INT_SERVER)

run-integrated: $(INT_SERVER)
	./$(INT_SERVER)

# Symbol check. A strong symbol defined twice would already stop the link, so
# this looks for the quieter problem: a symbol that BOTH sides define (inline
# functions, vtables), which the linker merges silently. That is only safe if
# both copies come from the same source. It fails if any shared symbol names
# AlarmSeverity or ReportTypes (the two clashing types), and lists the other
# shared, non-standard-library symbols for review (expected: only things from
# Inodh's own headers, e.g. eventTypeName, ~ADC, ~RoleSettings).
MINE_OBJECTS = $(BEHAVIOUR_OBJECTS) $(INT_OBJECTS)
THEIR_OBJECTS = $(SW_OBJECTS) $(HW_OBJECTS)
check-symbols: $(INTEGRATED_OBJECTS)
	@nm --defined-only $(MINE_OBJECTS) 2>/dev/null | awk 'NF>=3 && $$3 !~ /^\./ {print $$3}' | sort -u > $(OBJ_DIR)/symbols_mine.txt
	@nm --defined-only $(THEIR_OBJECTS) 2>/dev/null | awk 'NF>=3 && $$3 !~ /^\./ {print $$3}' | sort -u > $(OBJ_DIR)/symbols_theirs.txt
	@comm -12 $(OBJ_DIR)/symbols_mine.txt $(OBJ_DIR)/symbols_theirs.txt | c++filt > $(OBJ_DIR)/symbols_shared.txt
	@echo "Symbols defined on both sides: $$(wc -l < $(OBJ_DIR)/symbols_shared.txt) (mostly the standard library)"
	@echo "Shared, not from the standard library:"; grep -vE '(^|[^A-Za-z])std::|__gnu|__cxx|__x86' $(OBJ_DIR)/symbols_shared.txt | sed 's/^/  /' || true
	@if grep -qE 'AlarmSeverity|ReportTypes' $(OBJ_DIR)/symbols_shared.txt; then 	   echo "PROBLEM: a clashing type is defined on both sides:"; grep -E 'AlarmSeverity|ReportTypes' $(OBJ_DIR)/symbols_shared.txt; exit 1; 	 else echo "OK: no symbol of AlarmSeverity or ReportTypes is defined on both sides."; fi

-include $(OBJECTS:.o=.d) $(API_TEST).d $(SERVER).d $(INTEGRATED_OBJECTS:.o=.d) $(INT_TEST).d

.PHONY: all test run clean integrated integrated-test run-integrated check-symbols
