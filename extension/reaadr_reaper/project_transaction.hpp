#pragma once

#include <string>

struct ReaProject;

namespace reaadr::reaper {

struct TransactionApi {
  void (*begin)(ReaProject*) = nullptr;
  void (*end)(ReaProject*, const char*, int) = nullptr;
  const char* (*can_undo)(ReaProject*) = nullptr;
  int (*undo)(ReaProject*) = nullptr;
  void (*prevent_ui_refresh)(int) = nullptr;
};

// Nested transaction scopes join the outer undo block. If any nested scope is
// marked failed—or an exception crosses a scope—the outer owner closes a
// clearly labelled failed block and rolls it back only when that exact block
// is REAPER's current undo point.
class ProjectTransaction {
public:
  ProjectTransaction(ReaProject* project,
                     TransactionApi api,
                     std::string description,
                     int undo_flags = -1,
                     bool rollback_on_failure = true);
  ~ProjectTransaction();

  ProjectTransaction(const ProjectTransaction&) = delete;
  ProjectTransaction& operator=(const ProjectTransaction&) = delete;

  void mark_failed();
  bool owns_undo_block() const { return owner_; }

private:
  ReaProject* project_ = nullptr;
  TransactionApi api_;
  std::string description_;
  int undo_flags_ = -1;
  int exceptions_on_entry_ = 0;
  bool rollback_on_failure_ = true;
  bool owner_ = false;
};

// REAPER's refresh API is counter-based. RAII ensures every +1 is paired with
// -1 on normal returns and exception paths.
class UiRefreshScope {
public:
  explicit UiRefreshScope(void (*prevent_ui_refresh)(int));
  ~UiRefreshScope();

  UiRefreshScope(const UiRefreshScope&) = delete;
  UiRefreshScope& operator=(const UiRefreshScope&) = delete;

private:
  void (*prevent_ui_refresh_)(int) = nullptr;
};

} // namespace reaadr::reaper
