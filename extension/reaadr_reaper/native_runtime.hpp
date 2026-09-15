#pragma once

#include "cue_manager_session.hpp"

#include <string>

struct reaper_plugin_info_t;

namespace reaadr::reaper {

// Small extension-host boundary for native workflow lifetime. Keeping action
// registration and persistent Manager ownership behind this API makes the final
// reaper_reaadr.cpp cut-over a pair of explicit load/unload calls rather than
// spreading native lifetime rules through the legacy host monolith.
bool initialize_native_runtime(reaper_plugin_info_t* plugin, std::string* error = nullptr);
bool shutdown_native_runtime(reaper_plugin_info_t* plugin, std::string* error = nullptr);

bool open_native_cue_manager(CueManagerSessionConfig config, std::string& error);

} // namespace reaadr::reaper
