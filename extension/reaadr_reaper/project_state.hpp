#pragma once

#include "reaadr_core/model_repository.hpp"

#include <cstddef>

struct ReaProject;

namespace reaadr::reaper {

// Function pointers are supplied after REAPERAPI_LoadAPI succeeds. This makes
// buffer growth and error handling testable without linking the REAPER host.
struct ProjectStateApi {
  int (*get)(ReaProject*, const char*, const char*, char*, int) = nullptr;
  int (*set)(ReaProject*, const char*, const char*, const char*) = nullptr;
};

class ProjectStateStore final : public core::ProjectStateStore {
public:
  ProjectStateStore(ReaProject* project, ProjectStateApi api) : project_(project), api_(api) {}

  core::StateReadResult read(const char* name_space, const char* key) const override;
  bool write(const char* name_space, const char* key, const std::string& value) override;

private:
  // Start small for ordinary settings, but allow large cue sessions. Reads
  // grow only when the previous buffer appears completely filled.
  static constexpr std::size_t kInitialCapacity = 64U * 1024U;
  static constexpr std::size_t kMaximumCapacity = 64U * 1024U * 1024U;

  ReaProject* project_ = nullptr;
  ProjectStateApi api_;
};

} // namespace reaadr::reaper
