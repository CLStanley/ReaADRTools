#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace reaadr::reaper {

// REAPER-independent plan for populating an existing customizable menu.
// The host adapter performs platform-specific insertion and label updates.
struct NativeMenuEntry {
  int command_id = 0;
  std::string label;
};

enum class NativeMenuOperation { update, append };

struct NativeMenuStep {
  NativeMenuOperation operation;
  int position;
  NativeMenuEntry entry;
};

inline std::vector<NativeMenuStep> plan_native_menu(
  const std::vector<NativeMenuEntry>& entries, int existing_count,
  bool supports_update)
{
  std::vector<NativeMenuStep> steps;
  if (existing_count < 0) return steps;
  int position = 0;
  for (const auto& entry : entries) {
    if (entry.command_id == 0) continue;
    if (position < existing_count) {
      if (supports_update)
        steps.push_back({NativeMenuOperation::update, position, entry});
    } else {
      steps.push_back({NativeMenuOperation::append, position, entry});
    }
    ++position;
  }
  return steps;
}

} // namespace reaadr::reaper
