#include "reaadr_core/manager_preferences.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const char* message)
{
  if (!condition) {
    std::cerr << "quick_action_tests: " << message << '\n';
    std::exit(1);
  }
}

} // namespace

int main()
{
  using namespace reaadr::core;

  ManagerPreferences preferences;
  preferences.quick_actions = {
    "record_cue", "character_filter", "refresh_overlay", "validate",
  };

  const auto record = resolve_manager_quick_action(preferences, 1);
  require(static_cast<bool>(record), "configured record quick action should resolve");
  require(record.quick_action.key == "record_cue", "record slot should preserve configured key");
  require(record.quick_action.action == "record_cue", "record slot should route to native record action");
  require(!record.used_default, "known configured action should not use fallback");

  const auto filter = resolve_manager_quick_action(preferences, 2);
  require(static_cast<bool>(filter) && filter.quick_action.action == "character_filter",
          "character filter should resolve to native semantic action");

  const auto overlay = resolve_manager_quick_action(preferences, 3);
  require(static_cast<bool>(overlay) && overlay.quick_action.action == "refresh_overlay",
          "refresh overlay should resolve to native semantic action");

  const auto validate = resolve_manager_quick_action(preferences, 4);
  require(static_cast<bool>(validate) && validate.quick_action.action == "validate_session",
          "validate quick action should preserve Lua app-action semantics");

  const auto menu = build_manager_menu(preferences);
  require(static_cast<bool>(menu), "configured native ReaADR menu should build");
  require(menu.entries.size() == 5, "native ReaADR menu should contain Manager plus four quick actions");
  require(menu.entries[0].label == "Open Manager" &&
          menu.entries[0].action == "cue_manager" &&
          menu.entries[0].quick_action_slot == 0,
          "Open Manager should remain the first fixed native menu entry");
  require(menu.entries[1].label == "Record Current Cue" &&
          menu.entries[1].action == "record_cue" &&
          menu.entries[1].quick_action_slot == 1,
          "slot one should expose its configured record action");
  require(menu.entries[2].label == "Character Filter" && menu.entries[2].quick_action_slot == 2,
          "slot two should preserve configured ordering");
  require(menu.entries[3].label == "Refresh Video Overlay" && menu.entries[3].quick_action_slot == 3,
          "slot three should preserve configured ordering");
  require(menu.entries[4].label == "Check Session" &&
          menu.entries[4].action == "validate_session" &&
          menu.entries[4].quick_action_slot == 4,
          "slot four should preserve configured validation routing");

  preferences.quick_actions[0] = "not_a_real_action";
  const auto fallback = resolve_manager_quick_action(preferences, 1);
  require(static_cast<bool>(fallback), "invalid persisted key should fall back instead of failing");
  require(fallback.used_default, "invalid persisted key should report default fallback");
  require(fallback.quick_action.key == "import",
          "slot one invalid persisted key should fall back to Lua default import action");

  const auto fallback_menu = build_manager_menu(preferences);
  require(static_cast<bool>(fallback_menu), "menu should tolerate an invalid persisted quick-action key");
  require(fallback_menu.entries[1].label == "Import Cue Sheet" &&
          fallback_menu.entries[1].action == "import",
          "menu should expose the slot default when persisted configuration is invalid");

  require(!resolve_manager_quick_action(preferences, 0), "slot zero should be rejected");
  require(!resolve_manager_quick_action(preferences, 5), "slot five should be rejected");

  const auto& actions = manager_quick_actions();
  require(actions.size() == 8, "native quick action catalog should match Lua's eight choices");
  require(actions[0].key == "import" && actions[0].label == "Import Cue Sheet",
          "native quick action labels should match Lua");
  require(actions[3].action == "export_reports",
          "export reports should preserve Lua app-action routing");
  require(actions[4].action == "open_overlay_manager",
          "overlay settings should preserve Lua Manager-tab routing");

  std::cout << "quick_action_tests: ok\n";
  return 0;
}
