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

  QuickActionApplicationResult run(std::size_t slot);

private:
  core::ManagerPreferencesRepository& preferences_;
  QuickActionDispatcher dispatcher_;
};

} // namespace reaadr::reaper
