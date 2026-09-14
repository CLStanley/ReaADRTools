#pragma once

#include <string>
#include <vector>

#ifdef REAADR_LEGACY_MANAGER_TEST_COMPAT
#include "reaadr_core/model_repository.hpp"

#include <algorithm>
#include <cstdlib>
#endif

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

#ifdef REAADR_LEGACY_MANAGER_TEST_COMPAT
// Temporary adapter for assertions that still live in the historical
// session_model_tests.cpp aggregate. Production builds do not expose this API;
// focused Manager tests exercise WindowLayoutRepository instead.
struct ManagerWindowLayout {
  int width = 1040;
  int height = 880;
  int min_width = 1040;
  int min_height = 880;
  int dock = 0;
  int x = 0;
  int y = 0;
  bool has_position = false;
};

inline ManagerWindowLayout default_manager_window_layout()
{
  return {};
}

struct ManagerWindowLayoutLoadResult {
  ManagerWindowLayout layout;
  std::string error;
  explicit operator bool() const { return error.empty(); }
};

class ManagerWindowLayoutRepository {
public:
  explicit ManagerWindowLayoutRepository(ProjectStateStore& state) : state_(state) {}

  ManagerWindowLayoutLoadResult load(bool remember) const
  {
    ManagerWindowLayoutLoadResult result;
    if (!remember) return result;

    const auto read_int = [this](const char* key, int& output) {
      const auto value = state_.read(SessionModelRepository::kNamespace, key);
      if (!value || value.value.empty()) return false;
      char* end = nullptr;
      const long parsed = std::strtol(value.value.c_str(), &end, 10);
      if (!end || *end != '\0') return false;
      output = static_cast<int>(parsed);
      return true;
    };

    int value = 0;
    if (read_int("ui.window.manager.width", value))
      result.layout.width = (std::max)(result.layout.min_width, value);
    if (read_int("ui.window.manager.height", value))
      result.layout.height = (std::max)(result.layout.min_height, value);
    read_int("ui.window.manager.dock", result.layout.dock);
    const bool has_x = read_int("ui.window.manager.x", result.layout.x);
    const bool has_y = read_int("ui.window.manager.y", result.layout.y);
    result.layout.has_position = has_x && has_y;
    return result;
  }

  bool save(const ManagerWindowLayout& layout)
  {
    return state_.write(SessionModelRepository::kNamespace, "ui.window.manager.width",
                        std::to_string((std::max)(layout.min_width, layout.width))) &&
      state_.write(SessionModelRepository::kNamespace, "ui.window.manager.height",
                   std::to_string((std::max)(layout.min_height, layout.height))) &&
      state_.write(SessionModelRepository::kNamespace, "ui.window.manager.dock",
                   std::to_string(layout.dock)) &&
      state_.write(SessionModelRepository::kNamespace, "ui.window.manager.x",
                   std::to_string(layout.x)) &&
      state_.write(SessionModelRepository::kNamespace, "ui.window.manager.y",
                   std::to_string(layout.y));
  }

private:
  ProjectStateStore& state_;
};
#endif

} // namespace reaadr::core
