CXX      := c++
CXXFLAGS := -Isource -std=c++26 -Wall -Wextra -Werror -MMD -MP
BINARY   := $(abspath dcc)
BUILD    := $(abspath build)

SRCS := $(shell find source -name '*.cc')
OBJS := $(addprefix $(BUILD)/obj/, $(SRCS:%=%.o))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean

all: $(BINARY)

$(BINARY): $(OBJS)
	@echo "[ld] linking $@"
	$(CXX) $(OBJS) -o $@

$(BUILD)/obj/%.cc.o: %.cc
	@mkdir -p $(dir $@)
	@echo "[c++] compiling $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(BINARY)

-include $(DEPS)
