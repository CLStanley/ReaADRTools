#include "cue_status.hpp"

#include "domain_utils.hpp"
#include "render_plan.hpp"

namespace reaadr::core {
namespace {

std::string field(const Fields& fields, const char* key)
{
  const auto found = fields.find(key);
  return found == fields.end() ? std::string() : found->second;
}

void restore_after_failure(SessionModelRepository& repository,
                           const SessionSnapshot& snapshot,
                           CueStatusCommitResult& result)
{
  const RevisionResult restored = repository.restore_snapshot(snapshot);
  result.rolled_back = static_cast<bool>(restored);
  if (!restored) result.error += " Snapshot rollback also failed: " + restored.error;
}

} // namespace

CueStatusUpdateResult update_cue_status(
  const SessionModel& model,
  const CueStatusUpdateOptions& options)
{
  CueStatusUpdateResult result;
  if (model.session_id().empty()) {
    result.error = "A canonical session ID is required before updating cue status.";
    return result;
  }
  if (options.cue_key.empty()) {
    result.error = "A cue key is required before updating cue status.";
    return result;
  }

  std::size_t selected = model.cues.size();
  for (std::size_t index = 0; index < model.cues.size(); ++index) {
    if (render_cue_key(model.cues[index]) != options.cue_key) continue;
    if (selected != model.cues.size()) {
      result.error = "Multiple cues match the status key: " + options.cue_key;
      return result;
    }
    selected = index;
  }
  if (selected == model.cues.size()) {
    result.error = "The cue status target is not present in the canonical session.";
    return result;
  }

  result.model = model;
  result.cue_model_index = selected;
  result.normalized_status = normalize_status(options.status);
  result.cue = result.model.cues[selected];
  if (field(result.cue, "status") == result.normalized_status) return result;

  result.cue["status"] = result.normalized_status;
  result.model.cues[selected] = result.cue;
  result.model.state["last_operation"] = options.last_operation;
  result.model.dirty_flags["cues_modified"] = "true";
  result.changed = true;
  return result;
}

CueStatusCommitResult commit_cue_status(
  SessionModelRepository& repository,
  const CueStatusCommitOptions& options)
{
  CueStatusCommitResult result;
  const SessionLoadResult loaded = repository.load();
  if (!loaded) {
    result.error = session_load_error_message(loaded);
    return result;
  }

  result.update = update_cue_status(loaded.model, options.update);
  if (!result.update) {
    result.error = result.update.error;
    return result;
  }
  if (!result.update.changed) {
    const RevisionResult current = repository.revision();
    if (!current) result.error = current.error;
    else result.revision = current.revision;
    return result;
  }

  const SnapshotResult snapshot =
    repository.create_snapshot(options.snapshot_label, options.utc_timestamp);
  if (!snapshot) {
    result.error = snapshot.error;
    return result;
  }
  result.snapshot = snapshot.snapshot;

  if (!repository.save(result.update.model)) {
    result.error = "Could not persist the cue status in the ADR Session Model.";
    restore_after_failure(repository, result.snapshot, result);
    return result;
  }
  const RevisionResult revision = options.bump_revision
    ? repository.bump_revision()
    : repository.revision();
  if (!revision) {
    result.error = revision.error;
    restore_after_failure(repository, result.snapshot, result);
    return result;
  }
  result.revision = revision.revision;
  return result;
}

} // namespace reaadr::core
