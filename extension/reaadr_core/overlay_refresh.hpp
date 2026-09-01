#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace reaadr::core {

inline constexpr const char* kOverlayFxName = "ReaADR Video Overlay";
inline constexpr const char* kOverlayCodeMarker =
  "// ReaADR generated source-track video overlay";

struct ExistingOverlayFx {
  int fx_index = -1;
  std::string renamed_name;
  std::string video_code;
  bool enabled = false;
};

struct ExistingOverlayTrack {
  std::size_t project_index = 0;
  std::string role;
  std::string key;
  std::vector<ExistingOverlayFx> effects;
};

enum class OverlayMutationKind {
  none,
  create,
  update,
  remove,
};

struct OverlayRefreshOptions {
  bool enabled = true;
  std::string video_code;
};

struct OverlayRefreshPlan {
  std::size_t track_project_index = 0;
  std::string expected_track_role = "source_video";
  std::string expected_track_key = "source_video";
  OverlayMutationKind mutation = OverlayMutationKind::none;
  ExistingOverlayFx existing;
  std::string video_code;
};

struct OverlayRefreshPlanResult {
  OverlayRefreshPlan plan;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

bool is_owned_overlay_fx(const ExistingOverlayFx& effect);

// Selects only the exact generated source-video track and refuses ambiguous FX
// ownership. Enabled plans accept code carrying the native/Lua ownership marker.
OverlayRefreshPlanResult build_overlay_refresh_plan(
  const std::vector<ExistingOverlayTrack>& tracks,
  const OverlayRefreshOptions& options);

} // namespace reaadr::core
