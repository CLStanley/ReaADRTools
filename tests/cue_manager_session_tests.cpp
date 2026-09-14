#include "reaadr_reaper/cue_manager_session.hpp"

#include <iostream>
#include <string>
#include <type_traits>

int main()
{
  static_assert(!std::is_copy_constructible_v<reaadr::reaper::CueManagerSession>,
                "CueManagerSession must uniquely own its service graph");
  static_assert(!std::is_copy_assignable_v<reaadr::reaper::CueManagerSession>,
                "CueManagerSession must not duplicate controller/service references");
  static_assert(std::is_default_constructible_v<reaadr::reaper::CueManagerSessionConfig>,
                "CueManagerSessionConfig should remain a lightweight host handoff");
  static_assert(std::is_default_constructible_v<reaadr::reaper::CueManagerSessionHost>,
                "CueManagerSessionHost should be constructible before REAPER opens the Manager");

  reaadr::reaper::CueManagerSessionHost host;
  if (host.has_session() || host.session() != nullptr) {
    std::cerr << "FAIL: persistent Cue Manager host must start empty.\n";
    return 1;
  }

  std::string shutdown_error = "stale";
  if (!host.shutdown(&shutdown_error) || !shutdown_error.empty() || host.has_session()) {
    std::cerr << "FAIL: shutting down an empty Manager host must be safe and idempotent.\n";
    return 1;
  }
  if (!host.shutdown()) {
    std::cerr << "FAIL: repeated empty Manager shutdown must remain safe.\n";
    return 1;
  }

  std::cout << "Cue Manager session ownership tests passed.\n";
  return 0;
}
