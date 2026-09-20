#pragma once

#include "model_repository.hpp"

#include <string>

namespace reaadr::core {

struct RecordingPreferenceLoadResult {
  bool include_preroll_each_loop = true;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

struct RecordingPreferenceSaveResult {
  bool include_preroll_each_loop = true;
  bool changed = false;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Reads and writes the canonical per-project overlay preference used by the
// native recording and overlay workflows. The storage key is intentionally
// stable so existing ReaADR projects retain their saved behavior.
class RecordingPreferenceRepository {
public:
  explicit RecordingPreferenceRepository(ProjectStateStore& store) : store_(store) {}

  RecordingPreferenceLoadResult load() const;
  RecordingPreferenceSaveResult save_include_preroll_each_loop(bool enabled);

  static constexpr const char* kNamespace = SessionModelRepository::kNamespace;
  static constexpr const char* kIncludePrerollKey = "overlay.include_preroll_each_loop";

private:
  ProjectStateStore& store_;
};

} // namespace reaadr::core
