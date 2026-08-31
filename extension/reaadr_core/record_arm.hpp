#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace reaadr::core {

struct RecordArmSnapshotEntry {
  std::size_t snapshot_index = 0;
  double armed = 0.0;
};

struct RecordArmMutation {
  std::size_t snapshot_index = 0;
  double armed = 0.0;
};

struct RecordArmPlanResult {
  std::vector<RecordArmMutation> mutations;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Produces the complete isolation intent on every call. Reapplying every
// snapshot entry is required when loop recording changes target lanes while
// preserving the one original arm-state snapshot for final restoration.
RecordArmPlanResult build_record_arm_isolation_plan(
  const std::vector<RecordArmSnapshotEntry>& snapshot,
  std::size_t target_snapshot_index);

} // namespace reaadr::core
