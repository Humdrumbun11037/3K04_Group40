# One makefile for the whole HMS (Subgroup_B). Run make from this folder.
# Every .cpp file inside a module folder is compiled automatically, including
# subfolders, so new modules need no makefile edits. Module files must not
# contain main(); mains live in tests/ and demo_main.cpp.
# #include paths are written from this folder, e.g. "behaviour_hiding/interfaces/clock.hpp".

# Compiler and flags (-MMD -MP: rebuild a file when a header it includes changes)
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I. -MMD -MP

# Directories (a module folder that does not exist yet is simply skipped)
# Only behaviour_hiding is built. Other groups add their folder here once it
# compiles (see INTEGRATION_PROPOSAL.md).
MODULE_DIRS = behaviour_hiding
TEST_DIR = tests
OBJ_DIR = obj
BIN_DIR = bin

# rwildcard: like wildcard, but also searches subfolders
rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

SOURCES = $(foreach dir,$(MODULE_DIRS),$(call rwildcard,$(dir),*.cpp))
OBJECTS = $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(SOURCES))
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.cpp)
TEST_BINS = $(patsubst $(TEST_DIR)/%.cpp,$(BIN_DIR)/%,$(TEST_SOURCES))
DEMO = $(BIN_DIR)/demo

# Default build target: compile every module
all: $(OBJECTS)

# Compile a module .cpp into obj/, keeping its folder path
$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link one test program against all module objects
$(BIN_DIR)/%: $(TEST_DIR)/%.cpp $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< $(OBJECTS) -o $@

# Build and run every test program; stop at the first failure
test: $(TEST_BINS)
	@for t in $(TEST_BINS); do ./$$t || exit 1; done

# Build and run the console demo
$(DEMO): demo_main.cpp $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< $(OBJECTS) -o $@

demo: $(DEMO)
	./$(DEMO)

# Clean up build files
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

-include $(OBJECTS:.o=.d) $(TEST_BINS:=.d) $(DEMO).d

.PHONY: all test demo clean
