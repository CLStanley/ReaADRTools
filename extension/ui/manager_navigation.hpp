#pragma once

#include "reaadr_core/model_repository.hpp"

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
struct ManagerWindowLayout {
  int width = 1040;
  int height = 880;
  int min_width = 1040;
  int min_height = 880;
  int dock = 0;
  int x = 0;
  int y = 0;
};

struct ManagerWindowLayoutResult {
  ManagerWindowLayout layout;
  std::string error;
  explicit operator bool() const { return error.empty(); }
};

// Persists the Manager's project-scoped geometry using the same keys as the
// Lua window helper. Coordinates are optional; width/height are clamped to
// the native minimum when restored.
class ManagerWindowLayoutRepository {
public:
  explicit ManagerWindowLayoutRepository(ProjectStateStore& store) : store_(store) {}
  ManagerWindowLayoutResult load(bool remember_layout) const;
  bool save(const ManagerWindowLayout& layout);

private:
  ProjectStateStore& store_;
};

const std::vector<ManagerModule>& manager_modules();
bool is_manager_tab(const std::string& key);
std::string normalize_manager_tab(const std::string& requested);
const std::vector<ManagerAction>& manager_actions();
bool manager_action_is_native(const std::string& key);
ManagerWindowLayout default_manager_window_layout();

} // namespace reaadr::core
