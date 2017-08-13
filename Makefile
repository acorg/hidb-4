# -*- Makefile -*-
# Eugene Skepner 2016

# submodules and git: https://git-scm.com/book/en/v2/Git-Tools-Submodules

# ----------------------------------------------------------------------

MAKEFLAGS = -w

# ----------------------------------------------------------------------

HIDB_SOURCES = hidb.cc hidb-export.cc hidb-import.cc variant-id.cc vaccines.cc
HIDB_PY_SOURCES = py.cc $(HIDB_SOURCES)

# ----------------------------------------------------------------------

include $(ACMACSD_ROOT)/share/Makefile.g++
include $(ACMACSD_ROOT)/share/Makefile.dist-build.vars

PYTHON_VERSION = $(shell python3 -c 'import sys; print("{0.major}.{0.minor}".format(sys.version_info))')
PYTHON_CONFIG = python$(PYTHON_VERSION)-config
PYTHON_MODULE_SUFFIX = $(shell $(PYTHON_CONFIG) --extension-suffix)

LIB_DIR = $(ACMACSD_ROOT)/lib
LOCATION_DB_LIB = $(LIB_DIR)/liblocationdb.so
HIDB_LIB = $(DIST)/libhidb.so

# -fvisibility=hidden and -flto make resulting lib smaller (pybind11) but linking is much slower
OPTIMIZATION = -O3 #-fvisibility=hidden -flto
PROFILE = # -pg
CXXFLAGS = -MMD -g $(OPTIMIZATION) $(PROFILE) -fPIC -std=$(STD) $(WEVERYTHING) $(WARNINGS) -Icc -I$(BUILD)/include -I$(ACMACSD_ROOT)/include $(PKG_INCLUDES)
LDFLAGS = $(OPTIMIZATION) $(PROFILE)
HIDB_LDLIBS = -L$(LIB_DIR) -llocationdb -lacmacsbase -lacmacschart -lboost_filesystem -lboost_system $(shell pkg-config --libs liblzma) $(shell $(PYTHON_CONFIG) --ldflags | sed -E 's/-Wl,-stack_size,[0-9]+//')

PKG_INCLUDES = $(shell pkg-config --cflags liblzma) $(shell $(PYTHON_CONFIG) --includes)

# ----------------------------------------------------------------------

all: check-acmacsd-root $(DIST)/hidb_backend$(PYTHON_MODULE_SUFFIX) $(HIDB_LIB)

install: check-acmacsd-root install-headers $(DIST)/hidb_backend$(PYTHON_MODULE_SUFFIX) $(HIDB_LIB)
	ln -sf $(HIDB_LIB) $(ACMACSD_ROOT)/lib
	if [ $$(uname) = "Darwin" ]; then /usr/bin/install_name_tool -id $(ACMACSD_ROOT)/lib/$(notdir $(HIDB_LIB)) $(ACMACSD_ROOT)/lib/$(notdir $(HIDB_LIB)); fi
	ln -sf $(DIST)/hidb_backend$(PYTHON_MODULE_SUFFIX) $(ACMACSD_ROOT)/py
	ln -sf $(abspath py)/* $(ACMACSD_ROOT)/py
	ln -sf $(abspath bin)/hidb-* $(ACMACSD_ROOT)/bin
	$(abspath bin)/hidb-get-from-albertine

install-headers:
	if [ ! -d $(ACMACSD_ROOT)/include/hidb ]; then mkdir $(ACMACSD_ROOT)/include/hidb; fi
	ln -sf $(abspath cc)/*.hh $(ACMACSD_ROOT)/include/hidb

test: install
	test/test

include $(ACMACSD_ROOT)/share/Makefile.rtags

# ----------------------------------------------------------------------

-include $(BUILD)/*.d

# ----------------------------------------------------------------------

$(DIST)/hidb_backend$(PYTHON_MODULE_SUFFIX): $(patsubst %.cc,$(BUILD)/%.o,$(HIDB_PY_SOURCES)) | $(DIST) $(LOCATION_DB_LIB)
	$(CXX) -shared $(LDFLAGS) -o $@ $^ $(HIDB_LDLIBS)
	@#strip $@

$(HIDB_LIB): $(patsubst %.cc,$(BUILD)/%.o,$(HIDB_SOURCES)) | $(DIST) $(LOCATION_DB_LIB)
	$(CXX) -shared $(LDFLAGS) -o $@ $^ $(HIDB_LDLIBS)

# $(DIST)/test-rapidjson: $(BUILD)/test-rapidjson.o $(BUILD)/chart.o $(BUILD)/chart-rj.o $(BUILD)/ace.o $(BUILD)/read-file.o $(BUILD)/xz.o | $(DIST)
#	$(CXX) $(LDFLAGS) -o $@ $^ $(shell pkg-config --libs liblzma)

# ----------------------------------------------------------------------

$(BUILD)/%.o: cc/%.cc | $(BUILD) install-headers
	@echo $<
	@$(CXX) $(CXXFLAGS) -c -o $@ $<

# ----------------------------------------------------------------------

# $(BUILD)/submodules:
#	git submodule init
#	git submodule update
#	git submodule update --remote
#	touch $@

# ----------------------------------------------------------------------

check-acmacsd-root:
ifndef ACMACSD_ROOT
	$(error ACMACSD_ROOT is not set)
endif

include $(ACMACSD_ROOT)/share/Makefile.dist-build.rules

.PHONY: check-acmacsd-root

# ======================================================================
### Local Variables:
### eval: (if (fboundp 'eu-rename-buffer) (eu-rename-buffer))
### End:
