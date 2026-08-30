#pragma once

#include "session_model.hpp"

#include <cstdint>
#include <string>

namespace reaadr::core {

enum class StateReadError {
  none,
  not_found,
  unavailable,
  value_too_large,
};

struct StateReadResult {
  std::string value;
  StateReadError error = StateReadError::none;

  explicit operator bool() const { return error == StateReadError::none; }
};

// Port implemented by the REAPER layer and faked by native tests. Keeping the
// domain repository dependent on this narrow interface prevents REAPER SDK
// types and global function pointers from spreading into model code.
class ProjectStateStore {
public:
  virtual ~ProjectStateStore() = default;
  virtual StateReadResult read(const char* name_space, const char* key) const = 0;
  virtual bool write(const char* name_space, const char* key, const std::string& value) = 0;
};

enum class SessionLoadError {
  none,
  missing,
  storage_unavailable,
  value_too_large,
  invalid_model,
};

struct SessionLoadResult {
  SessionModel model;
  SessionLoadError error = SessionLoadError::none;
  ParseError parse_error = ParseError::none;

  explicit operator bool() const { return error == SessionLoadError::none; }
};

struct RevisionResult {
  std::uint64_t revision = 0;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

struct SessionSnapshot {
  std::string label;
  std::string timestamp;
  std::string model_blob;
  std::string revision;
  // Captured in memory so rollback also restores the compatibility index.
  // The model blob remains the canonical source of session identity.
  std::string session_id;
};

struct SnapshotResult {
  SessionSnapshot snapshot;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Owns the canonical keys used to load and save the ADR Session Model. Features
// should use this class instead of reading project extstate directly.
class SessionModelRepository {
public:
  explicit SessionModelRepository(ProjectStateStore& store) : store_(store) {}

  SessionLoadResult load() const;
  bool save(const SessionModel& model);
  RevisionResult revision() const;
  RevisionResult bump_revision();

  // The timestamp is injected by the application layer to keep tests and the
  // domain repository independent from wall-clock and timezone facilities.
  SnapshotResult create_snapshot(const std::string& label, const std::string& utc_timestamp);
  RevisionResult restore_snapshot(const SessionSnapshot& snapshot);

  static constexpr const char* kNamespace = "ReaADRTools";
  static constexpr const char* kModelKey = "adr_session_model_v1";
  static constexpr const char* kSessionIdKey = "adr_session_id";
  static constexpr const char* kRevisionKey = "session_revision";
  static constexpr const char* kSnapshotLabelKey = "session_snapshot_last_label";
  static constexpr const char* kSnapshotTimestampKey = "session_snapshot_last_timestamp";
  static constexpr const char* kSnapshotModelKey = "session_snapshot_last_model_v1";
  static constexpr const char* kSnapshotRevisionKey = "session_snapshot_last_revision";

private:
  ProjectStateStore& store_;
};

const char* session_load_error_message(const SessionLoadResult& result);

} // namespace reaadr::core
