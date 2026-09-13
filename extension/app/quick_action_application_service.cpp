#include "quick_action_application_service.hpp"

namespace reaadr::reaper {

QuickActionApplicationResult QuickActionApplicationService::run(std::size_t slot)
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

} // namespace reaadr::reaper
