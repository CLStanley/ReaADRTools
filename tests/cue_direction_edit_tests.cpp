#include "reaadr_core/cue_manager_model.hpp"
#include "reaadr_ui/cue_manager_ui_contract.hpp"

#include <cstdlib>
#include <iostream>

namespace {

void check(bool condition, const char* message)
{
  if (condition) return;
  std::cerr << "FAILED: " << message << '\n';
  std::exit(1);
}

reaadr::core::SessionModel model()
{
  reaadr::core::SessionModel value;
  value.session["session_id"] = "test-session";
  value.timecode["frame_rate"] = "24";
  value.cues = {{{"id", "1"}, {"character", "Actor"}, {"start_time", "1"},
                 {"end_time", "2"}, {"line", "Hello"}, {"direction", "Original"},
                 {"status", "Not Recorded"}, {"cue_type", "Dialogue"}}};
  return value;
}

void test_direction_update()
{
  auto options = reaadr::core::CueManagerEditOptions{};
  options.cue_key = "1";
  options.direction = "Whisper";
  options.direction_set = true;
  const auto result = reaadr::core::edit_cue_manager_row(model(), options);
  check(result && result.changed, "direction edit should change the cue");
  check(result.model.cues[0].at("direction") == "Whisper", "direction should persist");
}

void test_direction_clear()
{
  auto options = reaadr::core::CueManagerEditOptions{};
  options.cue_key = "1";
  options.direction.clear();
  options.direction_set = true;
  const auto result = reaadr::core::edit_cue_manager_row(model(), options);
  check(result && result.changed, "explicit empty direction should change the cue");
  check(result.model.cues[0].at("direction").empty(), "direction should be clearable");
}

void test_omitted_direction_is_preserved()
{
  auto options = reaadr::core::CueManagerEditOptions{};
  options.cue_key = "1";
  options.status = "Recorded";
  const auto result = reaadr::core::edit_cue_manager_row(model(), options);
  check(result && result.changed, "unrelated edit should succeed");
  check(result.model.cues[0].at("direction") == "Original", "omitted direction must be preserved");
}

void test_character_filter_items()
{
  reaadr::core::CharacterFilterCatalogResult catalog;
  catalog.show_all = false;

  reaadr::core::CharacterFilterGroup alice;
  alice.character = "Alice";
  alice.all_active = false;
  alice.partially_active = true;
  alice.targets = {
    {"alice.lane1", "Alice", 1, true},
    {"alice.lane2", "Alice", 2, false},
  };

  reaadr::core::CharacterFilterGroup bob;
  bob.character = "Bob";
  bob.all_active = true;
  bob.targets = {{"bob.lane1", "Bob", 1, true}};
  catalog.groups = {alice, bob};

  const auto items = reaadr::core::cue_manager_character_filter_items(catalog);
  check(items.size() == 5, "filter presentation should include Show All, groups, and Alice lanes");
  check(items[0].kind == reaadr::core::CueManagerCharacterFilterItem::Kind::show_all && !items[0].checked,
        "Show All presentation should reflect the persisted filter state");
  check(items[1].kind == reaadr::core::CueManagerCharacterFilterItem::Kind::character &&
        items[1].label == "Alice" && !items[1].checked && items[1].partial,
        "partial character selection should be represented explicitly");
  check(items[2].kind == reaadr::core::CueManagerCharacterFilterItem::Kind::lane &&
        items[2].label == "Lane 1" && items[2].checked && items[2].target_key == "alice.lane1",
        "active lane should retain its target token for the Manager UI");
  check(items[3].kind == reaadr::core::CueManagerCharacterFilterItem::Kind::lane && !items[3].checked,
        "inactive lane should render unchecked");
  check(items[4].kind == reaadr::core::CueManagerCharacterFilterItem::Kind::character &&
        items[4].label == "Bob" && items[4].checked,
        "single-lane characters should stay compact instead of adding a redundant lane row");
}

} // namespace

int main()
{
  test_direction_update();
  test_direction_clear();
  test_omitted_direction_is_preserved();
  test_character_filter_items();
  std::cout << "cue_direction_edit_tests passed\n";
  return 0;
}
