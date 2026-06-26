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
    return registry
  end

  function ReaADR.load_script_registry()
    local session = ReaADR.load_adr_session and ReaADR.load_adr_session()
    if not session then
      return {}
    end
    return ReaADR.rebuild_script_registry(session.cues or {})
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

  function ReaADR.cue_manager_auto_dock_enabled()
    local _, value = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, "ui.cue_manager_auto_dock")
    return value == "1" or value == "true" or value == "yes"
  end

  function ReaADR.set_cue_manager_auto_dock_enabled(enabled)
    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "ui.cue_manager_auto_dock", enabled and "1" or "0")
    if enabled then
      ReaADR.save_window_geometry("cue_manager", {
        dock = 1,
        width = 980,
        height = 700,
      })
    end
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
    if defaults.force_dock and state.dock == 0 then
      state.dock = tonumber(defaults.dock) or 1
    end
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

  local function trim(value)
    return tostring(value or ""):match("^%s*(.-)%s*$")
  end

  local function stable_id(prefix, ...)
    local text = table.concat({ ... }, "|")
    local hash = 2166136261
    local bitlib = bit32 or bit
    for i = 1, #text do
      if bitlib then
        hash = bitlib.band(bitlib.bxor(hash, text:byte(i)), 0xFFFFFFFF)
        hash = bitlib.band(hash * 16777619, 0xFFFFFFFF)
      else
        hash = (hash + text:byte(i)) % 0x100000000
        hash = (hash * 16777619) % 0x100000000
      end
    end
    return ("%s_%08x"):format(prefix, hash)
  end

  local function cue_identity(cue, character_id)
    return table.concat({
      trim(cue.script_id),
      character_id or stable_id("character", trim(cue.script_id), trim(cue.character)),
      trim(cue.id),
    }, ":")
  end

  local function serialize_pairs(prefix, fields)
    local keys = {}
    for key in pairs(fields or {}) do
      keys[#keys + 1] = key
    end
    table.sort(keys)
    local parts = { prefix }
    for _, key in ipairs(keys) do
      parts[#parts + 1] = encode_cache_field(key) .. "=" .. encode_cache_field(fields[key])
    end
    return table.concat(parts, "\t")
  end

  local function parse_pairs(line)
    local fields = {}
    local first = true
    for token in tostring(line or ""):gmatch("([^\t]+)") do
      if first then
        first = false
      else
        local raw_key, raw_value = token:match("^([^=]*)=(.*)$")
        local key = decode_cache_field(raw_key or "")
        if key ~= "" then
          fields[key] = decode_cache_field(raw_value or "")
        end
      end
    end
    return fields
  end

  function ReaADR.build_adr_session(cues, options)
    options = options or {}
    cues = cues or {}
    local _, existing_session_id = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, "adr_session_id")
    local session_id = existing_session_id ~= "" and existing_session_id or stable_id("session", os.time(), #cues)
    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "adr_session_id", session_id)

    local session = {
      session_id = session_id,
      session_name = tostring(options.session_name or ""),
      project_metadata = options.project_metadata or {},
      scripts = {},
      characters = {},
      cues = {},
      tracks = {},
      regions = {},
      timecode_settings = {
        frame_rate = tostring(options.frame_rate or (reaper.TimeMap_curFrameRate and reaper.TimeMap_curFrameRate(project())) or 24),
      },
      import_registry = {},
      session_state = {
        active_script_id = "",
        refresh_version = tostring(ReaADR.session_revision and ReaADR.session_revision() or 0),
        last_operation = tostring(options.last_operation or "save_session"),
        dirty_flags = {
          cues_modified = options.cues_modified and "true" or "false",
          tracks_modified = options.tracks_modified and "true" or "false",
          regions_modified = options.regions_modified and "true" or "false",
        },
      },
    }

    local script_map = {}
    local character_map = {}
    local track_map = {}

    for _, cue in ipairs(cues) do
      local script_id = trim(cue.script_id)
      if script_id == "" then
        script_id = "manual"
        cue.script_id = script_id
      end
      local script = script_map[script_id]
      if not script then
        script = {
          script_id = script_id,
          script_name = cue.script_name or "",
          source_file = cue.source_file or "",
          import_timestamp = cue.import_timestamp or "",
          revision_id = cue.script_revision or "",
          characters = {},
          cue_count = 0,
          metadata = {},
        }
        script_map[script_id] = script
        session.scripts[#session.scripts + 1] = script
      end
      script.cue_count = script.cue_count + 1

      local character_name = trim(cue.character)
      if character_name == "" then character_name = "Unassigned" end
      local character_id = stable_id("character", script_id, character_name)
      local character = character_map[character_id]
      if not character then
        character = {
          character_id = character_id,
          character_name = character_name,
          script_id = script_id,
          cue_count = 0,
          status = "active",
          import_state = "imported",
        }
        character_map[character_id] = character
        session.characters[#session.characters + 1] = character
        script.characters[#script.characters + 1] = character_id
      end
      character.cue_count = character.cue_count + 1

      local lane = tonumber(cue._reaadr_lane) or 1
      local cue_track_id = stable_id("track", character_id, "cues", lane)
      local dialogue_track_id = stable_id("track", character_id, "dialogue", lane)
      if not track_map[cue_track_id] then
        track_map[cue_track_id] = {
          track_id = cue_track_id,
          character_id = character_id,
          track_type = "cues",
          track_name = lane > 1 and ("%s Cues #%d"):format(character_name, lane) or ("%s Cues"):format(character_name),
          assigned_cues = {},
        }
        session.tracks[#session.tracks + 1] = track_map[cue_track_id]
      end
      if not track_map[dialogue_track_id] then
        track_map[dialogue_track_id] = {
          track_id = dialogue_track_id,
          character_id = character_id,
          track_type = "dialogue",
          track_name = lane > 1 and ("%s #%d"):format(character_name, lane) or character_name,
          assigned_cues = {},
        }
        session.tracks[#session.tracks + 1] = track_map[dialogue_track_id]
      end

      local identity = cue_identity(cue, character_id)
      track_map[cue_track_id].assigned_cues[#track_map[cue_track_id].assigned_cues + 1] = identity
      track_map[dialogue_track_id].assigned_cues[#track_map[dialogue_track_id].assigned_cues + 1] = identity

      local region_id = stable_id("region", identity)
      cue.character_id = character_id
      cue.track_id = cue_track_id
      cue.region_id = region_id
      cue.session_cue_id = identity
      session.cues[#session.cues + 1] = cue
      session.regions[#session.regions + 1] = {
        region_id = region_id,
        cue_id = identity,
        start_time = tostring(tonumber(cue.start_time) or 0),
        end_time = tostring(tonumber(cue.end_time) or tonumber(cue.start_time) or 0),
        color = tostring(cue.color or ""),
        label = cue.id and tostring(cue.id) or "",
      }
    end

    for _, script in ipairs(session.scripts) do
      session.import_registry[#session.import_registry + 1] = {
        script_id = script.script_id,
        file_hash = stable_id("file", script.script_id, script.script_name, script.cue_count),
        import_timestamp = script.import_timestamp,
        imported_characters = table.concat(script.characters, ","),
        cue_snapshot_hash = stable_id("cues", script.script_id, script.cue_count, #cues),
      }
    end

    return session
  end

  local function serialize_session_model(session)
    local lines = {
      serialize_pairs("session", {
        session_id = session.session_id,
        session_name = session.session_name,
      }),
      serialize_pairs("timecode", session.timecode_settings or {}),
      serialize_pairs("state", {
        active_script_id = session.session_state and session.session_state.active_script_id or "",
        refresh_version = session.session_state and session.session_state.refresh_version or "",
        last_operation = session.session_state and session.session_state.last_operation or "",
      }),
    }
    local dirty = session.session_state and session.session_state.dirty_flags or {}
    for key, value in pairs(dirty) do
      lines[#lines + 1] = serialize_pairs("dirty", { key = key, value = value })
    end
    for _, script in ipairs(session.scripts or {}) do
      lines[#lines + 1] = serialize_pairs("script", {
        script_id = script.script_id,
        script_name = script.script_name,
        source_file = script.source_file,
        import_timestamp = script.import_timestamp,
        revision_id = script.revision_id,
        characters = table.concat(script.characters or {}, ","),
        cue_count = script.cue_count,
      })
    end
    for _, character in ipairs(session.characters or {}) do
      lines[#lines + 1] = serialize_pairs("character", character)
    end
    for _, cue in ipairs(session.cues or {}) do
      local fields = {}
      for _, key in ipairs(CUE_CACHE_FIELDS) do
        fields[key] = key == "metadata" and serialize_metadata(cue.metadata) or cue[key]
      end
      fields.character_id = cue.character_id
      fields.region_id = cue.region_id
      fields.track_id = cue.track_id
      fields.session_cue_id = cue.session_cue_id
      lines[#lines + 1] = serialize_pairs("cue", fields)
    end
    for _, track in ipairs(session.tracks or {}) do
      lines[#lines + 1] = serialize_pairs("track", {
        track_id = track.track_id,
        character_id = track.character_id,
        track_type = track.track_type,
        track_name = track.track_name,
        assigned_cues = table.concat(track.assigned_cues or {}, ","),
      })
    end
    for _, region in ipairs(session.regions or {}) do
      lines[#lines + 1] = serialize_pairs("region", region)
    end
    for _, record in ipairs(session.import_registry or {}) do
      lines[#lines + 1] = serialize_pairs("import", record)
    end
    return table.concat(lines, "\n")
  end

  local function deserialize_session_model(value)
    value = tostring(value or "")
    if value == "" then return nil end
    local session = {
      project_metadata = {},
      scripts = {},
      characters = {},
      cues = {},
      tracks = {},
      regions = {},
      timecode_settings = {},
      import_registry = {},
      session_state = { dirty_flags = {} },
    }
    for line in (value .. "\n"):gmatch("([^\n]*)\n") do
      if line ~= "" then
        local record_type = line:match("^([^\t]+)")
        local fields = parse_pairs(line)
        if record_type == "session" then
          session.session_id = fields.session_id or ""
          session.session_name = fields.session_name or ""
        elseif record_type == "timecode" then
          session.timecode_settings = fields
        elseif record_type == "state" then
          session.session_state.active_script_id = fields.active_script_id or ""
          session.session_state.refresh_version = fields.refresh_version or ""
          session.session_state.last_operation = fields.last_operation or ""
        elseif record_type == "dirty" then
          session.session_state.dirty_flags[fields.key or ""] = fields.value or ""
        elseif record_type == "script" then
          local chars = {}
          for id in tostring(fields.characters or ""):gmatch("([^,]+)") do chars[#chars + 1] = id end
          fields.characters = chars
          fields.cue_count = tonumber(fields.cue_count) or 0
          session.scripts[#session.scripts + 1] = fields
        elseif record_type == "character" then
          fields.cue_count = tonumber(fields.cue_count) or 0
          session.characters[#session.characters + 1] = fields
        elseif record_type == "cue" then
          local cue = fields
          cue.start_time = tonumber(cue.start_time) or 0
          cue.end_time = tonumber(cue.end_time) or cue.start_time
          cue.source_line = tonumber(cue.source_line) or 0
          cue.status = normalize_status(cue.status)
          cue.metadata = deserialize_metadata(cue.metadata)
          session.cues[#session.cues + 1] = cue
        elseif record_type == "track" then
          local assigned = {}
          for id in tostring(fields.assigned_cues or ""):gmatch("([^,]+)") do assigned[#assigned + 1] = id end
          fields.assigned_cues = assigned
          session.tracks[#session.tracks + 1] = fields
        elseif record_type == "region" then
          fields.start_time = tonumber(fields.start_time) or 0
          fields.end_time = tonumber(fields.end_time) or fields.start_time
          session.regions[#session.regions + 1] = fields
        elseif record_type == "import" then
          session.import_registry[#session.import_registry + 1] = fields
        end
      end
    end
    if not session.session_id or session.session_id == "" then return nil end
    return session
  end

  function ReaADR.save_adr_session(session)
    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "adr_session_model_v1", serialize_session_model(session))
    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "adr_session_id", session.session_id or "")
    return true
  end

  function ReaADR.load_adr_session()
    local _, value = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, "adr_session_model_v1")
    local session = deserialize_session_model(value)
    if session then
      return session
    end
    return nil, "No ADR session model found. Import or generate cues first."
  end

  function ReaADR.save_session_cues(cues, options)
    options = options or {}
    local session = ReaADR.build_adr_session(cues or {}, {
      last_operation = options.last_operation or "save_cues",
      cues_modified = true,
    })
    ReaADR.save_adr_session(session)
    local revision = ReaADR.bump_session_revision()
    if options.emit_event ~= false and ReaADR.emit_event then
      ReaADR.emit_event(options.event_type or "SessionSaved", {
        cue_count = #(cues or {}),
        revision = revision,
        operation = options.last_operation or "save_cues",
      }, {
        source = options.source or "session",
        session_id = session.session_id,
        batch_id = options.batch_id,
      })
    end
  end

  function ReaADR.create_session_snapshot(label)
    label = tostring(label or "operation")
    local timestamp = os.date("!%Y-%m-%dT%H:%M:%SZ")
    local _, model_blob = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, "adr_session_model_v1")
    local _, revision = reaper.GetProjExtState(project(), ReaADR.EXT_NAMESPACE, "session_revision")
    local snapshot = {
      label = label,
      timestamp = timestamp,
      model_blob = model_blob or "",
      revision = revision or "",
    }
    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "session_snapshot_last_label", label)
    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "session_snapshot_last_timestamp", timestamp)
    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "session_snapshot_last_model_v1", snapshot.model_blob)
    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "session_snapshot_last_revision", snapshot.revision)
    if ReaADR.log then
      ReaADR.log("INFO", "SNAPSHOT", "Session snapshot created", { detail = label })
    end
    return snapshot
  end

  function ReaADR.restore_session_snapshot(snapshot, reason)
    if not snapshot then
      return false, "No session snapshot is available."
    end
    reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "adr_session_model_v1", snapshot.model_blob or "")
    if snapshot.revision and snapshot.revision ~= "" then
      reaper.SetProjExtState(project(), ReaADR.EXT_NAMESPACE, "session_revision", snapshot.revision)
    end
    ReaADR.bump_session_revision()
    if ReaADR.log then
      ReaADR.log("WARN", "RESTORE", "Session snapshot restored", {
        detail = tostring(reason or snapshot.label or "unknown"),
      })
    end
    return true
  end

  function ReaADR.load_session_cues()
    local session = ReaADR.load_adr_session and ReaADR.load_adr_session()
    if session and session.cues and #session.cues > 0 then
      return session.cues
    end
    return nil, "No ADR session model found. Import or generate cues first."
  end
end
