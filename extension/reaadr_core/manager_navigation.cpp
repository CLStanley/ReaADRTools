#include "manager_navigation.hpp"

namespace reaadr::core {

const std::vector<ManagerModule>& manager_modules()
{
  static const std::vector<ManagerModule> modules = {
    {"import", "Import"}, {"cues", "Cue Management"},
    {"session", "Session Tools"}, {"reports", "Reports"},
    {"overlay", "Video Overlays"}, {"preferences", "Preferences"},
    {"help", "Help"},
  };
  return modules;
}

bool is_manager_tab(const std::string& key)
{
  for (const auto& module : manager_modules()) if (module.key == key) return true;
  return false;
}

std::string normalize_manager_tab(const std::string& requested)
{
  return is_manager_tab(requested) ? requested : manager_modules().front().key;
}

} // namespace reaadr::core
