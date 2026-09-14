#pragma once

#include "../app/character_filter_application_service.hpp"
#include "../reaadr_core/character_filter.hpp"

#include <string>
#include <vector>

namespace reaadr::reaper {

// Loads the canonical session plus persisted filter state and returns the
// character/lane catalog consumed by native Manager UI.
core::CharacterFilterCatalogResult load_native_character_filter_catalog();

// Applies an exact set of character or character.laneN filter tokens through
// the transactional native character-filter service.
CharacterFilterApplicationResult apply_native_character_filter_tokens(
  const std::vector<std::string>& tokens,
  bool hide_inactive_regions);

} // namespace reaadr::reaper
