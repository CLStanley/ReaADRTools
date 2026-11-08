#pragma once

#include "app/manager_view_application_service.hpp"
#include "app/cue_manager_application_service.hpp"

#include <array>
#include <cstring>
#include <sstream>
#include <string>

namespace reaadr::reaper {

// REAPER dialog entry points are supplied by the compatibility host. This
// fallback is used only when the native SWELL Cue Manager cannot open.
struct CueManagerFallbackHost {
  bool (*get_user_inputs)(const char*, int, const char*, char*, int) = nullptr;
  void (*show_message)(const std::string&, const std::string&) = nullptr;
};

inline void show_cue_manager_fallback(
  ManagerViewApplicationService& service,
  CueManagerApplicationService& mutations,
  const CueManagerFallbackHost& host)
{
  if (!host.show_message) return;
  core::CueManagerViewOptions view_options;
  if (host.get_user_inputs) {
    std::array<char, 1024> filter_input = {};
    if (host.get_user_inputs("ReaADR Cue Manager: Filter", 3, "Search,Character,Status",
                             filter_input.data(), static_cast<int>(filter_input.size()))) {
      char* first = std::strchr(filter_input.data(), ',');
      char* second = first ? std::strchr(first + 1, ',') : nullptr;
      if (first && second) {
        *first = '\0';
        *second = '\0';
        view_options.query = filter_input.data();
        view_options.character = first + 1;
        view_options.status = second + 1;
      }
    }
  }
  const auto loaded = service.load(view_options, "cues");
  if (!loaded) {
    host.show_message(loaded.error, "ReaADR Cue Manager");
    return;
  }
  const auto& view = loaded.view.cues;
  std::ostringstream summary;
  summary << "Session: " << view.session_id << "\n"
          << "Showing " << view.rows.size() << " of " << loaded.view.total_cues << " cues\n\n";
  for (const auto& row : view.rows) {
    summary << (row.selected ? "> " : "  ") << row.cue_key << "  " << row.character
            << "  [" << row.status << "]  " << row.dialogue << "\n";
  }
  host.show_message(summary.str(), "ReaADR Cue Manager (Native)");
  if (!host.get_user_inputs) return;
  std::array<char, 1024> input = {};
  if (!host.get_user_inputs("ReaADR Cue Manager: Set Status", 2, "Cue ID,Status",
                            input.data(), static_cast<int>(input.size()))) return;
  char* comma = std::strchr(input.data(), ',');
  if (!comma || comma == input.data() || *(comma + 1) == '\0') {
    host.show_message("Enter a cue ID and status separated by a comma.", "ReaADR Cue Manager");
    return;
  }
  *comma = '\0';
  core::CueManagerEditOptions options;
  options.cue_key = input.data();
  options.status = comma + 1;
  const auto updated = mutations.edit(options);
  if (!updated) host.show_message(updated.error, "ReaADR Cue Manager");
}

} // namespace reaadr::reaper
