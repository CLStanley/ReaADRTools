#pragma once

namespace reaadr::ui {

class CueManagerController;

// Opens the native grouped character/lane filter UI. Returns true when a
// platform-native presentation was available and shown.
bool show_character_filter_window(CueManagerController& controller);

} // namespace reaadr::ui
