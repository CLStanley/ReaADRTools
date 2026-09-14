# Linux x86_64 REAPER extension build and dist payload assembly.
#
# Windows builds are handled by build-windows-msvc.bat because the Windows DLL
# must be built with MSVC.

REAPER_SDK := ../vendor/reaper-sdk
WDL := ../vendor/WDL/WDL
TARGET := reaper_reaadr-x86_64.so
BUILD_DIR := ../build/extension
TEST_TARGET := $(BUILD_DIR)/session_model_tests
MARKER_TEST_TARGET := $(BUILD_DIR)/marker_cue_generation_tests
DIALOGUE_TEST_TARGET := $(BUILD_DIR)/dialogue_cue_generation_tests
DIALOGUE_DETECTION_TEST_TARGET := $(BUILD_DIR)/dialogue_detection_adapter_tests
RECORDING_TARGET_TEST_TARGET := $(BUILD_DIR)/recording_target_tests
RECORDING_SHUTDOWN_TEST_TARGET := $(BUILD_DIR)/recording_shutdown_tests
CUE_INFO_TEST_TARGET := $(BUILD_DIR)/cue_info_tests
CUE_TAKE_COUNT_TEST_TARGET := $(BUILD_DIR)/cue_take_count_adapter_tests
CUE_DIRECTION_EDIT_TEST_TARGET := $(BUILD_DIR)/cue_direction_edit_tests
MANAGER_NAVIGATION_TEST_TARGET := $(BUILD_DIR)/manager_navigation_tests
CUE_MANAGER_SESSION_TEST_TARGET := $(BUILD_DIR)/cue_manager_session_tests
QUICK_ACTION_TEST_TARGET := $(BUILD_DIR)/quick_action_tests
DIST_DIR := ../dist
DIST_USERPLUGINS_DIR := $(DIST_DIR)/UserPlugins
DIST_REAADR_DIR := $(DIST_DIR)/Scripts/ReaADRTools
CXX ?= g++
CXXFLAGS ?= -std=c++17 -fPIC -O2 -Wall -Wextra
CPPFLAGS += -I. -isystem $(REAPER_SDK)/sdk -isystem $(WDL)
CPPFLAGS += -DSWELL_PROVIDED_BY_APP
CPPFLAGS += -DREAPERAPI_WANT_CountTakes -DREAPERAPI_WANT_GetSet_LoopTimeRange2
LDFLAGS += -shared -ldl
CORE_SOURCES := reaadr_core/session_model.cpp reaadr_core/model_repository.cpp reaadr_core/domain_utils.cpp \
	reaadr_core/cue_import.cpp reaadr_core/marker_cue_generation.cpp reaadr_core/dialogue_cue_generation.cpp reaadr_core/cue_info.cpp reaadr_core/session_builder.cpp reaadr_core/session_mutation.cpp \
	reaadr_core/session_commit.cpp reaadr_core/lane_assignment.cpp reaadr_core/render_plan.cpp reaadr_core/cue_wav.cpp reaadr_core/event_log.cpp \
	reaadr_core/character_filter.cpp reaadr_core/cue_cleanup.cpp reaadr_core/cue_manager_model.cpp reaadr_core/manager_preferences.cpp \
	reaadr_core/region_timing_sync.cpp reaadr_core/cue_navigation.cpp reaadr_core/record_arm.cpp reaadr_core/recording_setup.cpp \
	reaadr_core/recording_target.cpp reaadr_core/recording_transport.cpp reaadr_core/cue_status.cpp reaadr_core/recording_preferences.cpp \
	reaadr_core/overlay_settings.cpp reaadr_core/overlay_eel.cpp reaadr_core/overlay_refresh.cpp
UI_SOURCES := reaadr_ui/reaadr_ui.cpp reaadr_ui/cue_manager_controller.cpp reaadr_ui/cue_manager_window_swell.cpp \
	reaadr_ui/character_filter_window.cpp reaadr_ui/cue_info_controller.cpp reaadr_ui/cue_info_window.cpp reaadr_ui/cue_status_window.cpp \
	reaadr_ui/recording_controller.cpp reaadr_ui/recording_window.cpp reaadr_ui/manager_view_model.cpp reaadr_ui/manager_navigation.cpp \
	reaadr_ui/manager_ui_contract.cpp reaadr_ui/cue_manager_ui_contract.cpp
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
UI_SOURCES += $(WDL)/swell/swell-modstub.mm
else
UI_SOURCES += $(WDL)/swell/swell-modstub-generic.cpp
endif
CORE_HEADERS := reaadr_core/session_model.hpp reaadr_core/model_repository.hpp reaadr_core/domain_utils.hpp reaadr_core/cue_import.hpp \
	reaadr_core/marker_cue_generation.hpp reaadr_core/dialogue_cue_generation.hpp reaadr_core/cue_info.hpp reaadr_core/session_builder.hpp reaadr_core/session_mutation.hpp \
	reaadr_core/session_commit.hpp reaadr_core/lane_assignment.hpp reaadr_core/render_plan.hpp reaadr_core/cue_wav.hpp reaadr_core/event_log.hpp \
	reaadr_core/character_filter.hpp reaadr_core/cue_cleanup.hpp reaadr_core/cue_manager_model.hpp reaadr_core/manager_preferences.hpp \
	reaadr_core/region_timing_sync.hpp reaadr_core/cue_navigation.hpp reaadr_core/record_arm.hpp reaadr_core/recording_setup.hpp \
	reaadr_core/recording_target.hpp reaadr_core/recording_transport.hpp reaadr_core/cue_status.hpp reaadr_core/recording_preferences.hpp \
	reaadr_core/overlay_settings.hpp reaadr_core/overlay_eel.hpp reaadr_core/overlay_refresh.hpp
REAPER_SOURCES := reaadr_reaper/project_state.cpp reaadr_reaper/project_transaction.cpp reaadr_reaper/track_region_adapter.cpp \
	reaadr_reaper/render_artifact_adapter.cpp reaadr_reaper/session_render_service.cpp reaadr_reaper/native_host_services.cpp reaadr_reaper/cue_manager_session.cpp \
	reaadr_reaper/marker_snapshot_adapter.cpp reaadr_reaper/marker_cue_generation_command.cpp reaadr_reaper/dialogue_detection_adapter.cpp \
	reaadr_reaper/dialogue_detection_command.cpp reaadr_reaper/cue_status_command.cpp reaadr_reaper/cue_take_count_adapter.cpp reaadr_reaper/cue_info_command.cpp reaadr_reaper/character_filter_adapter.cpp reaadr_reaper/character_filter_command.cpp \
	reaadr_reaper/cue_navigation_service.cpp reaadr_reaper/record_arm_adapter.cpp reaadr_reaper/recording_setup_adapter.cpp \
	reaadr_reaper/recording_transport_executor.cpp reaadr_reaper/recording_command_context.cpp reaadr_reaper/recording_command.cpp reaadr_reaper/recording_action.cpp \
	reaadr_reaper/overlay_refresh_adapter.cpp reaadr_reaper/cue_cleanup_adapter.cpp \
	app/cue_info_application_service.cpp app/cue_status_application_service.cpp app/recording_application_service.cpp app/recording_target_application_service.cpp \
	app/recording_workflow_service.cpp app/recording_session_service.cpp app/overlay_application_service.cpp app/cue_import_application_service.cpp \
	app/marker_cue_generation_application_service.cpp app/dialogue_cue_generation_application_service.cpp app/dialogue_detection_application_service.cpp \
	app/cue_cleanup_application_service.cpp app/character_filter_application_service.cpp app/manager_view_application_service.cpp \
	app/cue_manager_application_service.cpp app/session_refresh_application_service.cpp app/region_timing_application_service.cpp
REAPER_HEADERS := reaadr_reaper/project_state.hpp reaadr_reaper/project_transaction.hpp reaadr_reaper/track_region_adapter.hpp \
	reaadr_reaper/render_artifact_adapter.hpp reaadr_reaper/session_render_service.hpp reaadr_reaper/native_host_services.hpp reaadr_reaper/cue_manager_session.hpp \
	reaadr_reaper/marker_snapshot_adapter.hpp reaadr_reaper/marker_cue_generation_command.hpp reaadr_reaper/dialogue_detection_adapter.hpp \
	reaadr_reaper/dialogue_detection_command.hpp reaadr_reaper/cue_status_command.hpp reaadr_reaper/cue_take_count_adapter.hpp reaadr_reaper/cue_info_command.hpp reaadr_reaper/character_filter_adapter.hpp reaadr_reaper/character_filter_command.hpp \
	reaadr_reaper/cue_navigation_service.hpp reaadr_reaper/record_arm_adapter.hpp reaadr_reaper/recording_setup_adapter.hpp \
	reaadr_reaper/recording_transport_executor.hpp reaadr_reaper/recording_command_context.hpp reaadr_reaper/recording_command.hpp reaadr_reaper/recording_action.hpp \
	reaadr_reaper/overlay_refresh_adapter.hpp reaadr_reaper/cue_cleanup_adapter.hpp app/cue_info_application_service.hpp app/cue_status_application_service.hpp \
	app/recording_application_service.hpp app/recording_target_application_service.hpp app/recording_workflow_service.hpp \
	app/recording_session_service.hpp app/overlay_application_service.hpp app/cue_import_application_service.hpp \
	app/marker_cue_generation_application_service.hpp app/dialogue_cue_generation_application_service.hpp app/dialogue_detection_application_service.hpp \
	app/cue_cleanup_application_service.hpp app/character_filter_application_service.hpp app/manager_view_application_service.hpp \
	app/cue_manager_application_service.hpp app/session_refresh_application_service.hpp app/region_timing_application_service.hpp \
	reaadr_ui/character_filter_window.hpp reaadr_ui/cue_info_controller.hpp reaadr_ui/cue_info_window.hpp reaadr_ui/cue_status_window.hpp reaadr_ui/win32_utf8.hpp
NATIVE_SOURCES := $(CORE_SOURCES) $(REAPER_SOURCES) $(UI_SOURCES)
NATIVE_HEADERS := $(CORE_HEADERS) $(REAPER_HEADERS)
EXTENSION_SOURCES := reaper_reaadr.cpp $(NATIVE_SOURCES)
.PHONY: all clean dist prepare-sdk test
all: $(BUILD_DIR)/$(TARGET)
prepare-sdk:
	@if [ ! -d "$(REAPER_SDK)/sdk" ] || [ ! -d "$(WDL)/swell" ]; then echo "Missing SDK dependencies. Clone reaper-sdk to vendor/reaper-sdk and WDL to vendor/WDL before building."; exit 1; fi
	@if [ ! -e "$(REAPER_SDK)/WDL" ]; then ln -s ../WDL/WDL "$(REAPER_SDK)/WDL"; fi
$(BUILD_DIR)/$(TARGET): $(EXTENSION_SOURCES) $(NATIVE_HEADERS) | prepare-sdk
	@mkdir -p "$(BUILD_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(EXTENSION_SOURCES) -o $@ $(LDFLAGS)
$(TEST_TARGET): ../tests/session_model_tests.cpp ../tests/reaper_api_test_stubs.cpp $(EXTENSION_SOURCES) $(NATIVE_HEADERS)
	@mkdir -p "$(BUILD_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) ../tests/session_model_tests.cpp ../tests/reaper_api_test_stubs.cpp $(EXTENSION_SOURCES) -o $@ -ldl
$(MARKER_TEST_TARGET): ../tests/marker_cue_generation_tests.cpp reaadr_core/marker_cue_generation.cpp reaadr_reaper/marker_snapshot_adapter.cpp reaadr_core/marker_cue_generation.hpp reaadr_reaper/marker_snapshot_adapter.hpp reaadr_core/session_model.hpp
	@mkdir -p "$(BUILD_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) ../tests/marker_cue_generation_tests.cpp reaadr_core/marker_cue_generation.cpp reaadr_reaper/marker_snapshot_adapter.cpp -o $@
$(DIALOGUE_TEST_TARGET): ../tests/dialogue_cue_generation_tests.cpp reaadr_core/dialogue_cue_generation.cpp reaadr_core/dialogue_cue_generation.hpp reaadr_core/session_model.hpp
	@mkdir -p "$(BUILD_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) ../tests/dialogue_cue_generation_tests.cpp reaadr_core/dialogue_cue_generation.cpp -o $@
$(DIALOGUE_DETECTION_TEST_TARGET): ../tests/dialogue_detection_adapter_tests.cpp reaadr_reaper/dialogue_detection_adapter.cpp reaadr_reaper/dialogue_detection_adapter.hpp reaadr_core/dialogue_cue_generation.hpp reaadr_core/session_model.hpp
	@mkdir -p "$(BUILD_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) ../tests/dialogue_detection_adapter_tests.cpp reaadr_reaper/dialogue_detection_adapter.cpp -o $@
$(RECORDING_TARGET_TEST_TARGET): ../tests/recording_target_tests.cpp reaadr_core/recording_target.cpp reaadr_core/cue_navigation.cpp reaadr_core/lane_assignment.cpp reaadr_core/character_filter.cpp reaadr_core/domain_utils.cpp reaadr_core/render_plan.cpp reaadr_core/session_model.cpp
	@mkdir -p "$(BUILD_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) ../tests/recording_target_tests.cpp reaadr_core/recording_target.cpp reaadr_core/cue_navigation.cpp reaadr_core/lane_assignment.cpp reaadr_core/character_filter.cpp reaadr_core/domain_utils.cpp reaadr_core/render_plan.cpp reaadr_core/session_model.cpp -o $@
$(RECORDING_SHUTDOWN_TEST_TARGET): ../tests/recording_shutdown_tests.cpp reaadr_core/recording_transport.cpp reaadr_core/recording_transport.hpp
	@mkdir -p "$(BUILD_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) ../tests/recording_shutdown_tests.cpp reaadr_core/recording_transport.cpp -o $@
$(CUE_INFO_TEST_TARGET): ../tests/cue_info_tests.cpp reaadr_core/cue_info.cpp reaadr_core/domain_utils.cpp reaadr_core/cue_info.hpp reaadr_core/domain_utils.hpp reaadr_core/session_model.hpp
	@mkdir -p "$(BUILD_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) ../tests/cue_info_tests.cpp reaadr_core/cue_info.cpp reaadr_core/domain_utils.cpp -o $@
$(CUE_TAKE_COUNT_TEST_TARGET): ../tests/cue_take_count_adapter_tests.cpp reaadr_reaper/cue_take_count_adapter.cpp reaadr_reaper/cue_take_count_adapter.hpp reaadr_core/session_model.hpp
	@mkdir -p "$(BUILD_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) ../tests/cue_take_count_adapter_tests.cpp reaadr_reaper/cue_take_count_adapter.cpp -o $@
$(CUE_DIRECTION_EDIT_TEST_TARGET): ../tests/cue_direction_edit_tests.cpp reaadr_core/cue_manager_model.cpp reaadr_core/domain_utils.cpp reaadr_core/session_model.cpp reaadr_core/model_repository.cpp reaadr_ui/cue_manager_ui_contract.cpp reaadr_core/cue_manager_model.hpp reaadr_core/domain_utils.hpp reaadr_core/session_model.hpp reaadr_core/model_repository.hpp reaadr_ui/cue_manager_ui_contract.hpp
	@mkdir -p "$(BUILD_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) ../tests/cue_direction_edit_tests.cpp reaadr_core/cue_manager_model.cpp reaadr_core/domain_utils.cpp reaadr_core/session_model.cpp reaadr_core/model_repository.cpp reaadr_ui/cue_manager_ui_contract.cpp -o $@
$(MANAGER_NAVIGATION_TEST_TARGET): ../tests/manager_navigation_tests.cpp reaadr_ui/manager_navigation.cpp reaadr_ui/manager_navigation.hpp reaadr_core/model_repository.hpp app/manager_window_slot_service.hpp
	@mkdir -p "$(BUILD_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) ../tests/manager_navigation_tests.cpp reaadr_ui/manager_navigation.cpp -o $@
$(CUE_MANAGER_SESSION_TEST_TARGET): ../tests/cue_manager_session_tests.cpp reaadr_reaper/cue_manager_session.hpp
	@mkdir -p "$(BUILD_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) ../tests/cue_manager_session_tests.cpp -o $@
$(QUICK_ACTION_TEST_TARGET): ../tests/quick_action_tests.cpp reaadr_core/manager_preferences.cpp reaadr_core/manager_preferences.hpp reaadr_core/overlay_settings.cpp reaadr_core/overlay_settings.hpp reaadr_core/model_repository.cpp reaadr_core/model_repository.hpp reaadr_core/session_model.cpp reaadr_core/session_model.hpp reaadr_core/domain_utils.cpp reaadr_core/domain_utils.hpp app/quick_action_application_service.hpp
	@mkdir -p "$(BUILD_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) ../tests/quick_action_tests.cpp reaadr_core/manager_preferences.cpp reaadr_core/overlay_settings.cpp reaadr_core/model_repository.cpp reaadr_core/session_model.cpp reaadr_core/domain_utils.cpp -o $@
test: $(TEST_TARGET) $(MARKER_TEST_TARGET) $(DIALOGUE_TEST_TARGET) $(DIALOGUE_DETECTION_TEST_TARGET) $(RECORDING_TARGET_TEST_TARGET) $(RECORDING_SHUTDOWN_TEST_TARGET) $(CUE_INFO_TEST_TARGET) $(CUE_TAKE_COUNT_TEST_TARGET) $(CUE_DIRECTION_EDIT_TEST_TARGET) $(MANAGER_NAVIGATION_TEST_TARGET) $(CUE_MANAGER_SESSION_TEST_TARGET) $(QUICK_ACTION_TEST_TARGET)
	$(TEST_TARGET)
	$(MARKER_TEST_TARGET)
	$(DIALOGUE_TEST_TARGET)
	$(DIALOGUE_DETECTION_TEST_TARGET)
	$(RECORDING_TARGET_TEST_TARGET)
	$(RECORDING_SHUTDOWN_TEST_TARGET)
	$(CUE_INFO_TEST_TARGET)
	$(CUE_TAKE_COUNT_TEST_TARGET)
	$(CUE_DIRECTION_EDIT_TEST_TARGET)
	$(MANAGER_NAVIGATION_TEST_TARGET)
	$(CUE_MANAGER_SESSION_TEST_TARGET)
	$(QUICK_ACTION_TEST_TARGET)
dist: all
	rm -rf "$(DIST_DIR)"
	@mkdir -p "$(DIST_USERPLUGINS_DIR)" "$(DIST_REAADR_DIR)/scripts" "$(DIST_REAADR_DIR)/assets"
	cp "$(BUILD_DIR)/$(TARGET)" "$(DIST_USERPLUGINS_DIR)/$(TARGET)"
	cp ../scripts/*.lua "$(DIST_REAADR_DIR)/scripts/"
	cp ../assets/* "$(DIST_REAADR_DIR)/assets/"
clean:
	rm -rf "$(BUILD_DIR)" ../dist
