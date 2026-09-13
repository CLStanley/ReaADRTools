#include "reaadr_ui/manager_navigation.hpp"

#include <iostream>
#include <string>

namespace {
int failures = 0;

void check(bool condition, const std::string& message)
{
  if (condition) return;
  ++failures;
  std::cerr << "FAIL: " << message << '\n';
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

  if (failures != 0) {
    std::cerr << failures << " manager navigation test(s) failed.\n";
    return 1;
  }

  std::cout << "Manager navigation tests passed.\n";
  return 0;
}
