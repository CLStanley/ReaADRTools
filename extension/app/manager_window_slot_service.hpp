#pragma once

#include "../reaadr_core/model_repository.hpp"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <string>

namespace reaadr::reaper {

struct ManagerWindowSlotClaim {
  int slot = 0;
  std::string error;

  explicit operator bool() const { return error.empty() && slot > 0; }
};

// Native compatibility service for the three Lua Manager instance slots.
// Slots are project-scoped and considered live while either an active heartbeat
// or an in-progress launch timestamp is no more than two seconds old.
class ManagerWindowSlotService final {
public:
  static constexpr int kMaxSlots = 3;
  static constexpr double kStaleSeconds = 2.0;

  ManagerWindowSlotService(core::ProjectStateStore& state, double (*now_seconds)())
    : state_(state), now_seconds_(now_seconds) {}

  ManagerWindowSlotClaim claim()
  {
    ManagerWindowSlotClaim result;
    if (!now_seconds_) {
      result.error = "Manager window slot timing is unavailable.";
      return result;
    }
    const double now = now_seconds_();
    if (!std::isfinite(now)) {
      result.error = "Manager window slot timing is invalid.";
      return result;
    }

    for (int slot = 1; slot <= kMaxSlots; ++slot) {
      const bool active = read(slot, "active") == "1";
      const double heartbeat = number(read(slot, "heartbeat"));
      const double launching = number(read(slot, "launching"));
      const bool slot_busy = active && (now - heartbeat) <= kStaleSeconds;
      const bool slot_launching = launching > 0.0 && (now - launching) <= kStaleSeconds;
      if (slot_busy || slot_launching) continue;

      if (!write(slot, "active", "1") ||
          !write(slot, "heartbeat", std::to_string(now)) ||
          !write(slot, "launching", std::to_string(now))) {
        release(slot);
        result.error = "Could not claim a native Manager window slot.";
        return result;
      }
      result.slot = slot;
      return result;
    }

    result.error = "Three ReaADR manager windows are already open. Close one before opening another.";
    return result;
  }

  bool heartbeat(int slot)
  {
    if (!valid_slot(slot) || !now_seconds_) return false;
    const double now = now_seconds_();
    if (!std::isfinite(now)) return false;
    return write(slot, "active", "1") &&
      write(slot, "heartbeat", std::to_string(now)) &&
      write(slot, "launching", "");
  }

  bool release(int slot)
  {
    if (!valid_slot(slot)) return false;
    const bool active = write(slot, "active", "");
    const bool heartbeat_cleared = write(slot, "heartbeat", "");
    const bool launching = write(slot, "launching", "");
    const bool launch_tab = write(slot, "launch_tab", "");
    return active && heartbeat_cleared && launching && launch_tab;
  }

  bool set_launch_tab(int slot, const std::string& tab)
  {
    return valid_slot(slot) && write(slot, "launch_tab", tab);
  }

  std::string consume_launch_tab(int slot)
  {
    if (!valid_slot(slot)) return {};
    const std::string tab = read(slot, "launch_tab");
    write(slot, "launch_tab", "");
    return tab;
  }

private:
  static bool valid_slot(int slot) { return slot >= 1 && slot <= kMaxSlots; }

  static double number(const std::string& value)
  {
    if (value.empty()) return 0.0;
    char* end = nullptr;
    const double parsed = std::strtod(value.c_str(), &end);
    return end && *end == '\0' && std::isfinite(parsed) ? parsed : 0.0;
  }

  std::string key(int slot, const char* suffix) const
  {
    return "ui.manager_slot." + std::to_string(slot) + "." + suffix;
  }

  std::string read(int slot, const char* suffix) const
  {
    const auto result = state_.read(core::SessionModelRepository::kNamespace,
                                    key(slot, suffix).c_str());
    return result ? result.value : std::string();
  }

  bool write(int slot, const char* suffix, const std::string& value)
  {
    return state_.write(core::SessionModelRepository::kNamespace,
                        key(slot, suffix).c_str(), value);
  }

  core::ProjectStateStore& state_;
  double (*now_seconds_)() = nullptr;
};

} // namespace reaadr::reaper
