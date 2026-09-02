#include "overlay_settings.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <utility>
#include <vector>

namespace reaadr::core {
namespace {

struct BooleanMember {
  const char* key;
  bool OverlaySettings::*member;
};

constexpr std::array<BooleanMember, 23> kBooleanMembers = {{
  {"enabled", &OverlaySettings::enabled},
  {"show_cue_id", &OverlaySettings::show_cue_id},
  {"show_character", &OverlaySettings::show_character},
  {"show_dialogue", &OverlaySettings::show_dialogue},
  {"show_cue_timecode", &OverlaySettings::show_cue_timecode},
  {"show_project_timer", &OverlaySettings::show_project_timer},
  {"show_visual_cue", &OverlaySettings::show_visual_cue},
  {"show_direction", &OverlaySettings::show_direction},
  {"show_cue_type", &OverlaySettings::show_cue_type},
  {"show_streamer", &OverlaySettings::show_streamer},
  {"show_flash", &OverlaySettings::show_flash},
  {"show_status", &OverlaySettings::show_status},
  {"show_metadata", &OverlaySettings::show_metadata},
  {"bg_cue_id", &OverlaySettings::bg_cue_id},
  {"bg_character", &OverlaySettings::bg_character},
  {"bg_cue_timecode", &OverlaySettings::bg_cue_timecode},
  {"bg_project_timer", &OverlaySettings::bg_project_timer},
  {"bg_dialogue", &OverlaySettings::bg_dialogue},
  {"bg_direction", &OverlaySettings::bg_direction},
  {"bg_cue_type", &OverlaySettings::bg_cue_type},
  {"bg_status", &OverlaySettings::bg_status},
  {"bg_metadata", &OverlaySettings::bg_metadata},
  {"include_preroll_each_loop", &OverlaySettings::include_preroll_each_loop},
}};

std::string setting_key(const char* key)
{
  return std::string(OverlaySettingsRepository::kPrefix) + key;
}

bool lua_compatible_boolean(const std::string& value)
{
  return value == "1" || value == "true" || value == "yes";
}

bool parse_number(const std::string& value, double& output)
{
  char* end = nullptr;
  output = std::strtod(value.c_str(), &end);
  if (!end || end == value.c_str()) return false;
  while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n' ||
         *end == '\f' || *end == '\v') ++end;
  return *end == '\0' && std::isfinite(output);
}

std::string number_string(double value)
{
  std::ostringstream output;
  output << std::setprecision(15) << value;
  return output.str();
}

std::string storage_error(StateReadError error)
{
  return error == StateReadError::value_too_large
    ? "An overlay preference is too large to load safely."
    : "REAPER project extstate is unavailable while loading overlay preferences.";
}

struct PreviousValue {
  std::string key;
  bool existed = false;
  std::string value;
};

} // namespace

bool operator==(const OverlaySettings& left, const OverlaySettings& right)
{
  for (const BooleanMember& entry : kBooleanMembers) {
    if (left.*(entry.member) != right.*(entry.member)) return false;
  }
  return left.text_color == right.text_color &&
    left.metadata_fields == right.metadata_fields &&
    left.preroll_seconds == right.preroll_seconds;
}

bool apply_overlay_profile(OverlaySettings& settings, const std::string& profile)
{
  if (profile != "actor" && profile != "engineer" && profile != "studio" && profile != "minimal") return false;
  settings.enabled = true;
  settings.show_cue_id = true; settings.show_character = profile != "minimal";
  settings.show_dialogue = true; settings.show_cue_timecode = profile != "minimal";
  settings.show_project_timer = true; settings.show_visual_cue = true;
  settings.show_direction = profile == "actor" || profile == "engineer";
  settings.show_cue_type = profile == "engineer" || profile == "studio";
  settings.show_streamer = true; settings.show_flash = profile != "minimal";
  settings.show_status = profile != "minimal"; settings.show_metadata = profile == "engineer" || profile == "studio";
  settings.bg_project_timer = profile == "engineer" || profile == "studio";
  settings.bg_cue_id = false; settings.bg_character = false;
  settings.bg_cue_timecode = profile == "engineer" || profile == "studio";
  settings.bg_dialogue = true; settings.bg_direction = false; settings.bg_cue_type = false;
  settings.bg_status = false; settings.bg_metadata = false;
  return true;
}

std::string normalize_overlay_metadata_fields(const std::string& value)
{
  static const std::array<const char*, 6> defaults = {
    "PGID", "MID", "Media Time", "Watermark Timestamp", "Asset Date Code", "Project Name"};
  std::vector<std::string> fields;
  std::stringstream input(value);
  std::string field;
  while (std::getline(input, field, ',')) {
    const auto first = field.find_first_not_of(" \t\r\n");
    const auto last = field.find_last_not_of(" \t\r\n");
    if (first != std::string::npos) fields.push_back(field.substr(first, last - first + 1));
  }
  for (std::size_t i = fields.size(); i < defaults.size(); ++i) fields.emplace_back(defaults[i]);
  std::ostringstream output;
  for (std::size_t i = 0; i < fields.size(); ++i) { if (i) output << ','; output << fields[i]; }
  return output.str();
}

std::string normalize_overlay_text_color(const std::string& value)
{
  std::string normalized = value;
  for (char& byte : normalized) if (byte >= 'A' && byte <= 'Z') byte = static_cast<char>(byte - 'A' + 'a');
  return normalized == "yellow" ? "yellow" : "white";
}

OverlaySettingsLoadResult OverlaySettingsRepository::load() const
{
  OverlaySettingsLoadResult result;
  const auto read_value = [this, &result](const char* key, std::string& value) {
    const StateReadResult stored = store_.read(kNamespace, setting_key(key).c_str());
    if (stored) {
      value = stored.value;
      return true;
    }
    if (stored.error != StateReadError::not_found) result.error = storage_error(stored.error);
    return false;
  };

  for (const BooleanMember& entry : kBooleanMembers) {
    std::string value;
    if (read_value(entry.key, value) && !value.empty()) {
      result.settings.*(entry.member) = lua_compatible_boolean(value);
    }
    if (!result) return result;
  }

  std::string value;
  if (read_value("text_color", value) && !value.empty()) result.settings.text_color = value;
  if (!result) return result;
  value.clear();
  if (read_value("metadata_fields", value) && !value.empty()) result.settings.metadata_fields = value;
  if (!result) return result;
  value.clear();
  double preroll = 0.0;
  if (read_value("preroll_seconds", value) && !value.empty() && parse_number(value, preroll)) {
    result.settings.preroll_seconds = preroll;
  }
  return result;
}

OverlaySettingsSaveResult OverlaySettingsRepository::save(const OverlaySettings& settings)
{
  OverlaySettingsSaveResult result;
  if (!std::isfinite(settings.preroll_seconds)) {
    result.error = "Overlay preroll must be a finite number.";
    return result;
  }
  const OverlaySettingsLoadResult loaded = load();
  if (!loaded) {
    result.error = loaded.error;
    return result;
  }
  if (loaded.settings == settings) return result;

  std::vector<std::pair<std::string, std::string>> desired;
  desired.reserve(kBooleanMembers.size() + 3);
  for (const BooleanMember& entry : kBooleanMembers) {
    desired.emplace_back(setting_key(entry.key), settings.*(entry.member) ? "1" : "0");
  }
  desired.emplace_back(setting_key("text_color"), settings.text_color);
  desired.emplace_back(setting_key("metadata_fields"), settings.metadata_fields);
  desired.emplace_back(setting_key("preroll_seconds"), number_string(settings.preroll_seconds));

  std::vector<PreviousValue> written;
  for (const auto& [key, value] : desired) {
    const StateReadResult previous = store_.read(kNamespace, key.c_str());
    if (!previous && previous.error != StateReadError::not_found) {
      result.error = storage_error(previous.error);
      break;
    }
    if (previous && previous.value == value) continue;
    if (!store_.write(kNamespace, key.c_str(), value)) {
      result.error = "Could not persist all overlay preferences.";
      break;
    }
    written.push_back({key, static_cast<bool>(previous), previous.value});
  }

  if (!result.error.empty()) {
    bool rollback_succeeded = true;
    for (auto it = written.rbegin(); it != written.rend(); ++it) {
      rollback_succeeded = store_.write(
        kNamespace, it->key.c_str(), it->existed ? it->value : std::string()) && rollback_succeeded;
    }
    if (!rollback_succeeded) result.error += " Some prior preference values could not be restored.";
    return result;
  }
  result.changed = !written.empty();
  return result;
}

} // namespace reaadr::core
