return function(ReaADR, deps)
  local first_nonempty = deps.first_nonempty
  local project = deps.project
  local sanitize_token = deps.sanitize_token

  local function add_unique(list, seen, value)
    value = tostring(value or "")
    if value ~= "" and not seen[value] then
      seen[value] = true
      list[#list + 1] = value
    end
  end

  local function character_lane_key(character, lane)
    local resolver = deps.get_character_lane_key()
    return resolver(first_nonempty(character, "Unassigned"), tonumber(lane) or 1)
  end

  local function assign_filter_lanes(cues)
    local setup_preroll_seconds = deps.get_setup_preroll_seconds()
    local assign_character_lanes = deps.get_assign_character_lanes()
    local preroll_seconds = setup_preroll_seconds({ overlay_settings = ReaADR.load_overlay_settings() })
    assign_character_lanes(cues or {}, preroll_seconds)
  end

  function ReaADR.character_filter_key(character)
    return sanitize_token(character):lower()
  end

  function ReaADR.character_filter_target_key(character, lane)
    return character_lane_key(character, lane):lower()
  end

  function ReaADR.encode_character_filter(characters)
    local tokens = {}
    for _, character in ipairs(characters or {}) do
      local token
      if type(character) == "table" then
        token = character.key or ReaADR.character_filter_target_key(character.character, character.lane)
      else
        token = ReaADR.character_filter_key(character)
      end
      if token ~= "" then
        tokens[#tokens + 1] = token
      end
    end
    table.sort(tokens)
    return table.concat(tokens, ",")
  end

  function ReaADR.active_character_filter()
    local _, value = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, "active_character_filter")
    local active = {}
    for token in tostring(value or ""):gmatch("([^,]+)") do
      active[token] = true
    end
    return active, value
  end

  function ReaADR.set_active_character_filter(characters)
    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "active_character_filter", ReaADR.encode_character_filter(characters))
    ReaADR.bump_session_revision()
  end

  function ReaADR.character_filter_hides_regions()
    local _, value = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, "character_filter_hide_regions")
    return value == "1"
  end

  function ReaADR.set_character_filter_hides_regions(enabled)
    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "character_filter_hide_regions", enabled and "1" or "0")
    ReaADR.bump_session_revision()
  end

  function ReaADR.character_filter_enabled()
    local _, value = ReaADR.active_character_filter()
    return value ~= nil and value ~= ""
  end

  function ReaADR.character_is_active(character)
    local active, value = ReaADR.active_character_filter()
    if value == nil or value == "" then
      return true
    end
    return active[ReaADR.character_filter_key(character)] == true
  end

  function ReaADR.character_lane_is_active(character, lane)
    local active, value = ReaADR.active_character_filter()
    if value == nil or value == "" then
      return true
    end
    local lane_key = ReaADR.character_filter_target_key(character, lane)
    local character_key = ReaADR.character_filter_key(character)
    return active[lane_key] == true or active[character_key] == true
  end

  function ReaADR.filter_cues_by_active_characters(cues)
    if not ReaADR.character_filter_enabled() then
      return cues or {}
    end

    assign_filter_lanes(cues)
    local filtered = {}
    for _, cue in ipairs(cues or {}) do
      if ReaADR.character_lane_is_active(cue.character, cue._reaadr_lane) then
        filtered[#filtered + 1] = cue
      end
    end
    return filtered
  end

  function ReaADR.collect_characters(cues)
    local characters = {}
    local seen = {}
    for _, cue in ipairs(cues or {}) do
      add_unique(characters, seen, first_nonempty(cue.character, "Unassigned"))
    end
    table.sort(characters)
    return characters
  end

  function ReaADR.character_region_lanes(cues)
    cues = cues or {}
    local assign_character_lanes = deps.get_assign_character_lanes()
    local setup_preroll_seconds = deps.get_setup_preroll_seconds()
    if assign_character_lanes then
      assign_character_lanes(cues, setup_preroll_seconds({ overlay_settings = ReaADR.load_overlay_settings() }))
    end
    local lanes = {}
    local characters = ReaADR.collect_characters(cues)
    local max_lanes = {}
    for _, cue in ipairs(cues) do
      local character = first_nonempty(cue.character, "Unassigned")
      max_lanes[character] = math.max(max_lanes[character] or 1, tonumber(cue._reaadr_lane) or 1)
    end
    local lane_index = 0
    for _, character in ipairs(characters) do
      local lane_count = math.max(1, tonumber(max_lanes[character]) or 1)
      for lane = 1, lane_count do
        local key = character_lane_key(character, lane)
        lanes[key] = lane_index
        if lane == 1 then
          lanes[character] = lane_index
        end
        lane_index = lane_index + 1
      end
    end
    return lanes
  end
end
