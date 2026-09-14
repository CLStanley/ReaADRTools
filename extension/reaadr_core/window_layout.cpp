#include "window_layout.hpp"

#include <cstdlib>

namespace reaadr::core {
namespace {

bool truthy(const std::string& value)
{
  return value == "1" || value == "true" || value == "yes";
}

bool parse_int(const StateReadResult& value, int& output)
{
  if (!value || value.value.empty()) return false;
  char* end = nullptr;
  const long parsed = std::strtol(value.value.c_str(), &end, 10);
  if (!end || *end != '\0') return false;
  output = static_cast<int>(parsed);
  return true;
}

} // namespace

std::string WindowLayoutRepository::key(const char* suffix) const
{
  return "ui.window." + window_key_ + "." + suffix;
}

bool WindowLayoutRepository::remember_layout(std::string* error) const
{
  const auto value = store_.read(SessionModelRepository::kNamespace, kRememberLayoutKey);
  if (value.error == StateReadError::not_found) return false;
  if (!value) {
    if (error) *error = "REAPER project extstate is unavailable while loading window layout preferences.";
    return false;
  }
  return truthy(value.value);
}

WindowLayoutLoadResult WindowLayoutRepository::load() const
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

bool WindowLayoutRepository::save(const WindowLayout& layout)
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

} // namespace reaadr::core
