COMPDB := $(BUILD_DIR)/compile_commands.json
COMPDB_SCRIPT := $(TOPLEVEL)/mk/gen_compdb.py
COMPDB_FRAGMENTS_DIR := $(BUILD_DIR)/compdb

.PHONY: compdb-merge compdb-fragment

compdb-fragment:
	@mkdir -p $(COMPDB_FRAGMENTS_DIR)
	$(Q)$(PYTHON) $(COMPDB_SCRIPT) fragment \
		--cxx "$(CXX)" \
		--cxxflags "$(BASE_CXXFLAGS) $(LOCAL_CXXFLAGS)" \
		--pcm-dir "$(PCM_DIR)" \
		--toplevel "$(TOPLEVEL)" \
		--output "$(COMPDB_FRAGMENTS_DIR)/$(notdir $(CURDIR)).json" \
		$(MODULE_SRCS) $(ALL_CC_SRCS)

compdb-merge:
	$(Q)$(PYTHON) $(COMPDB_SCRIPT) merge \
		--output "$(COMPDB)" \
		$(wildcard $(COMPDB_FRAGMENTS_DIR)/*.json)
	@ln -sf $(COMPDB) $(TOPLEVEL)/compile_commands.json
	$(call MSG,COMPDB,$(COMPDB))
