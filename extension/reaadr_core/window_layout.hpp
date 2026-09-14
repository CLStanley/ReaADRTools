#pragma once

#include "model_repository.hpp"

#include <cstdlib>
#include <string>
#include <utility>

namespace reaadr::core {

struct WindowLayout {
  int width = 0;
  int height = 0;
  int dock = -1;
  int x = 0;
  int y = 0;
  bool has_position = false;
};

struct WindowLayoutLoadResult {
  WindowLayout layout;
  bool remembered = false;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

class WindowLayoutRepository {
public:
  WindowLayoutRepository(ProjectStateStore& store, std::string window_key,
                         int default_width, int default_height)
    : store_(store), window_key_(std::move(window_key)),
      default_width_(default_width), default_height_(default_height) {}

  WindowLayoutLoadResult load() const
  {
    WindowLayoutLoadResult result;
    result.layout.width = default_width_;
    result.layout.height = default_height_;

    std::string remember_error;
    result.remembered = remember_layout(&remember_error);
    if (!remember_error.empty()) {
      result.error = remember_error;
      return result;
    }
    if (!result.remembered) return result;

    int value = 0;
    if (parse_int(store_.read(SessionModelRepository::kNamespace, key("width").c_str()), value) && value > 0)
      result.layout.width = value;
    if (parse_int(store_.read(SessionModelRepository::kNamespace, key("height").c_str()), value) && value > 0)
      result.layout.height = value;
    if (parse_int(store_.read(SessionModelRepository::kNamespace, key("dock").c_str()), value))
      result.layout.dock = value;

    const bool has_x = parse_int(
      store_.read(SessionModelRepository::kNamespace, key("x").c_str()), result.layout.x);
    const bool has_y = parse_int(
      store_.read(SessionModelRepository::kNamespace, key("y").c_str()), result.layout.y);
    result.layout.has_position = has_x && has_y;
    return result;
  }

  bool save(const WindowLayout& layout)
  {
    std::string remember_error;
    if (!remember_layout(&remember_error)) return remember_error.empty();

    return store_.write(SessionModelRepository::kNamespace, key("dock").c_str(),
                        std::to_string(layout.dock)) &&
      store_.write(SessionModelRepository::kNamespace, key("x").c_str(),
                   std::to_string(layout.x)) &&
      store_.write(SessionModelRepository::kNamespace, key("y").c_str(),
                   std::to_string(layout.y)) &&
      store_.write(SessionModelRepository::kNamespace, key("width").c_str(),
                   std::to_string(layout.width)) &&
      store_.write(SessionModelRepository::kNamespace, key("height").c_str(),
                   std::to_string(layout.height));
  }

  static constexpr const char* kRememberLayoutKey = "ui.remember_window_layout";

private:
  static bool truthy(const std::string& value)
  {
    return value == "1" || value == "true" || value == "yes";
  }

  static bool parse_int(const StateReadResult& value, int& output)
  {
    if (!value || value.value.empty()) return false;
    char* end = nullptr;
    const long parsed = std::strtol(value.value.c_str(), &end, 10);
    if (!end || *end != '\0') return false;
    output = static_cast<int>(parsed);
    return true;
  }

  bool remember_layout(std::string* error = nullptr) const
  {
    const auto value = store_.read(SessionModelRepository::kNamespace, kRememberLayoutKey);
    if (value.error == StateReadError::not_found) return false;
    if (!value) {
      if (error) *error = "REAPER project extstate is unavailable while loading window layout preferences.";
      return false;
    }
    return truthy(value.value);
  }

  std::string key(const char* suffix) const
  {
    return "ui.window." + window_key_ + "." + suffix;
  }

  ProjectStateStore& store_;
  std::string window_key_;
  int default_width_ = 0;
  int default_height_ = 0;
};

} // namespace reaadr::core
