#pragma once

#include <functional>
#include <string>

namespace reaadr::reaper {

// Host-independent routing for actions originating in the native Cue Manager.
// REAPER command IDs and UI callbacks are injected by the host.
struct CueManagerActionHost {
  std::function<void(const std::string&)> command;
  std::function<void(const std::string&)> export_report;
  std::function<void(const std::string&, const std::string&)> setting;
};

inline bool dispatch_cue_manager_action(const std::string& action,
                                        const CueManagerActionHost& host)
{
  if (action == "validate_session" || action == "refresh_session" ||
      action == "sync_regions" || action == "clear_character_cues" ||
      action == "character_filter" || action == "refresh_overlay" ||
      action == "preferences") {
    if (host.command) host.command(action);
    return true;
  }
  if (action == "export_cue_sheet" || action == "export_timing_report" ||
      action == "export_session_metadata") {
    if (host.export_report) host.export_report(action);
    return true;
  }
  for (const char* prefix : {"overlay_profile:", "overlay_toggle:",
                              "overlay_text_color:", "overlay_settings:",
                              "quick_actions:", "preference_toggles:"}) {
    const std::string key(prefix);
    if (action.compare(0, key.size(), key) == 0) {
      if (host.setting) host.setting(key.substr(0, key.size() - 1),
                                     action.substr(key.size()));
      return true;
    }
  }
  return false;
}

} // namespace reaadr::reaper
