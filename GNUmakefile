CXX := clang++
CXXFLAGS := -Isource -std=c++26 -Wall -Wextra -Werror -MMD -MP -fsanitize=address,undefined -fno-omit-frame-pointer -g

BINARY := $(abspath dcc)

DRIVER_DIR := $(abspath driver)
TEST_DIR := $(abspath tests)
BUILD := $(abspath build)

CORE_LIB := $(BUILD)/libcompiler.a
CORE_SRCS := $(shell find source -name '*.cc')
CORE_OBJS := $(addprefix $(BUILD)/obj/, $(CORE_SRCS:%=%.o))

DRIVER_SRCS := $(shell find $(DRIVER_DIR) -name '*.cc')
DRIVER_OBJS := $(addprefix $(BUILD)/obj/, $(DRIVER_SRCS:%=%.o))

TEST_SRCS := $(shell find $(TEST_DIR) -name '*.cc')
TEST_OBJS := $(addprefix $(BUILD)/obj/, $(TEST_SRCS:%=%.o))

DEPS := $(CORE_OBJS:.o=.d) $(DRIVER_OBJS:.o=.d) $(TEST_OBJS:.o=.d)

GTEST_CFLAGS := $(shell pkg-config --cflags gtest_main)
GTEST_LIBS := $(shell pkg-config --libs gtest_main)

.PHONY: all clean test

all: $(BINARY)

$(CORE_LIB): $(CORE_OBJS)
	@echo "[ar] creating static library $@"
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(CORE_OBJS): $(BUILD)/obj/%.cc.o: %.cc
	@mkdir -p $(dir $@)
	@echo "[c++] compiling core compiler source $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BINARY): $(DRIVER_OBJS) $(CORE_LIB)
	@echo "[ld] linking $@"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DRIVER_OBJS) $(CORE_LIB) -o $@

$(DRIVER_OBJS): $(BUILD)/obj/%.cc.o: %.cc
	@mkdir -p $(dir $@)
	@echo "[c++] compiling driver source $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TEST_OBJS): $(BUILD)/obj/%.cc.o: %.cc
	@mkdir -p $(dir $@)
	@echo "[c++] compiling test source $<"
	$(CXX) $(CXXFLAGS) $(GTEST_CFLAGS) -c $< -o $@

test: $(TEST_OBJS) $(CORE_LIB)
	@echo "[test] running tests"
	$(CXX) $(CXXFLAGS) $(TEST_OBJS) $(CORE_LIB) $(GTEST_LIBS) -o $(BUILD)/test_runner
	$(BUILD)/test_runner

clean:
	rm -rf $(BUILD) $(BINARY)

-include $(DEPS)
