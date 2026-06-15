return function(ReaADR, deps)
  local project = deps.project
  local sanitize_token = deps.sanitize_token
  local string_to_bool = deps.string_to_bool
  local bool_to_string = deps.bool_to_string
  local encode_cache_field = deps.encode_cache_field
  local decode_cache_field = deps.decode_cache_field
  local serialize_metadata = deps.serialize_metadata
  local deserialize_metadata = deps.deserialize_metadata
  local normalize_status = deps.normalize_status
  local CUE_CACHE_FIELDS = deps.CUE_CACHE_FIELDS

  local function count_script_characters(cues)
    local counts = {}
    for _, cue in ipairs(cues or {}) do
      local character = tostring(cue.character or "")
      if character ~= "" then
        counts[character] = (counts[character] or 0) + 1
      end
    end
    return counts
  end

  local function serialize_character_counts(counts)
    local names = {}
    for character in pairs(counts or {}) do
      names[#names + 1] = character
    end
    table.sort(names)

    local parts = {}
    for _, character in ipairs(names) do
      parts[#parts + 1] = encode_cache_field(character) .. ":" .. tostring(tonumber(counts[character]) or 0)
    end
    return table.concat(parts, "|")
  end

  local function deserialize_character_counts(value)
    local counts = {}
    for token in tostring(value or ""):gmatch("([^|]+)") do
      local raw_name, raw_count = token:match("^([^:]*):(.*)$")
      local name = decode_cache_field(raw_name or "")
      if name ~= "" then
        counts[name] = tonumber(raw_count) or 0
      end
    end
    return counts
  end

  local function serialize_script_registry(registry)
    local script_ids = {}
    for script_id in pairs(registry or {}) do
      script_ids[#script_ids + 1] = script_id
    end
    table.sort(script_ids)

    local lines = {}
    for _, script_id in ipairs(script_ids) do
      local entry = registry[script_id] or {}
      local fields = {
        "script_id=" .. encode_cache_field(script_id),
        "script_name=" .. encode_cache_field(entry.script_name or ""),
        "script_revision=" .. encode_cache_field(entry.script_revision or ""),
        "import_timestamp=" .. encode_cache_field(entry.import_timestamp or ""),
        "cue_count=" .. tostring(tonumber(entry.cue_count) or 0),
        "character_counts=" .. serialize_character_counts(entry.character_counts or {}),
      }
      for key, value in pairs(entry.optional_fields or {}) do
        if tostring(value or "") ~= "" then
          fields[#fields + 1] = "field." .. encode_cache_field(key) .. "=" .. encode_cache_field(value)
        end
      end
      table.sort(fields)
      lines[#lines + 1] = table.concat(fields, "&")
    end
    return table.concat(lines, "\n")
  end

  local function deserialize_script_registry(value)
    local registry = {}
    value = tostring(value or "")
    if value == "" then
      return registry
    end

    value = value .. "\n"
    for line in value:gmatch("([^\n]*)\n") do
      if line ~= "" then
        local entry = { optional_fields = {} }
        for pair in line:gmatch("([^&]+)") do
          local raw_key, raw_value = pair:match("^([^=]+)=(.*)$")
          local key = tostring(raw_key or "")
          local decoded = decode_cache_field(raw_value or "")
          if key == "script_id" then
            entry.script_id = decoded
          elseif key == "script_name" then
            entry.script_name = decoded
          elseif key == "script_revision" then
            entry.script_revision = decoded
          elseif key == "import_timestamp" then
            entry.import_timestamp = decoded
          elseif key == "cue_count" then
            entry.cue_count = tonumber(decoded) or 0
          elseif key == "character_counts" then
            entry.character_counts = deserialize_character_counts(raw_value or "")
          elseif key:match("^field%.") then
            local field_name = decode_cache_field(key:sub(7))
            if field_name ~= "" then
              entry.optional_fields[field_name] = decoded
            end
          end
        end
        if entry.script_id and entry.script_id ~= "" then
          entry.character_counts = entry.character_counts or {}
          registry[entry.script_id] = entry
        end
      end
    end
    return registry
  end

  function ReaADR.rebuild_script_registry(cues)
    local registry = {}
    for _, cue in ipairs(cues or {}) do
      local script_id = tostring(cue.script_id or ""):match("^%s*(.-)%s*$")
      if script_id ~= "" then
        local entry = registry[script_id]
        if not entry then
          entry = {
            script_id = script_id,
            script_name = cue.script_name or "",
            script_revision = cue.script_revision or "",
            import_timestamp = cue.import_timestamp or "",
            cue_count = 0,
            character_counts = {},
            optional_fields = {},
          }
          registry[script_id] = entry
        end
        entry.cue_count = entry.cue_count + 1
        if entry.script_name == "" and tostring(cue.script_name or "") ~= "" then
          entry.script_name = cue.script_name
        end
        if entry.script_revision == "" and tostring(cue.script_revision or "") ~= "" then
          entry.script_revision = cue.script_revision
        end
        if entry.import_timestamp == "" and tostring(cue.import_timestamp or "") ~= "" then
          entry.import_timestamp = cue.import_timestamp
        end

        local character = tostring(cue.character or "")
        if character ~= "" then
          entry.character_counts[character] = (entry.character_counts[character] or 0) + 1
        end

        for key, value in pairs(cue.metadata or {}) do
          if tostring(value or "") ~= "" and entry.optional_fields[key] == nil then
            entry.optional_fields[key] = value
          end
        end
      end
    end

    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "script_registry_v1", serialize_script_registry(registry))
    return registry
  end

  function ReaADR.load_script_registry()
    local _, value = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, "script_registry_v1")
    return deserialize_script_registry(value)
  end

  local function ui_window_key(window_id, field)
    return ("ui.window.%s.%s"):format(sanitize_token(window_id), field)
  end

  function ReaADR.window_layout_enabled()
    local _, value = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, "ui.remember_window_layout")
    if value == "" then
      return false
    end
    return value == "1" or value == "true" or value == "yes"
  end

  function ReaADR.set_window_layout_enabled(enabled)
    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "ui.remember_window_layout", enabled and "1" or "0")
  end

  function ReaADR.cue_hover_preview_enabled()
    local _, value = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, "ui.cue_hover_preview")
    if value == "" then
      return true
    end
    return value == "1" or value == "true" or value == "yes"
  end

  function ReaADR.set_cue_hover_preview_enabled(enabled)
    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "ui.cue_hover_preview", enabled and "1" or "0")
  end

  function ReaADR.load_window_state(window_id, defaults)
    defaults = defaults or {}
    local state = {
      width = tonumber(defaults.width) or 800,
      height = tonumber(defaults.height) or 600,
      dock = tonumber(defaults.dock) or 0,
      x = tonumber(defaults.x),
      y = tonumber(defaults.y),
    }
    if not ReaADR.window_layout_enabled() then
      return state
    end

    local _, width = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, ui_window_key(window_id, "width"))
    local _, height = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, ui_window_key(window_id, "height"))
    local _, dock = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, ui_window_key(window_id, "dock"))
    local _, x = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, ui_window_key(window_id, "x"))
    local _, y = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, ui_window_key(window_id, "y"))

    state.width = tonumber(width) or state.width
    state.height = tonumber(height) or state.height
    state.dock = tonumber(dock) or state.dock
    state.x = tonumber(x) or state.x
    state.y = tonumber(y) or state.y
    return state
  end

  function ReaADR.init_persistent_window(window_id, title, defaults)
    local state = ReaADR.load_window_state(window_id, defaults)
    if state.x ~= nil and state.y ~= nil then
      gfx.init(title, state.width, state.height, state.dock, state.x, state.y)
    else
      gfx.init(title, state.width, state.height, state.dock)
    end
    return state
  end

  function ReaADR.save_window_geometry(window_id, geometry)
    geometry = geometry or {}
    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, ui_window_key(window_id, "dock"), tostring(tonumber(geometry.dock) or 0))
    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, ui_window_key(window_id, "x"), tostring(tonumber(geometry.x) or 0))
    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, ui_window_key(window_id, "y"), tostring(tonumber(geometry.y) or 0))
    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, ui_window_key(window_id, "width"), tostring(tonumber(geometry.width) or 0))
    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, ui_window_key(window_id, "height"), tostring(tonumber(geometry.height) or 0))
    return true
  end

  function ReaADR.save_window_state(window_id)
    if not ReaADR.window_layout_enabled() then
      return false
    end
    if not gfx or not gfx.w or not gfx.h or not gfx.dock then
      return false
    end

    local ok, dock, x, y, width, height = pcall(gfx.dock, -1, 0, 0, 0, 0)
    if not ok then
      dock = 0
      x = 0
      y = 0
      width = gfx.w
      height = gfx.h
    end

    return ReaADR.save_window_geometry(window_id, {
      dock = dock,
      x = x,
      y = y,
      width = tonumber(width) or gfx.w or 0,
      height = tonumber(height) or gfx.h or 0,
    })
  end

  function ReaADR.load_overlay_settings()
    local settings = {}
    for key, default in pairs(ReaADR.DEFAULT_OVERLAY_SETTINGS) do
      local value = ReaADR.get_setting(key, "")
      if type(default) == "boolean" then
        settings[key] = string_to_bool(value, default)
      elseif type(default) == "number" then
        settings[key] = tonumber(value) or default
      else
        settings[key] = value ~= "" and value or default
      end
    end
    return settings
  end

  function ReaADR.save_overlay_settings(settings)
    for key, default in pairs(ReaADR.DEFAULT_OVERLAY_SETTINGS) do
      local value = settings[key]
      if value == nil then
        value = default
      end

      if type(default) == "boolean" then
        ReaADR.set_setting(key, bool_to_string(value))
      else
        ReaADR.set_setting(key, value)
      end
    end
  end

  function ReaADR.save_last_import_cues(cues)
    local lines = {}
    for _, cue in ipairs(cues or {}) do
      local fields = {}
      for index, key in ipairs(CUE_CACHE_FIELDS) do
        local value = key == "metadata" and serialize_metadata(cue.metadata) or cue[key]
        fields[index] = encode_cache_field(value)
      end
      lines[#lines + 1] = table.concat(fields, "\t")
    end

    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "last_import_cues_v1", table.concat(lines, "\n"))
    ReaADR.rebuild_script_registry(cues or {})
    ReaADR.bump_session_revision()
  end

  function ReaADR.load_last_import_cues()
    local _, value = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, "last_import_cues_v1")
    if value == "" then
      return nil, "No cached cue sheet data found. Run Import Cue Sheet once first."
    end

    local cues = {}
    value = value .. "\n"
    for line in value:gmatch("([^\n]*)\n") do
      if line ~= "" then
        local cue = {}
        local field_index = 1
        for raw_field in (line .. "\t"):gmatch("([^\t]*)\t") do
          local key = CUE_CACHE_FIELDS[field_index]
          if key then
            cue[key] = decode_cache_field(raw_field)
          end
          field_index = field_index + 1
        end

        cue.start_time = tonumber(cue.start_time) or 0
        cue.end_time = tonumber(cue.end_time) or cue.start_time
        cue.source_line = tonumber(cue.source_line) or 0
        cue.id = cue.id or ""
        cue.character = cue.character or ""
        cue.line = cue.line or ""
        cue.notes = cue.notes or ""
        cue.direction = cue.direction or ""
        cue.cue_type = cue.cue_type or ""
        cue.status = normalize_status(cue.status)
        cue.script_id = cue.script_id or ""
        cue.script_name = cue.script_name or ""
        cue.script_revision = cue.script_revision or ""
        cue.import_timestamp = cue.import_timestamp or ""
        cue.metadata = deserialize_metadata(cue.metadata)
        cues[#cues + 1] = cue
      end
    end

    if #cues == 0 then
      return nil, "Cached cue sheet data is empty. Run Import Cue Sheet again."
    end

    ReaADR.rebuild_script_registry(cues)
    return cues
  end
end
