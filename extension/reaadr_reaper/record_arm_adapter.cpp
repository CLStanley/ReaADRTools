#include "record_arm_adapter.hpp"

#include <cmath>
#include <utility>

namespace reaadr::reaper {
namespace {

constexpr const char* kRecordArmParameter = "I_RECARM";

bool api_complete(const RecordArmApi& api)
{
  return api.count_tracks && api.get_track && api.validate_track &&
    api.get_track_value && api.set_track_value;
}

bool arm_matches(double actual, double expected)
{
  return std::isfinite(actual) && ((actual != 0.0) == (expected != 0.0));
}

} // namespace

RecordArmApplyResult RecordArmManager::capture_and_isolate(MediaTrack* target_track)
{
  RecordArmApplyResult result;
  if (!target_track) {
    result.error = "No ADR recording track is available.";
    return result;
  }
  if (!api_complete(api_)) {
    result.error = "The REAPER record-arm API is incomplete.";
    return result;
  }

  const bool captured_this_call = snapshot_.empty();
  if (captured_this_call) {
    const int track_count = api_.count_tracks(project_);
    if (track_count < 0) {
      result.error = "REAPER returned an invalid track count while capturing record-arm state.";
      return result;
    }
    snapshot_.reserve(static_cast<std::size_t>(track_count));
    for (int index = 0; index < track_count; ++index) {
      MediaTrack* track = api_.get_track(project_, index);
      if (!track || !api_.validate_track(project_, track)) {
        result.error = "REAPER could not resolve a track while capturing record-arm state.";
        snapshot_.clear();
        return result;
      }
      const double armed = api_.get_track_value(track, kRecordArmParameter);
      if (!std::isfinite(armed)) {
        result.error = "REAPER returned an invalid record-arm value.";
        snapshot_.clear();
        return result;
      }
      snapshot_.push_back({track, {static_cast<std::size_t>(index), armed}});
    }
  }

  std::size_t target_index = snapshot_.size();
  for (std::size_t index = 0; index < snapshot_.size(); ++index) {
    if (snapshot_[index].track == target_track) {
      target_index = index;
      break;
    }
  }
  if (target_index == snapshot_.size() || !api_.validate_track(project_, target_track)) {
    result.error = "The target recording track is not part of the captured project state.";
    if (captured_this_call) snapshot_.clear();
    return result;
  }

  std::vector<core::RecordArmSnapshotEntry> domain_snapshot;
  domain_snapshot.reserve(snapshot_.size());
  for (const CapturedTrack& captured : snapshot_) domain_snapshot.push_back(captured.state);
  const core::RecordArmPlanResult plan =
    core::build_record_arm_isolation_plan(
      domain_snapshot, snapshot_[target_index].state.snapshot_index);
  if (!plan) {
    result.error = plan.error;
    if (captured_this_call) snapshot_.clear();
    return result;
  }

  struct BeforeMutation {
    MediaTrack* track = nullptr;
    double armed = 0.0;
  };
  const auto captured_for = [&](std::size_t snapshot_index) -> const CapturedTrack* {
    for (const CapturedTrack& captured : snapshot_) {
      if (captured.state.snapshot_index == snapshot_index) return &captured;
    }
    return nullptr;
  };
  std::vector<BeforeMutation> before;
  before.reserve(plan.mutations.size());
  for (const core::RecordArmMutation& mutation : plan.mutations) {
    const CapturedTrack* captured = captured_for(mutation.snapshot_index);
    if (!captured) {
      result.error = "The record-arm isolation plan does not match its captured snapshot.";
      if (captured_this_call) snapshot_.clear();
      return result;
    }
    if (!api_.validate_track(project_, captured->track)) {
      if (captured->track == target_track) {
        result.error = "The target recording track became unavailable before isolation.";
        if (captured_this_call) snapshot_.clear();
        return result;
      }
      before.push_back({nullptr, 0.0});
      continue;
    }
    const double armed = api_.get_track_value(captured->track, kRecordArmParameter);
    if (!std::isfinite(armed)) {
      result.error = "REAPER returned an invalid record-arm value before isolation.";
      if (captured_this_call) snapshot_.clear();
      return result;
    }
    before.push_back({captured->track, armed});
  }

  ProjectTransaction transaction(
    project_, transaction_api_, "ReaADR: isolate recording track", -1, false);
  UiRefreshScope refresh(transaction_api_.prevent_ui_refresh);
  for (std::size_t index = 0; index < plan.mutations.size(); ++index) {
    const core::RecordArmMutation& mutation = plan.mutations[index];
    MediaTrack* track = before[index].track;
    if (!track) {
      ++result.tracks_skipped;
      continue;
    }
    if (arm_matches(before[index].armed, mutation.armed)) continue;
    if (!api_.set_track_value(track, kRecordArmParameter, mutation.armed) ||
        !arm_matches(api_.get_track_value(track, kRecordArmParameter), mutation.armed)) {
      result.error = "REAPER could not isolate the ADR recording track.";
      break;
    }
    ++result.tracks_updated;
  }

  if (!result.error.empty()) {
    bool restored = true;
    for (std::size_t index = 0; index < before.size(); ++index) {
      MediaTrack* track = before[index].track;
      if (!track) continue;
      if (!api_.validate_track(project_, track)) {
        restored = false;
        continue;
      }
      if (arm_matches(api_.get_track_value(track, kRecordArmParameter), before[index].armed)) {
        continue;
      }
      if (!api_.set_track_value(track, kRecordArmParameter, before[index].armed) ||
          !arm_matches(api_.get_track_value(track, kRecordArmParameter), before[index].armed)) {
        restored = false;
      }
    }
    result.restored_after_failure = restored;
    transaction.mark_failed();
    if (captured_this_call && restored) snapshot_.clear();
    if (!restored) result.error += " Prior record-arm state could not be fully restored.";
  }
  return result;
}

RecordArmApplyResult RecordArmManager::restore()
{
  RecordArmApplyResult result;
  if (snapshot_.empty()) return result;
  if (!api_complete(api_)) {
    result.error = "The REAPER record-arm API is incomplete.";
    return result;
  }

  ProjectTransaction transaction(
    project_, transaction_api_, "ReaADR: restore record-arm state", -1, false);
  UiRefreshScope refresh(transaction_api_.prevent_ui_refresh);
  std::vector<CapturedTrack> retry;
  for (const CapturedTrack& captured : snapshot_) {
    if (!api_.validate_track(project_, captured.track)) {
      ++result.tracks_skipped;
      continue;
    }
    const double current = api_.get_track_value(captured.track, kRecordArmParameter);
    if (arm_matches(current, captured.state.armed)) continue;
    if (!api_.set_track_value(captured.track, kRecordArmParameter, captured.state.armed) ||
        !arm_matches(api_.get_track_value(captured.track, kRecordArmParameter), captured.state.armed)) {
      retry.push_back(captured);
      continue;
    }
    ++result.tracks_updated;
  }
  snapshot_ = std::move(retry);
  if (!snapshot_.empty()) {
    result.error = "REAPER could not fully restore the prior record-arm state.";
    transaction.mark_failed();
  }
  return result;
}

} // namespace reaadr::reaper
