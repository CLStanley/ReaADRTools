#include <reaper_plugin.h>

// The production extension receives these REAPER API function-pointer globals
// from the REAPERAPI_IMPLEMENT translation unit. The broad native test binary
// links the same source graph outside of REAPER, so provide null definitions for
// APIs that are only touched through injected host adapters during tests.
void (*GetSet_LoopTimeRange2)(ReaProject*, bool, bool, double*, double*, bool) = nullptr;
int (*CountTakes)(MediaItem*) = nullptr;
