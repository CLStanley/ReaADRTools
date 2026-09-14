#ifndef _WIN32

// Keep the mature SWELL Cue Manager implementation intact while moving its
// exported window lifecycle behind a thin platform shell. This gives the
// migration a small seam for modeless/single-instance lifecycle work without
// duplicating or rewriting the large UI body.
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
