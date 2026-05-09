include $(RULES_MK)

LOCAL_CXXFLAGS := $(LLVM_DEFS) $(LLVM_CXXFLAGS)

.PHONY: all test clean

COMPILER_LIB := $(BUILD_DIR)/lib/libcompiler.a

ALL_TEST_SRCS := $(call find-sources,$(TOPLEVEL)/tests/source,cc)

ifeq ($(ENABLE_LLVM),0)
  TEST_SRCS := $(filter-out %/test_backend.cc,$(ALL_TEST_SRCS))
else
  TEST_SRCS := $(ALL_TEST_SRCS)
endif

ALL_TEST_OBJS := $(call objs-from-srcs,$(TEST_SRCS))
ALL_TEST_BINS := $(patsubst $(TOPLEVEL)/tests/source/%.cc,$(BUILD_DIR)/bin/tests/%,$(TEST_SRCS))
ALL_DEPS := $(ALL_TEST_OBJS:.o=.d)

COMPILER_PCMS := $(wildcard $(PCM_DIR)/compiler/source/*.pcm)

TEST_LDFLAGS := $(LLVM_LDFLAGS)

all: test

test: $(ALL_TEST_BINS)
	@passed=0; failed=0; \
	for bin in $(ALL_TEST_BINS); do \
		name=$$(basename $$bin); \
		if $$bin; then \
			passed=$$((passed+1)); \
		else \
			failed=$$((failed+1)); \
			printf "  FAIL     %s\n" "$$name"; \
		fi; \
	done; \
	if [ $$failed -eq 0 ]; then \
		printf "  PASS     %d/%d test suites\n" $$passed $$((passed+failed)); \
	else \
		printf "  FAIL     %d/%d test suites\n" $$failed $$((passed+failed)); \
		exit 1; \
	fi

$(BUILD_DIR)/bin/tests/%: $(OBJ_DIR)/tests/source/%.cc.o $(COMPILER_LIB)
	$(call link-binary,$@,$^,$(TEST_LDFLAGS))

$(ALL_TEST_OBJS): $(OBJ_DIR)/%.o: $(TOPLEVEL)/% | $(COMPILER_PCMS)
	$(call compile-cxx,$<,$@,$(LOCAL_CXXFLAGS))

clean:
	rm -f $(ALL_TEST_OBJS) $(ALL_DEPS) $(ALL_TEST_BINS)
	rm -rf $(BUILD_DIR)/bin/tests

-include $(ALL_DEPS)

include $(STD_MK)
