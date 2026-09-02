#pragma once

#include "model_repository.hpp"

#include <string>

namespace reaadr::core {

// Project-local overlay preferences mirror DEFAULT_OVERLAY_SETTINGS in the
// transitional Lua core. Keeping one typed representation allows native UI and
// EEL generation to share the exact persisted compatibility keys.
struct OverlaySettings {
  bool enabled = true;
  bool show_cue_id = true;
  bool show_character = true;
  bool show_dialogue = true;
  bool show_cue_timecode = true;
  bool show_project_timer = true;
  bool show_visual_cue = true;
  bool show_direction = true;
  bool show_cue_type = true;
  bool show_streamer = true;
  bool show_flash = true;
  bool show_status = true;
  bool show_metadata = false;
  bool bg_cue_id = false;
  bool bg_character = false;
  bool bg_cue_timecode = false;
  bool bg_project_timer = false;
  bool bg_dialogue = true;
  bool bg_direction = false;
  bool bg_cue_type = false;
  bool bg_status = false;
  bool bg_metadata = false;
  std::string text_color = "white";
  std::string metadata_fields =
    "PGID,MID,Media Time,Watermark Timestamp,Asset Date Code,Project Name";
  double preroll_seconds = 3.0;
  bool include_preroll_each_loop = true;
};

bool operator==(const OverlaySettings& left, const OverlaySettings& right);
inline bool operator!=(const OverlaySettings& left, const OverlaySettings& right)
{
  return !(left == right);
}

// Applies the exact Manager profile presets to a settings value.
bool apply_overlay_profile(OverlaySettings& settings, const std::string& profile);

struct OverlaySettingsLoadResult {
  OverlaySettings settings;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

struct OverlaySettingsSaveResult {
  bool changed = false;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

class OverlaySettingsRepository {
public:
  explicit OverlaySettingsRepository(ProjectStateStore& store) : store_(store) {}

  OverlaySettingsLoadResult load() const;
  OverlaySettingsSaveResult save(const OverlaySettings& settings);

  static constexpr const char* kNamespace = SessionModelRepository::kNamespace;
  static constexpr const char* kPrefix = "overlay.";

private:
  ProjectStateStore& store_;
};

} // namespace reaadr::core
