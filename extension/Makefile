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
CPPFLAGS += -I. -isystem $(REAPER_SDK)/sdk -isystem $(WDL)
LDFLAGS += -shared -ldl
CORE_SOURCES := reaadr_core/session_model.cpp reaadr_core/model_repository.cpp reaadr_core/domain_utils.cpp \
	reaadr_core/cue_import.cpp reaadr_core/session_builder.cpp reaadr_core/session_mutation.cpp \
	reaadr_core/session_commit.cpp reaadr_core/lane_assignment.cpp reaadr_core/render_plan.cpp \
	reaadr_core/cue_wav.cpp reaadr_core/event_log.cpp reaadr_core/character_filter.cpp \
	reaadr_core/region_timing_sync.cpp reaadr_core/cue_navigation.cpp reaadr_core/record_arm.cpp \
	reaadr_core/recording_setup.cpp reaadr_core/recording_transport.cpp reaadr_core/cue_status.cpp \
	reaadr_core/recording_preferences.cpp reaadr_core/overlay_settings.cpp \
	reaadr_core/overlay_eel.cpp reaadr_core/overlay_refresh.cpp
CORE_HEADERS := reaadr_core/session_model.hpp reaadr_core/model_repository.hpp reaadr_core/domain_utils.hpp \
	reaadr_core/cue_import.hpp reaadr_core/session_builder.hpp reaadr_core/session_mutation.hpp \
	reaadr_core/session_commit.hpp reaadr_core/lane_assignment.hpp reaadr_core/render_plan.hpp \
	reaadr_core/cue_wav.hpp reaadr_core/event_log.hpp reaadr_core/character_filter.hpp \
	reaadr_core/region_timing_sync.hpp reaadr_core/cue_navigation.hpp reaadr_core/record_arm.hpp \
	reaadr_core/recording_setup.hpp reaadr_core/recording_transport.hpp reaadr_core/cue_status.hpp \
	reaadr_core/recording_preferences.hpp reaadr_core/overlay_settings.hpp \
	reaadr_core/overlay_eel.hpp reaadr_core/overlay_refresh.hpp
REAPER_SOURCES := reaadr_reaper/project_state.cpp reaadr_reaper/project_transaction.cpp \
	reaadr_reaper/track_region_adapter.cpp reaadr_reaper/render_artifact_adapter.cpp \
	reaadr_reaper/session_render_service.cpp reaadr_reaper/character_filter_adapter.cpp \
	reaadr_reaper/cue_navigation_service.cpp reaadr_reaper/record_arm_adapter.cpp \
	reaadr_reaper/recording_setup_adapter.cpp reaadr_reaper/recording_transport_executor.cpp \
	reaadr_reaper/recording_application_service.cpp reaadr_reaper/overlay_refresh_adapter.cpp \
	reaadr_reaper/overlay_application_service.cpp
REAPER_HEADERS := reaadr_reaper/project_state.hpp reaadr_reaper/project_transaction.hpp \
	reaadr_reaper/track_region_adapter.hpp reaadr_reaper/render_artifact_adapter.hpp \
	reaadr_reaper/session_render_service.hpp reaadr_reaper/character_filter_adapter.hpp \
	reaadr_reaper/cue_navigation_service.hpp reaadr_reaper/record_arm_adapter.hpp \
	reaadr_reaper/recording_setup_adapter.hpp reaadr_reaper/recording_transport_executor.hpp \
	reaadr_reaper/recording_application_service.hpp reaadr_reaper/overlay_refresh_adapter.hpp \
	reaadr_reaper/overlay_application_service.hpp
NATIVE_SOURCES := $(CORE_SOURCES) $(REAPER_SOURCES)
NATIVE_HEADERS := $(CORE_HEADERS) $(REAPER_HEADERS)
EXTENSION_SOURCES := reaper_reaadr.cpp $(NATIVE_SOURCES)

.PHONY: all clean dist prepare-sdk test

all: $(BUILD_DIR)/$(TARGET)

prepare-sdk:
	@if [ ! -d "$(REAPER_SDK)/sdk" ] || [ ! -d "$(WDL)/swell" ]; then \
		echo "Missing SDK dependencies. Clone reaper-sdk to vendor/reaper-sdk and WDL to vendor/WDL before building."; \
		exit 1; \
	fi
	@if [ ! -e "$(REAPER_SDK)/WDL" ]; then ln -s ../WDL/WDL "$(REAPER_SDK)/WDL"; fi

$(BUILD_DIR)/$(TARGET): $(EXTENSION_SOURCES) $(NATIVE_HEADERS) | prepare-sdk
	@mkdir -p "$(BUILD_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(EXTENSION_SOURCES) -o $@ $(LDFLAGS)

$(TEST_TARGET): ../tests/session_model_tests.cpp $(NATIVE_SOURCES) $(NATIVE_HEADERS)
	@mkdir -p "$(BUILD_DIR)"
	$(CXX) -I. $(CXXFLAGS) ../tests/session_model_tests.cpp $(NATIVE_SOURCES) -o $@

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
