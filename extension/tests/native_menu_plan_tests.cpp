#include "../reaadr_reaper/native_menu_plan.hpp"

#include <cassert>
#include <vector>

int main()
{
  using namespace reaadr::reaper;
  const std::vector<NativeMenuEntry> entries = {
    {10, "Manager"}, {0, "Unavailable"}, {20, "Quick Action"},
    {30, "Validate"}, {40, "Refresh"}
  };
  const auto fresh = plan_native_menu(entries, 0, true);
  assert(fresh.size() == 4);
  for (int i = 0; i < 4; ++i) {
    assert(fresh[static_cast<std::size_t>(i)].position == i);
    assert(fresh[static_cast<std::size_t>(i)].operation == NativeMenuOperation::append);
  }
  const auto partial = plan_native_menu(entries, 2, true);
  assert(partial.size() == 4);
  assert(partial[0].operation == NativeMenuOperation::update);
  assert(partial[1].operation == NativeMenuOperation::update);
  assert(partial[2].operation == NativeMenuOperation::append);
  assert(partial[2].position == 2);
  const auto no_update = plan_native_menu(entries, 2, false);
  assert(no_update.size() == 2);
  assert(no_update[0].position == 2);
  assert(no_update[1].position == 3);
  assert(plan_native_menu(entries, -1, true).empty());
}
