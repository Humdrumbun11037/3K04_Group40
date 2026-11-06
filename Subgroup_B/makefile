# One makefile for the whole HMS (Subgroup_B). Run make from this folder.
# Every .cpp file inside a module folder is compiled automatically, including
# subfolders, so new modules need no makefile edits. Module files must not
# contain main(); mains live in tests/ and demo_main.cpp.
# #include paths are written from this folder, e.g. "behaviour_hiding/interfaces/clock.hpp".

# Compiler and flags (-MMD -MP: rebuild a file when a header it includes changes)
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I. -I../sw_decisions -I../Subgroup_A/inc -MMD -MP

# Behaviour Hiding and Hardware Hiding are compiled separately.

TEST_DIR = tests
OBJ_DIR = obj
BIN_DIR = bin

# rwildcard: like wildcard, but also searches subfolders
rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

BEHAVIOUR_SOURCES = $(call rwildcard,behaviour_hiding,*.cpp)
HARDWARE_SOURCES = $(call rwildcard,hardware_hiding,*.cpp)

BEHAVIOUR_OBJECTS = $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(BEHAVIOUR_SOURCES))
HARDWARE_OBJECTS = $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(HARDWARE_SOURCES))

# Subgroup A transceiver implementation required by Hardware Hiding
TRANSCEIVER_OBJECT = $(OBJ_DIR)/HW1_Transceiver.o

ALL_OBJECTS = $(BEHAVIOUR_OBJECTS) $(HARDWARE_OBJECTS) $(TRANSCEIVER_OBJECT)

$(TRANSCEIVER_OBJECT): ../Subgroup_A/src/HW1_Transceiver.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.cpp)
TEST_BINS = $(patsubst $(TEST_DIR)/%.cpp,$(BIN_DIR)/%,$(TEST_SOURCES))
DEMO = $(BIN_DIR)/demo

# Default build target: compile every module
all: $(ALL_OBJECTS)

# Compile a module .cpp into obj/, keeping its folder path
$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link one test program against all module objects
$(BIN_DIR)/%: $(TEST_DIR)/%.cpp $(BEHAVIOUR_OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< $(BEHAVIOUR_OBJECTS) -o $@

# Build and run every test program; stop at the first failure
test: $(TEST_BINS)
	@for t in $(TEST_BINS); do ./$$t || exit 1; done

# Build and run the console demo
$(DEMO): demo_main.cpp $(BEHAVIOUR_OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< $(BEHAVIOUR_OBJECTS) -o $@

demo: $(DEMO)
	./$(DEMO)

# Clean up build files
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

-include $(ALL_OBJECTS:.o=.d) $(TEST_BINS:=.d) $(DEMO).d

.PHONY: all test demo clean
