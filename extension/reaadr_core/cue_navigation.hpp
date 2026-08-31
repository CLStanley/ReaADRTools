#pragma once

#include "model_repository.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace reaadr::core {

struct CueNavigationEntry {
  Fields cue;
  std::size_t model_index = 0;
  std::string cue_key;
  std::string cue_id;
  double start_time = 0.0;
  double end_time = 0.0;
};

struct CueNavigationCatalogResult {
  std::vector<CueNavigationEntry> cues;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Builds the stable timeline order shared by next/previous navigation and the
// future native cue manager. Canonical timing must be valid; project regions
// are rendered output and are deliberately not a fallback cue store.
CueNavigationCatalogResult build_cue_navigation_catalog(const SessionModel& model);

const CueNavigationEntry* find_next_cue(const std::vector<CueNavigationEntry>& cues,
                                        double position,
                                        double epsilon = 0.0001);
const CueNavigationEntry* find_previous_cue(const std::vector<CueNavigationEntry>& cues,
                                            double position,
                                            double epsilon = 0.0001);
const CueNavigationEntry* find_cue_by_id(const std::vector<CueNavigationEntry>& cues,
                                         const std::string& cue_id);
const CueNavigationEntry* find_cue_at_position(const std::vector<CueNavigationEntry>& cues,
                                               double position);

struct CueSelectionState {
  std::string manager_selected_cue_key;
  std::string active_overlay_cue_key;
};

struct CueSelectionLoadResult {
  CueSelectionState state;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

struct CueSelectionSaveResult {
  CueSelectionState state;
  std::string error;
  bool changed = false;
  bool rolled_back = false;

  explicit operator bool() const { return error.empty(); }
};

// These keys are project-local UI state, not a second cue model. Saving keeps
// the manager and overlay selection paired, restoring the prior manager value
// if the second extstate write fails.
class CueSelectionRepository {
public:
  explicit CueSelectionRepository(ProjectStateStore& store) : store_(store) {}

  CueSelectionLoadResult load() const;
  CueSelectionSaveResult save_selected_cue(const std::string& cue_key);

  static constexpr const char* kNamespace = SessionModelRepository::kNamespace;
  static constexpr const char* kManagerSelectionKey = "manager_selected_cue_key";
  static constexpr const char* kActiveOverlayKey = "active_overlay_cue_key";

private:
  ProjectStateStore& store_;
};

} // namespace reaadr::core
