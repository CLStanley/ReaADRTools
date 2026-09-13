#include <reaper_plugin.h>

// The broad native test binary links reaper_reaadr.cpp, whose
// REAPERAPI_IMPLEMENT block now owns the requested REAPER API globals. Keep
// this translation unit as a compatibility placeholder for the test build;
// add explicit stubs here only for APIs that are not owned by the host TU.
