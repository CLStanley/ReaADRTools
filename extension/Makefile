# Linux x86_64 REAPER extension build and dist payload assembly.
#
# Windows builds are handled by build-windows-msvc.bat because the Windows DLL
# must be built with MSVC.

REAPER_SDK := ../vendor/reaper-sdk
WDL := ../vendor/WDL/WDL
TARGET := reaper_reaadr-x86_64.so
BUILD_DIR := ../build/extension
TEST_TARGET := $(BUILD_DIR)/session_model_tests
DIST_DIR := ../dist
DIST_USERPLUGINS_DIR := $(DIST_DIR)/UserPlugins
DIST_REAADR_DIR := $(DIST_DIR)/Scripts/ReaADRTools

CXX ?= g++
CXXFLAGS ?= -std=c++17 -fPIC -O2 -Wall -Wextra
CPPFLAGS += -isystem $(REAPER_SDK)/sdk -isystem $(WDL)
LDFLAGS += -shared -ldl
CORE_SOURCES := reaadr_core/session_model.cpp
CORE_HEADERS := reaadr_core/session_model.hpp
EXTENSION_SOURCES := reaper_reaadr.cpp $(CORE_SOURCES)

.PHONY: all clean dist prepare-sdk test

all: $(BUILD_DIR)/$(TARGET)

prepare-sdk:
	@if [ ! -d "$(REAPER_SDK)/sdk" ] || [ ! -d "$(WDL)/swell" ]; then \
		echo "Missing SDK dependencies. Clone reaper-sdk to vendor/reaper-sdk and WDL to vendor/WDL before building."; \
		exit 1; \
	fi
	@if [ ! -e "$(REAPER_SDK)/WDL" ]; then ln -s ../WDL/WDL "$(REAPER_SDK)/WDL"; fi

$(BUILD_DIR)/$(TARGET): $(EXTENSION_SOURCES) $(CORE_HEADERS) | prepare-sdk
	@mkdir -p "$(BUILD_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(EXTENSION_SOURCES) -o $@ $(LDFLAGS)

$(TEST_TARGET): ../tests/session_model_tests.cpp $(CORE_SOURCES) $(CORE_HEADERS)
	@mkdir -p "$(BUILD_DIR)"
	$(CXX) -I. $(CXXFLAGS) ../tests/session_model_tests.cpp $(CORE_SOURCES) -o $@

test: $(TEST_TARGET)
	$(TEST_TARGET)

dist: all
	rm -rf "$(DIST_DIR)"
	@mkdir -p "$(DIST_USERPLUGINS_DIR)" "$(DIST_REAADR_DIR)/scripts" "$(DIST_REAADR_DIR)/assets"
	cp "$(BUILD_DIR)/$(TARGET)" "$(DIST_USERPLUGINS_DIR)/$(TARGET)"
	cp ../scripts/*.lua "$(DIST_REAADR_DIR)/scripts/"
	cp ../assets/* "$(DIST_REAADR_DIR)/assets/"

clean:
	rm -rf "$(BUILD_DIR)" ../dist
