#pragma once

#include <string>
#include <vector>

namespace reaadr::core {

struct ManagerModule {
  std::string key;
  std::string title;
};
struct ManagerAction {
  std::string module;
  std::string key;
  std::string label;
  std::string hint;
};

const std::vector<ManagerModule>& manager_modules();
bool is_manager_tab(const std::string& key);
std::string normalize_manager_tab(const std::string& requested);
const std::vector<ManagerAction>& manager_actions();
bool manager_action_is_native(const std::string& key);

} // namespace reaadr::core
