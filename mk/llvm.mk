LLVM_CONFIG ?= llvm-config

ifeq ($(ENABLE_LLVM),1)
  LLVM_CXXFLAGS := $(shell $(LLVM_CONFIG) --cppflags 2>/dev/null)
  LLVM_LDFLAGS  := $(shell $(LLVM_CONFIG) --ldflags --libs core native orcjit support --system-libs 2>/dev/null)
  LLVM_DEFS     := -DDCC_ENABLE_LLVM=1
else
  LLVM_CXXFLAGS :=
  LLVM_LDFLAGS  :=
  LLVM_DEFS     :=
endif
