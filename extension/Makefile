# Linux x86_64 REAPER extension build and dist payload assembly.
#
# Windows builds are handled by build-windows-msvc.bat because the Windows DLL
# must be built with MSVC.

REAPER_SDK := ../vendor/reaper-sdk
WDL := ../vendor/WDL/WDL
TARGET := reaper_reaadr-x86_64.so
BUILD_DIR := ../build/extension
DIST_DIR := ../dist
DIST_USERPLUGINS_DIR := $(DIST_DIR)/UserPlugins
DIST_REAADR_DIR := $(DIST_DIR)/Scripts/ReaADRTools

CXX ?= g++
CXXFLAGS ?= -std=c++17 -fPIC -O2 -Wall -Wextra
CPPFLAGS += -isystem $(REAPER_SDK)/sdk -isystem $(WDL)
LDFLAGS += -shared -ldl

.PHONY: all clean dist prepare-sdk

all: prepare-sdk $(BUILD_DIR)/$(TARGET)

prepare-sdk:
	@if [ ! -d "$(REAPER_SDK)/sdk" ] || [ ! -d "$(WDL)/swell" ]; then \
		echo "Missing SDK dependencies. Clone reaper-sdk to vendor/reaper-sdk and WDL to vendor/WDL before building."; \
		exit 1; \
	fi
	@if [ ! -e "$(REAPER_SDK)/WDL" ]; then ln -s ../WDL/WDL "$(REAPER_SDK)/WDL"; fi

$(BUILD_DIR)/$(TARGET): reaper_reaadr.cpp
	@mkdir -p "$(BUILD_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

dist: all
	rm -rf "$(DIST_DIR)"
	@mkdir -p "$(DIST_USERPLUGINS_DIR)" "$(DIST_REAADR_DIR)/scripts" "$(DIST_REAADR_DIR)/assets"
	cp "$(BUILD_DIR)/$(TARGET)" "$(DIST_USERPLUGINS_DIR)/$(TARGET)"
	cp ../scripts/*.lua "$(DIST_REAADR_DIR)/scripts/"
	cp ../assets/* "$(DIST_REAADR_DIR)/assets/"

clean:
	rm -rf "$(BUILD_DIR)" ../dist
