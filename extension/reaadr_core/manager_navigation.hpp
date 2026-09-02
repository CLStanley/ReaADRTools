#pragma once

#include <string>
#include <vector>

namespace reaadr::core {

struct ManagerModule {
  std::string key;
  std::string title;
};

const std::vector<ManagerModule>& manager_modules();
bool is_manager_tab(const std::string& key);
std::string normalize_manager_tab(const std::string& requested);

} // namespace reaadr::core
