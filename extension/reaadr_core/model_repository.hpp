#pragma once

#include "session_model.hpp"

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

// Owns the canonical keys used to load and save the ADR Session Model. Features
// should use this class instead of reading project extstate directly.
class SessionModelRepository {
public:
  explicit SessionModelRepository(ProjectStateStore& store) : store_(store) {}

  SessionLoadResult load() const;
  bool save(const SessionModel& model);

  static constexpr const char* kNamespace = "ReaADRTools";
  static constexpr const char* kModelKey = "adr_session_model_v1";
  static constexpr const char* kSessionIdKey = "adr_session_id";

private:
  ProjectStateStore& store_;
};

const char* session_load_error_message(const SessionLoadResult& result);

} // namespace reaadr::core
