#include "project_transaction.hpp"

#include <exception>
#include <utility>

namespace reaadr::reaper {
namespace {

struct TransactionState {
  int depth = 0;
  bool failed = false;
};

// REAPER invokes extension actions on its UI thread. thread_local still keeps
// tests and any future worker-thread use from sharing nesting state by accident.
thread_local TransactionState g_transaction_state;

} // namespace

ProjectTransaction::ProjectTransaction(ReaProject* project,
                                       TransactionApi api,
                                       std::string description,
                                       int undo_flags,
                                       bool rollback_on_failure)
  : project_(project),
    api_(api),
    description_(std::move(description)),
    undo_flags_(undo_flags),
    exceptions_on_entry_(std::uncaught_exceptions()),
    rollback_on_failure_(rollback_on_failure),
    owner_(g_transaction_state.depth == 0 && api_.begin && api_.end)
{
  if (owner_) {
    g_transaction_state.failed = false;
    api_.begin(project_);
  }
  ++g_transaction_state.depth;
}

ProjectTransaction::~ProjectTransaction()
{
  if (std::uncaught_exceptions() > exceptions_on_entry_) g_transaction_state.failed = true;
  if (g_transaction_state.depth > 0) --g_transaction_state.depth;
  if (!owner_) return;

  const std::string final_description =
    g_transaction_state.failed ? description_ + " (failed)" : description_;
  api_.end(project_, final_description.c_str(), undo_flags_);

  // Matching the description prevents a failed no-op from undoing the user's
  // previous, unrelated action when REAPER did not create a new undo point.
  if (g_transaction_state.failed && rollback_on_failure_ && api_.can_undo && api_.undo) {
    const char* available = api_.can_undo(project_);
    if (available && final_description == available) api_.undo(project_);
  }
  g_transaction_state.failed = false;
}

void ProjectTransaction::mark_failed()
{
  g_transaction_state.failed = true;
}

UiRefreshScope::UiRefreshScope(void (*prevent_ui_refresh)(int))
  : prevent_ui_refresh_(prevent_ui_refresh)
{
  if (prevent_ui_refresh_) prevent_ui_refresh_(1);
}

UiRefreshScope::~UiRefreshScope()
{
  if (prevent_ui_refresh_) prevent_ui_refresh_(-1);
}

} // namespace reaadr::reaper
