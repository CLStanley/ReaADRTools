#include "reaadr_reaper/cue_manager_action_dispatch.hpp"

#include <cassert>
#include <string>
#include <vector>

int main()
{
  using reaadr::reaper::CueManagerActionHost;
  using reaadr::reaper::dispatch_cue_manager_action;
  std::vector<std::string> calls;
  const CueManagerActionHost host{
    [&](const std::string& action) { calls.push_back("command:" + action); },
    [&](const std::string& action) { calls.push_back("export:" + action); },
    [&](const std::string& key, const std::string& value) {
      calls.push_back("setting:" + key + "=" + value);
    }
  };
  for (const auto& action : {"validate_session", "refresh_session", "sync_regions",
                              "clear_character_cues", "character_filter",
                              "refresh_overlay", "preferences"}) {
    assert(dispatch_cue_manager_action(action, host));
    assert(calls.back() == std::string("command:") + action);
  }
  for (const auto& action : {"export_cue_sheet", "export_timing_report",
                              "export_session_metadata"}) {
    assert(dispatch_cue_manager_action(action, host));
    assert(calls.back() == std::string("export:") + action);
  }
  for (const auto& key : {"overlay_profile", "overlay_toggle",
                          "overlay_text_color", "overlay_settings",
                          "quick_actions", "preference_toggles"}) {
    const std::string action = std::string(key) + ":value:with:colons";
    assert(dispatch_cue_manager_action(action, host));
    assert(calls.back() == std::string("setting:") + key + "=value:with:colons");
  }
  const auto count = calls.size();
  assert(!dispatch_cue_manager_action("unknown_action", host));
  assert(!dispatch_cue_manager_action("overlay_profile", host));
  assert(calls.size() == count);
  assert(dispatch_cue_manager_action("preferences", {}));
  return 0;
}
