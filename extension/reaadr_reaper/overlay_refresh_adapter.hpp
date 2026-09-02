#pragma once

#include "reaadr_core/overlay_refresh.hpp"
#include "project_transaction.hpp"

#include <string>

struct MediaTrack;
struct ReaProject;

namespace reaadr::reaper {

struct OverlayRefreshApi {
  int (*count_tracks)(ReaProject*) = nullptr;
  MediaTrack* (*get_track)(ReaProject*, int) = nullptr;
  bool (*validate_track)(ReaProject*, MediaTrack*) = nullptr;
  bool (*get_set_track_string)(MediaTrack*, const char*, char*, bool) = nullptr;
  int (*track_fx_get_count)(MediaTrack*) = nullptr;
  bool (*track_fx_get_named_config)(MediaTrack*, int, const char*, char*, int) = nullptr;
  bool (*track_fx_get_enabled)(MediaTrack*, int) = nullptr;
  int (*track_fx_add_by_name)(MediaTrack*, const char*, bool, int) = nullptr;
  bool (*track_fx_delete)(MediaTrack*, int) = nullptr;
  bool (*track_fx_set_named_config)(MediaTrack*, int, const char*, const char*) = nullptr;
  void (*track_fx_set_enabled)(MediaTrack*, int, bool) = nullptr;
  void (*adjust_track_windows)(bool) = nullptr;
  void (*update_arrange)() = nullptr;
};

struct OverlayRefreshInspectionResult {
  std::vector<core::ExistingOverlayTrack> tracks;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

struct OverlayRefreshApplyResult {
  int effects_created = 0;
  int effects_updated = 0;
  int effects_removed = 0;
  bool restored_after_failure = false;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

OverlayRefreshInspectionResult inspect_overlay_project(
  ReaProject* project,
  OverlayRefreshApi api);

OverlayRefreshApplyResult apply_overlay_refresh_plan_transactionally(
  ReaProject* project,
  OverlayRefreshApi api,
  TransactionApi transaction_api,
  const core::OverlayRefreshPlan& plan,
  const std::string& description);

// Convenience boundary used by recording/status coordinators once native EEL
// code generation supplies the desired overlay code.
OverlayRefreshApplyResult refresh_generated_overlay_transactionally(
  ReaProject* project,
  OverlayRefreshApi api,
  TransactionApi transaction_api,
  const core::OverlayRefreshOptions& options,
  const std::string& description);

} // namespace reaadr::reaper
