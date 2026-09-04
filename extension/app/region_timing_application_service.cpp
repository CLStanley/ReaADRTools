#include "region_timing_application_service.hpp"

namespace reaadr::reaper {

RegionTimingApplicationResult RegionTimingApplicationService::update()
{
  RegionTimingApplicationResult result;
  const RegionTimingRenderResult rendered = renderer_.sync_region_timings_and_render(options_);
  result.timing = rendered.timing;
  result.synchronization = rendered.render;
  result.error = rendered.error;
  return result;
}

} // namespace reaadr::reaper
