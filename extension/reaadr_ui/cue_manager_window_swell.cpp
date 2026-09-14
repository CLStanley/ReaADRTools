#ifndef _WIN32

// Keep the mature SWELL Cue Manager implementation as the non-Windows
// presentation while the persistent native Manager session is being wired into
// the extension host. The dialog remains modal here so its stack-owned
// controller/service graph stays valid for the entire window lifetime.
//
// Once CueManagerSessionHost owns that graph independently of the action stack,
// this shell can switch to a host-loop modeless CreateDialogParam lifecycle
// without inventing a private SWELL message pump.
#define show_cue_manager show_cue_manager_swell_legacy
#include "cue_manager_window.cpp"
#undef show_cue_manager

namespace reaadr::ui {

bool show_cue_manager(CueManagerController& controller, double frame_rate)
{
  return show_cue_manager_swell_legacy(controller, frame_rate);
}

} // namespace reaadr::ui

#endif
