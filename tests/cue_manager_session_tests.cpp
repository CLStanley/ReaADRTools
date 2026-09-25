#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

// WDL/SWELL intentionally provides Win32-compatible min/max macros on
// non-Windows hosts. Pull the C++ standard-library headers in first so those
// macros cannot rewrite std::min/std::max inside libstdc++ headers.
#include "reaadr_reaper/native_runtime.hpp"

int main()
{
  using Session = reaadr::reaper::CueManagerSession;
  using Host = reaadr::reaper::CueManagerSessionHost;
  using ShutdownSignature = bool (Host::*)(std::string*);
  using ProjectSignature = ReaProject* (Session::*)() const;
  using ImportSignature = reaadr::reaper::CueImportApplicationResult (Host::*)(
    const std::string&,
    const std::string&,
    const std::optional<reaadr::core::ColumnMapping>&,
    const std::string&,
    const std::vector<std::string>&);
  using SyncSignature = bool (Host::*)(std::string&);
  using CleanupSignature = reaadr::reaper::CueCleanupApplicationResult (Host::*)(
    const std::vector<std::string>&, std::string&);
  using LoadSignature = reaadr::core::SessionLoadResult (Host::*)() const;
  using RuntimeInitSignature = bool (*)(reaper_plugin_info_t*, std::string*);
  using RuntimeShutdownSignature = bool (*)(reaper_plugin_info_t*, std::string*);

  static_assert(!std::is_copy_constructible_v<Session>,
                "CueManagerSession must uniquely own its service graph");
  static_assert(!std::is_copy_assignable_v<Session>,
                "CueManagerSession must not duplicate controller/service references");
  static_assert(std::is_default_constructible_v<reaadr::reaper::CueManagerSessionConfig>,
                "CueManagerSessionConfig should remain a lightweight host handoff");
  static_assert(std::is_default_constructible_v<Host>,
                "CueManagerSessionHost should be constructible before REAPER opens the Manager");
  static_assert(std::is_same_v<decltype(&Host::shutdown), ShutdownSignature>,
                "CueManagerSessionHost must expose an unload-safe shutdown contract");
  static_assert(std::is_same_v<decltype(&Session::project), ProjectSignature>,
                "CueManagerSession must expose its bound REAPER project for safe rebinding");
  static_assert(std::is_same_v<decltype(&Host::import_content), ImportSignature>,
                "Manager imports must execute through the persistent session host");
  static_assert(std::is_same_v<decltype(&Host::sync_regions), SyncSignature>,
                "Region synchronization must execute through the persistent session host");
  static_assert(std::is_same_v<decltype(&Host::clear_characters), CleanupSignature>,
                "Character cleanup must execute through the persistent session host");
  static_assert(std::is_same_v<decltype(&Host::load_session), LoadSignature>,
                "Manager exports must read through the persistent session host");
  static_assert(std::is_same_v<decltype(&reaadr::reaper::initialize_native_runtime), RuntimeInitSignature>,
                "Native runtime initialization must stay a small plug-in host handoff");
  static_assert(std::is_same_v<decltype(&reaadr::reaper::shutdown_native_runtime), RuntimeShutdownSignature>,
                "Native runtime shutdown must close Manager ownership before unregistering actions");

  Host host;
  if (host.has_session() || host.session() != nullptr) {
    std::cerr << "FAIL: persistent Cue Manager host must start empty.\n";
    return 1;
  }

  std::cout << "Cue Manager session ownership tests passed.\n";
  return 0;
}
