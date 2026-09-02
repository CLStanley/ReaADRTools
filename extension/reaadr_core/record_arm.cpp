#include "record_arm.hpp"

#include <cmath>
#include <set>

namespace reaadr::core {

RecordArmPlanResult build_record_arm_isolation_plan(
  const std::vector<RecordArmSnapshotEntry>& snapshot,
  std::size_t target_snapshot_index)
{
  RecordArmPlanResult result;
  if (snapshot.empty()) {
    result.error = "A record-arm snapshot is required before isolating a track.";
    return result;
  }

  std::set<std::size_t> indexes;
  bool target_found = false;
  result.mutations.reserve(snapshot.size());
  for (const RecordArmSnapshotEntry& entry : snapshot) {
    if (!std::isfinite(entry.armed)) {
      result.error = "The record-arm snapshot contains an invalid value.";
      return result;
    }
    if (!indexes.insert(entry.snapshot_index).second) {
      result.error = "The record-arm snapshot contains duplicate track entries.";
      return result;
    }
    const bool target = entry.snapshot_index == target_snapshot_index;
    target_found = target_found || target;
    result.mutations.push_back({entry.snapshot_index, target ? 1.0 : 0.0});
  }
  if (!target_found) {
    result.mutations.clear();
    result.error = "The target recording track is not part of the captured snapshot.";
  }
  return result;
}

} // namespace reaadr::core
