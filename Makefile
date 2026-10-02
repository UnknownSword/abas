.PHONY: all build safe run test check clean

CXX          := g++
CXXFLAGS     := -Iinclude
# -fwrapv is part of what a value is. a value is 64 bits and a number that does
# not fit in one goes round rather than being undefined, so a mul, an add, a
# negation and a shift all come back with the answer a word can hold
SAFE_FLAGS   := -fsanitize=address,undefined -fno-omit-frame-pointer -fwrapv -Wall -Wextra -g
LIBS :=

CXXFLAGS += $(SAFE_FLAGS)

SRC_DIR      := src
INCLUDE_DIR  := include
BUILD_DIR    := build
BIN_DIR      := bin
TEST_DIR     := test

MAIN_SRC     := main
BINARY       := $(BIN_DIR)/$(MAIN_SRC)
MAIN_FILE    := $(TEST_DIR)/cases/01_hello.abas

SRCS := $(shell find $(SRC_DIR) -name '*.cpp' -o -name '*.c')
OBJS := $(BUILD_DIR)/$(MAIN_SRC).o \
        $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(filter %.cpp,$(SRCS))) \
        $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(filter %.c,$(SRCS)))
DEPS := $(OBJS:.o=.d)

all: build

build: $(BINARY)

$(BINARY): $(OBJS)
	@mkdir -p $(BIN_DIR)
	@echo "Linking $@ ..."
	@$(CXX) $(OBJS) $(CXXFLAGS) $(LIBS) -o $@
	@echo "Build successful! Run with: ./$@"

$(BUILD_DIR)/$(MAIN_SRC).o: $(MAIN_SRC).cpp
	@mkdir -p $(dir $@)
	@echo "Compiling $< ..."
	@$(CXX) $(CXXFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "Compiling $< ..."
	@$(CXX) $(CXXFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $< ..."
	@$(CXX) $(CXXFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

safe: CXXFLAGS += $(SAFE_FLAGS)
safe: build

run: build
	@./$(BINARY) $(MAIN_FILE)

test: build
	@$(TEST_DIR)/run.sh ./$(BINARY)

check: test

clean:
	@echo "Cleaning up ..."
	rm -rf $(BUILD_DIR) $(BIN_DIR) tmp

-include $(DEPS)
