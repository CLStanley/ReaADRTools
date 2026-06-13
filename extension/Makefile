REAPER_SDK := ../vendor/reaper-sdk
WDL := ../vendor/WDL/WDL
TARGET := reaper_reaadr-x86_64.so
BUILD_DIR := ../build/extension
DIST_DIR := ../dist/UserPlugins

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
	@mkdir -p "$(DIST_DIR)/ReaADRTools/scripts" "$(DIST_DIR)/ReaADRTools/assets"
	cp "$(BUILD_DIR)/$(TARGET)" "$(DIST_DIR)/$(TARGET)"
	cp ../scripts/ReaADR_App.lua "$(DIST_DIR)/ReaADRTools/scripts/"
	cp ../scripts/ReaADR_Open_Manager.lua "$(DIST_DIR)/ReaADRTools/scripts/"
	cp ../scripts/ReaADR_Import_Script.lua "$(DIST_DIR)/ReaADRTools/scripts/"
	cp ../scripts/ReaADR_Export_Reports.lua "$(DIST_DIR)/ReaADRTools/scripts/"
	cp ../scripts/ReaADR_Preferences.lua "$(DIST_DIR)/ReaADRTools/scripts/"
	cp ../scripts/ReaADR_Quick_Action_1.lua "$(DIST_DIR)/ReaADRTools/scripts/"
	cp ../scripts/ReaADR_Quick_Action_2.lua "$(DIST_DIR)/ReaADRTools/scripts/"
	cp ../scripts/ReaADR_Quick_Action_3.lua "$(DIST_DIR)/ReaADRTools/scripts/"
	cp ../scripts/ReaADR_Quick_Action_4.lua "$(DIST_DIR)/ReaADRTools/scripts/"
	cp ../scripts/ReaADR_Core.lua "$(DIST_DIR)/ReaADRTools/scripts/"
	cp ../scripts/ReaADR_Import_Cue_Sheet.lua "$(DIST_DIR)/ReaADRTools/scripts/"
	cp ../scripts/ReaADR_Export_Cue_Sheet.lua "$(DIST_DIR)/ReaADRTools/scripts/"
	cp ../scripts/ReaADR_Next_Cue.lua "$(DIST_DIR)/ReaADRTools/scripts/"
	cp ../scripts/ReaADR_Previous_Cue.lua "$(DIST_DIR)/ReaADRTools/scripts/"
	cp ../scripts/ReaADR_Jump_To_Cue.lua "$(DIST_DIR)/ReaADRTools/scripts/"
	cp ../scripts/ReaADR_Set_Cue_Status.lua "$(DIST_DIR)/ReaADRTools/scripts/"
	cp ../scripts/ReaADR_Character_Filter.lua "$(DIST_DIR)/ReaADRTools/scripts/"
	cp ../scripts/ReaADR_Cue_Manager.lua "$(DIST_DIR)/ReaADRTools/scripts/"
	cp ../scripts/ReaADR_Cue_Info_Panel.lua "$(DIST_DIR)/ReaADRTools/scripts/"
	cp ../scripts/ReaADR_Generate_Cues.lua "$(DIST_DIR)/ReaADRTools/scripts/"
	cp ../scripts/ReaADR_Clean_Generated_Cues.lua "$(DIST_DIR)/ReaADRTools/scripts/"
	cp ../scripts/ReaADR_Overlay_Settings.lua "$(DIST_DIR)/ReaADRTools/scripts/"
	cp ../scripts/ReaADR_Menu.lua "$(DIST_DIR)/ReaADRTools/scripts/"
	cp ../assets/cue.wav "$(DIST_DIR)/ReaADRTools/assets/"

clean:
	rm -rf "$(BUILD_DIR)" ../dist
