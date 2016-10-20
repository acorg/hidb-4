# -*- Makefile -*-
# Eugene Skepner 2016

# submodules and git: https://git-scm.com/book/en/v2/Git-Tools-Submodules

# ----------------------------------------------------------------------

MAKEFLAGS = -w

# ----------------------------------------------------------------------

HIDB_SOURCES = hidb-py.cc chart.cc read-file.cc xz.cc

# ----------------------------------------------------------------------

CLANG = $(shell if g++ --version 2>&1 | grep -i llvm >/dev/null; then echo Y; else echo N; fi)
ifeq ($(CLANG),Y)
  WEVERYTHING = -Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-padded
  WARNINGS = -Wno-weak-vtables # -Wno-padded
  STD = c++14
else
  WEVERYTHING = -Wall -Wextra
  WARNINGS =
  STD = c++14
endif

PYTHON_VERSION = $(shell python3 -c 'import sys; print("{0.major}.{0.minor}".format(sys.version_info))')
PYTHON_CONFIG = python$(PYTHON_VERSION)-config
PYTHON_MODULE_SUFFIX = $(shell $(PYTHON_CONFIG) --extension-suffix)

# -fvisibility=hidden and -flto make resulting lib smaller (pybind11) but linking is much slower
OPTIMIZATION = -O3 #-fvisibility=hidden -flto
CXXFLAGS = -MMD -g $(OPTIMIZATION) -fPIC -std=$(STD) $(WEVERYTHING) $(WARNINGS) -I$(BUILD)/include $(PKG_INCLUDES) $(MODULES_INCLUDE)
LDFLAGS = -O3
HIDB_LDLIBS = $$(pkg-config --libs liblzma) $$($(PYTHON_CONFIG) --ldflags | sed -E 's/-Wl,-stack_size,[0-9]+//')

MODULES_INCLUDE = -Imodules/rapidjson/include -Imodules/pybind11/include
PKG_INCLUDES = $$(pkg-config --cflags liblzma) $$($(PYTHON_CONFIG) --includes)

# ----------------------------------------------------------------------

BUILD = build
DIST = dist

all: $(DIST)/hidb_backend$(PYTHON_MODULE_SUFFIX) $(DIST)/test-rapidjson

-include $(BUILD)/*.d

# ----------------------------------------------------------------------

$(DIST)/test-rapidjson: $(BUILD)/test-rapidjson.o $(BUILD)/chart.o $(BUILD)/chart-rj.o $(BUILD)/read-file.o $(BUILD)/xz.o | $(DIST)
	g++ $(LDFLAGS) -o $@ $^ $$(pkg-config --libs liblzma)

$(DIST)/hidb_backend$(PYTHON_MODULE_SUFFIX): $(patsubst %.cc,$(BUILD)/%.o,$(HIDB_SOURCES)) | $(DIST)
	g++ -shared $(LDFLAGS) -o $@ $^ $(HIDB_LDLIBS)
	@#strip $@

clean:
	rm -rf $(DIST) $(BUILD)/*.o $(BUILD)/*.d $(BUILD)/submodules

distclean: clean
	rm -rf $(BUILD)

# ----------------------------------------------------------------------

$(BUILD)/%.o: src/%.cc | $(BUILD) $(BUILD)/submodules
	@echo $<
	@g++ $(CXXFLAGS) -c -o $@ $<

# ----------------------------------------------------------------------

$(BUILD)/submodules:
	git submodule init
	git submodule update
	git submodule update --remote
	touch $@

# ----------------------------------------------------------------------

$(DIST):
	mkdir -p $(DIST)

$(BUILD):
	mkdir -p $(BUILD)

# ======================================================================
### Local Variables:
### eval: (if (fboundp 'eu-rename-buffer) (eu-rename-buffer))
### End: