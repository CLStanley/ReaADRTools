#pragma once

#include "session_model.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace reaadr::core {

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

struct ProjectRenderState {
  std::vector<ExistingTrack> tracks;
  std::vector<ExistingRegion> regions;
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

struct RenderPlan {
  std::vector<TrackMutation> track_mutations;
  std::vector<RegionMutation> region_mutations;

  bool empty() const { return track_mutations.empty() && region_mutations.empty(); }
};

struct RenderPlanOptions {
  double preroll_seconds = 3.0;
  double timing_epsilon = 0.0005;
  bool create_cue_tracks = true;
  bool create_dialogue_tracks = true;
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
