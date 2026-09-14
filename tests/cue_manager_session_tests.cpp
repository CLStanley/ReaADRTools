#include "reaadr_reaper/cue_manager_session.hpp"

#include <iostream>
#include <string>
#include <type_traits>

int main()
{
  using Host = reaadr::reaper::CueManagerSessionHost;
  using ShutdownSignature = bool (Host::*)(std::string*);

  static_assert(!std::is_copy_constructible_v<reaadr::reaper::CueManagerSession>,
                "CueManagerSession must uniquely own its service graph");
  static_assert(!std::is_copy_assignable_v<reaadr::reaper::CueManagerSession>,
                "CueManagerSession must not duplicate controller/service references");
  static_assert(std::is_default_constructible_v<reaadr::reaper::CueManagerSessionConfig>,
                "CueManagerSessionConfig should remain a lightweight host handoff");
  static_assert(std::is_default_constructible_v<Host>,
                "CueManagerSessionHost should be constructible before REAPER opens the Manager");
  static_assert(std::is_same_v<decltype(&Host::shutdown), ShutdownSignature>,
                "CueManagerSessionHost must expose an unload-safe shutdown contract");

  Host host;
  if (host.has_session() || host.session() != nullptr) {
    std::cerr << "FAIL: persistent Cue Manager host must start empty.\n";
    return 1;
  }

  std::cout << "Cue Manager session ownership tests passed.\n";
  return 0;
}
