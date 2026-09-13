#pragma once

#include "../reaadr_core/manager_preferences.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <utility>

namespace reaadr::reaper {

struct QuickActionApplicationResult {
  core::ManagerQuickAction quick_action;
  bool used_default = false;
  bool dispatched = false;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

using QuickActionDispatcher =
  std::function<bool(const std::string& action, std::string* error)>;

// Native application boundary for ReaADR_App.run_quick_action(slot): load the
// persisted Manager preferences, resolve Lua-compatible slot fallback rules,
// then dispatch one semantic action. UI/host code owns the actual action
// callbacks so this service stays independent from REAPER command IDs.
class QuickActionApplicationService final {
public:
  QuickActionApplicationService(core::ManagerPreferencesRepository& preferences,
                                QuickActionDispatcher dispatcher)
    : preferences_(preferences), dispatcher_(std::move(dispatcher)) {}

  QuickActionApplicationResult run(std::size_t slot)
  {
    QuickActionApplicationResult result;
    const auto loaded = preferences_.load();
    if (!loaded) {
      result.error = loaded.error;
      return result;
    }

    const auto resolved = core::resolve_manager_quick_action(loaded.preferences, slot);
    if (!resolved) {
      result.error = resolved.error;
      return result;
    }
    result.quick_action = resolved.quick_action;
    result.used_default = resolved.used_default;

    if (!dispatcher_) {
      result.error = "The native quick-action dispatcher is unavailable.";
      return result;
    }

    std::string dispatch_error;
    if (!dispatcher_(resolved.quick_action.action, &dispatch_error)) {
      result.error = dispatch_error.empty()
        ? "The native quick action could not be dispatched."
        : dispatch_error;
      return result;
    }

    result.dispatched = true;
    return result;
  }

private:
  core::ManagerPreferencesRepository& preferences_;
  QuickActionDispatcher dispatcher_;
};

} // namespace reaadr::reaper
