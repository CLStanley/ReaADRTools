#include "window_docking.hpp"

// The REAPER docking adapter is implemented in native_host_services.cpp.
// Keep this translation unit intentionally empty while the build manifests
// still list window_docking.cpp; the public boundary remains window_docking.hpp
// and there must be exactly one implementation of each host docking operation.
