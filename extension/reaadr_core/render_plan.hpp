#pragma once

#include "session_model.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace reaadr::core {

// Shared render identities are public because cleanup, filtering, and future
// overlay adapters must resolve exactly the same generated objects as the main
// renderer. Keeping one implementation protects the ownership boundary.
std::string render_character_name(const Fields& cue);
std::string render_cue_key(const Fields& cue);
std::string render_region_name(const Fields& cue);

// Colors are kept as RGB in the domain layer because REAPER's packed native
// color representation differs by operating system. The adapter performs the
// platform-specific conversion immediately before reading or writing REAPER.
struct RgbColor {
  int red = 0;
  int green = 0;
  int blue = 0;
  bool custom = false;
};

bool operator==(const RgbColor& left, const RgbColor& right);
bool operator!=(const RgbColor& left, const RgbColor& right);

struct ExistingTrack {
  std::size_t project_index = 0;
  std::string role;
  std::string key;
  std::string name;
  RgbColor color;
};

struct ExistingRegion {
  int id = -1;
  std::string name;
  double start_time = 0.0;
  double end_time = 0.0;
  RgbColor color;
};

struct ExistingRulerLane {
  int index = 0;
  std::string name;
  RgbColor color;
  bool hidden = false;
};

struct ExistingRegionLane {
  std::string region_name;
  int lane_index = 0;
};

struct ExistingCueAudioItem {
  std::size_t track_index = 0;
  std::size_t item_index = 0;
  std::string track_role;
  std::string track_key;
  std::string role;
  std::string cue_key;
  std::string source_path;
  std::string take_name;
  double position = 0.0;
  double length = 0.0;
  bool loops_source = false;
  bool has_take = false;
};

struct ProjectRenderState {
  std::vector<ExistingTrack> tracks;
  std::vector<ExistingRegion> regions;
  std::vector<ExistingRulerLane> ruler_lanes;
  std::vector<ExistingRegionLane> region_lanes;
  std::vector<ExistingCueAudioItem> cue_audio_items;
};

struct DesiredTrack {
  std::string role;
  std::string key;
  std::string name;
  RgbColor color;
};

struct DesiredRegion {
  std::string model_region_id;
  std::string name;
  double start_time = 0.0;
  double end_time = 0.0;
  RgbColor color;
};

struct DesiredRulerLane {
  int index = 0;
  std::string name;
  RgbColor color;
  bool hidden = false;
};

struct RegionLaneMutation {
  std::string region_name;
  int lane_index = 0;
};

struct DesiredCueAudioItem {
  std::string cue_key;
  std::string target_track_key;
  std::string source_path;
  std::string take_name;
  double position = 0.0;
  double length = 0.0;
  bool replace_source = false;
};

enum class RenderMutationKind {
  create,
  update,
  remove,
};

struct TrackMutation {
  RenderMutationKind kind = RenderMutationKind::create;
  std::size_t project_index = 0;
  DesiredTrack desired;
};

struct RegionMutation {
  RenderMutationKind kind = RenderMutationKind::create;
  int existing_id = -1;
  DesiredRegion desired;
};

struct CueAudioMutation {
  RenderMutationKind kind = RenderMutationKind::create;
  std::size_t existing_track_index = 0;
  std::size_t existing_item_index = 0;
  DesiredCueAudioItem desired;
};

struct RenderPlan {
  std::vector<TrackMutation> track_mutations;
  std::vector<RegionMutation> region_mutations;
  int minimum_ruler_lane_count = 0;
  std::vector<DesiredRulerLane> ruler_lane_mutations;
  std::vector<RegionLaneMutation> region_lane_mutations;
  std::vector<CueAudioMutation> cue_audio_mutations;

  bool empty() const {
    return track_mutations.empty() && region_mutations.empty() && minimum_ruler_lane_count == 0 &&
      ruler_lane_mutations.empty() && region_lane_mutations.empty() && cue_audio_mutations.empty();
  }
};

struct RenderPlanOptions {
  double preroll_seconds = 3.0;
  double timing_epsilon = 0.0005;
  bool create_cue_tracks = true;
  bool create_dialogue_tracks = true;
  bool configure_ruler_lanes = true;
  // Cue audio is optional because the generated WAV belongs to the application
  // layer. A positive measured duration and a path enable item planning.
  std::string cue_audio_path;
  double cue_audio_duration = 0.0;
};

struct RenderPlanResult {
  RenderPlan plan;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Builds a deterministic mutation plan from the canonical model and a snapshot
// of the visible project. previous_model is optional, but stale regions are
// removable only when their exact generated names can be derived from it. This
// proof-of-ownership boundary prevents a refresh from deleting user regions.
RenderPlanResult build_render_plan(const SessionModel& model,
                                   const SessionModel* previous_model,
                                   const ProjectRenderState& existing,
                                   const RenderPlanOptions& options = {});

} // namespace reaadr::core
