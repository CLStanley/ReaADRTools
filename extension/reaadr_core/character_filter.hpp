#pragma once

#include "model_repository.hpp"

#include <cstddef>
#include <set>
#include <string>
#include <vector>

namespace reaadr::core {

struct CharacterFilterState {
  std::string encoded_selection;
  std::set<std::string> active_tokens;
  bool hide_inactive_regions = false;

  bool enabled() const { return !encoded_selection.empty(); }
};

struct CharacterFilterLoadResult {
  CharacterFilterState state;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

std::string character_filter_key(const std::string& character);
std::string character_filter_target_key(const std::string& character, int lane);
std::string encode_character_filter_tokens(std::vector<std::string> tokens);
CharacterFilterState parse_character_filter_state(const std::string& encoded_selection,
                                                  bool hide_inactive_regions);
bool character_lane_is_active(const CharacterFilterState& state,
                              const std::string& character,
                              int lane);

// Project-local filter settings are UI state, not an alternative cue model.
// Saving them is intentionally separate from revision/event publication so an
// application service can coordinate those writes with visible REAPER changes.
class CharacterFilterRepository {
public:
  explicit CharacterFilterRepository(ProjectStateStore& store) : store_(store) {}

  CharacterFilterLoadResult load() const;
  bool save(const CharacterFilterState& state);

  static constexpr const char* kNamespace = SessionModelRepository::kNamespace;
  static constexpr const char* kSelectionKey = "active_character_filter";
  static constexpr const char* kHideRegionsKey = "character_filter_hide_regions";

private:
  ProjectStateStore& store_;
};

struct ExistingFilterTrack {
  std::size_t project_index = 0;
  std::string role;
  std::string key;
  bool muted = false;
};

struct ExistingFilterRegion {
  int id = -1;
  std::string name;
  bool hidden = false;
};

struct CharacterFilterProjectState {
  std::vector<ExistingFilterTrack> tracks;
  std::vector<ExistingFilterRegion> regions;
};

struct TrackMuteMutation {
  std::size_t project_index = 0;
  std::string expected_role;
  std::string expected_key;
  bool muted = false;
};

struct RegionVisibilityMutation {
  int existing_id = -1;
  std::string expected_name;
  bool hidden = false;
};

struct CharacterFilterPlan {
  std::vector<TrackMuteMutation> track_mutations;
  std::vector<RegionVisibilityMutation> region_mutations;

  bool empty() const { return track_mutations.empty() && region_mutations.empty(); }
};

struct CharacterFilterPlanResult {
  CharacterFilterPlan plan;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Builds an idempotent filter plan from the canonical cues. Only tracks with
// explicit ADR roles and exact model-derived region names enter the plan.
CharacterFilterPlanResult build_character_filter_plan(
  const SessionModel& model,
  const CharacterFilterState& filter,
  const CharacterFilterProjectState& existing,
  double preroll_seconds = 3.0);

} // namespace reaadr::core
