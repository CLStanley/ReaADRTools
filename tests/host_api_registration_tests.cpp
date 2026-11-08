#include "reaadr_reaper/host_api_registration.hpp"

#include <array>
#include <cassert>
#include <string>
#include <utility>
#include <vector>

using reaadr::reaper::HostApiExport;
using reaadr::reaper::register_host_api_exports;
using reaadr::reaper::unregister_host_api_exports;

int main()
{
  int a = 1;
  int b = 2;
  const std::array<HostApiExport, 2> exports = {{
    {"API_ReaADR_First", &a},
    {"APIdef_ReaADR_First", &b},
  }};

  std::vector<std::pair<std::string, void*>> calls;
  const auto succeed = [&](const char* key, void* value) {
    calls.emplace_back(key, value);
    return true;
  };

  assert(register_host_api_exports(exports, succeed));
  assert(calls.size() == 2);
  assert(calls[0].first == "API_ReaADR_First" && calls[0].second == &a);
  assert(calls[1].first == "APIdef_ReaADR_First" && calls[1].second == &b);

  unregister_host_api_exports(exports, succeed);
  assert(calls.size() == 4);
  assert(calls[2].first == "-APIdef_ReaADR_First" && calls[2].second == &b);
  assert(calls[3].first == "-API_ReaADR_First" && calls[3].second == &a);

  calls.clear();
  const auto fail_second = [&](const char* key, void* value) {
    calls.emplace_back(key, value);
    return calls.size() != 2;
  };
  assert(!register_host_api_exports(exports, fail_second));
  assert(calls.size() == 3);
  assert(calls[2].first == "-API_ReaADR_First" && calls[2].second == &a);

  calls.clear();
  const std::array<HostApiExport, 1> invalid = {{{nullptr, &a}}};
  assert(!register_host_api_exports(invalid, succeed));
  assert(calls.empty());
  return 0;
}
