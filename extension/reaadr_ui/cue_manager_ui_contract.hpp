#pragma once

#include "reaadr_core/character_filter.hpp"

#include <string>
#include <vector>

namespace reaadr::core {

struct CueManagerColumn {
  std::string key;
  std::string label;
  int width = 0;
  bool editable = false;
};

struct CueManagerAction {
  std::string key;
  std::string label;
  std::string hint;
};

struct CueManagerCharacterFilterItem {
  enum class Kind { show_all, character, lane };

  Kind kind = Kind::show_all;
  std::string label;
  std::string character;
  std::string target_key;
  bool checked = false;
  bool partial = false;
};

const std::vector<CueManagerColumn>& cue_manager_columns();
// Lua column controls step by 24 pixels and clamp widths to 44..900.
int adjust_cue_manager_column_width(int current, bool increase);
const std::vector<CueManagerAction>& cue_manager_actions();
const std::vector<std::string>& cue_manager_status_choices();
const std::vector<std::string>& cue_manager_type_choices();
std::vector<CueManagerCharacterFilterItem> cue_manager_character_filter_items(
  const CharacterFilterCatalogResult& catalog);
bool is_cue_manager_sort_key(const std::string& key);

} // namespace reaadr::core
