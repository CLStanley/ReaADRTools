#include "session_commit.hpp"

#include <utility>

namespace reaadr::core {
namespace {

void restore_after_failure(SessionModelRepository& repository,
                           const SessionSnapshot& snapshot,
                           SessionCommitResult& result)
{
  const RevisionResult restored = repository.restore_snapshot(snapshot);
  result.rolled_back = static_cast<bool>(restored);
  if (!restored) result.error += " Snapshot rollback also failed: " + restored.error;
}

} // namespace

SessionCommitResult commit_session_cues(SessionModelRepository& repository,
                                         const std::vector<Fields>& cues,
                                         const SessionCommitOptions& options)
{
  SessionCommitResult result;
  SessionLoadResult loaded = repository.load();
  const SessionModel* existing = nullptr;
  if (loaded) {
    existing = &loaded.model;
  } else if (loaded.error != SessionLoadError::missing) {
    result.error = session_load_error_message(loaded);
    return result;
  }

  const SnapshotResult snapshot = repository.create_snapshot(options.snapshot_label, options.utc_timestamp);
  if (!snapshot) {
    result.error = snapshot.error;
    return result;
  }
  result.snapshot = snapshot.snapshot;

  SessionBuildResult replacement = replace_session_cues(existing, cues, options.replacement);
  if (!replacement) {
    result.error = replacement.error;
    return result;
  }

  if (!repository.save(replacement.model)) {
    result.error = "Could not persist the replacement ADR Session Model.";
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

  result.model = std::move(replacement.model);
  result.revision = revision.revision;
  return result;
}

} // namespace reaadr::core
