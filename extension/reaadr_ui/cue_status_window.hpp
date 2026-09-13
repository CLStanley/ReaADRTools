#pragma once

#include <string>
#include <vector>

namespace reaadr::ui {

// Presents the standalone Set Cue Status choices without mutating project
// state. Returns false when the user cancels or the native presentation is not
// available on the current platform.
bool choose_cue_status(const std::vector<std::string>& statuses,
                       std::string& selected_status);

} // namespace reaadr::ui
