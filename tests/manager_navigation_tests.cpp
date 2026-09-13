#include "app/manager_window_slot_service.hpp"
#include "reaadr_ui/manager_navigation.hpp"

#include <iostream>
#include <map>
#include <string>

namespace {
int failures = 0;
double current_time = 10.0;

void check(bool condition, const std::string& message)
{
  if (condition) return;
  ++failures;
  std::cerr << "FAIL: " << message << '\n';
}

double now_seconds()
{
  return current_time;
}

class FakeProjectStateStore final : public reaadr::core::ProjectStateStore {
public:
  reaadr::core::StateReadResult read(const char* name_space, const char* key) const override
  {
    const std::string composite = std::string(name_space ? name_space : "") + "\n" +
      std::string(key ? key : "");
    const auto found = values.find(composite);
    if (found == values.end()) return {{}, reaadr::core::StateReadError::not_found};
    return {found->second, reaadr::core::StateReadError::none};
  }

  bool write(const char* name_space, const char* key, const std::string& value) override
  {
    const std::string composite = std::string(name_space ? name_space : "") + "\n" +
      std::string(key ? key : "");
    if (value.empty()) values.erase(composite);
    else values[composite] = value;
    return true;
  }

  std::string get(const std::string& key) const
  {
    const auto found = values.find(
      std::string(reaadr::core::SessionModelRepository::kNamespace) + "\n" + key);
    return found == values.end() ? std::string() : found->second;
  }

  void set(const std::string& key, const std::string& value)
  {
    values[std::string(reaadr::core::SessionModelRepository::kNamespace) + "\n" + key] = value;
  }

  std::map<std::string, std::string> values;
};

void test_manager_window_slots()
{
  FakeProjectStateStore state;
  reaadr::reaper::ManagerWindowSlotService slots(state, now_seconds);

  current_time = 10.0;
  auto first = slots.claim();
  check(static_cast<bool>(first) && first.slot == 1,
        "first native Manager window should claim slot 1");
  check(state.get("ui.manager_slot.1.active") == "1",
        "claimed Manager slot should be marked active");
  check(!state.get("ui.manager_slot.1.launching").empty(),
        "claimed Manager slot should carry a launch timestamp until heartbeat");

  check(slots.set_launch_tab(1, "overlay"),
        "claimed Manager slot should accept a launch-tab handoff");
  check(slots.consume_launch_tab(1) == "overlay",
        "Manager launch tab should be consumed once");
  check(slots.consume_launch_tab(1).empty(),
        "Manager launch tab should be cleared after consumption");

  current_time = 10.5;
  check(slots.heartbeat(1), "active Manager should refresh its heartbeat");
  check(state.get("ui.manager_slot.1.launching").empty(),
        "first heartbeat should clear the launching marker");

  auto second = slots.claim();
  check(static_cast<bool>(second) && second.slot == 2,
        "second native Manager window should claim slot 2 while slot 1 is live");
  auto third = slots.claim();
  check(static_cast<bool>(third) && third.slot == 3,
        "third native Manager window should claim slot 3 while earlier slots are live");
  const auto full = slots.claim();
  check(!full && full.error.find("Three ReaADR manager windows") != std::string::npos,
        "fourth Manager launch should preserve Lua's three-window limit");

  current_time = 13.0;
  auto reclaimed = slots.claim();
  check(static_cast<bool>(reclaimed) && reclaimed.slot == 1,
        "stale heartbeat should make slot 1 reclaimable after two seconds");

  check(slots.release(1), "releasing Manager slot should succeed");
  check(state.get("ui.manager_slot.1.active").empty() &&
          state.get("ui.manager_slot.1.heartbeat").empty() &&
          state.get("ui.manager_slot.1.launching").empty() &&
          state.get("ui.manager_slot.1.launch_tab").empty(),
        "releasing Manager slot should clear all Lua-compatible slot state");
}
} // namespace

int main()
{
  const auto& actions = reaadr::core::manager_actions();

  check(actions.size() == 18,
        "Manager action catalog must retain its established 18-action presentation contract");
  check(!actions.empty() && actions.front().key == "import_cue_sheet",
        "Manager action catalog must retain Import Cue Sheet as its first action");
  check(actions.size() > 4 && actions[4].key == "validate_session",
        "Manager action ordering must not shift when native-only routes are added");
  check(!actions.empty() && actions.back().key == "help_quick_actions",
        "Manager action catalog must retain Quick Actions Help as its final action");

  bool cue_info_in_catalog = false;
  for (const auto& action : actions) {
    if (action.key == "cue_info") {
      cue_info_in_catalog = true;
      break;
    }
  }
  check(!cue_info_in_catalog,
        "Cue Info must not enter the Manager presentation catalog before the UI contract is deliberately updated");
  check(reaadr::core::manager_action_is_native("cue_info"),
        "Cue Info must remain classified as a native controller route");
  check(reaadr::core::manager_action_is_native("record_cue") &&
          reaadr::core::manager_action_is_native("generate_cues") &&
          reaadr::core::manager_action_is_native("detect_dialogue"),
        "migrated workflow routes must remain classified as native");
  check(!reaadr::core::manager_action_is_native("import_cue_sheet"),
        "Import Cue Sheet must retain its current compatibility classification until its routing contract changes");

  test_manager_window_slots();

  if (failures != 0) {
    std::cerr << failures << " manager navigation test(s) failed.\n";
    return 1;
  }

  std::cout << "Manager navigation tests passed.\n";
  return 0;
}
