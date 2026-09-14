#pragma once

#include "model_repository.hpp"

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

  WindowLayoutLoadResult load() const;
  bool save(const WindowLayout& layout);

  static constexpr const char* kRememberLayoutKey = "ui.remember_window_layout";

private:
  bool remember_layout(std::string* error = nullptr) const;
  std::string key(const char* suffix) const;

  ProjectStateStore& store_;
  std::string window_key_;
  int default_width_ = 0;
  int default_height_ = 0;
};

} // namespace reaadr::core
