local root = (... and ... ~= "" and ...) or "."
local passed, failed = 0, 0

local function test(name, fn)
  local ok, err = xpcall(fn, debug.traceback)
  if ok then
    passed = passed + 1
    io.write("ok - " .. name .. "\n")
  else
    failed = failed + 1
    io.write("not ok - " .. name .. "\n" .. tostring(err) .. "\n")
  end
end

local function eq(actual, expected, label)
  if actual ~= expected then
    error((label or "values differ") .. ": expected " .. tostring(expected) .. ", got " .. tostring(actual), 2)
  end
end

local function make_persistence()
  local extstate = {}
  _G.reaper = {
    GetProjExtState = function(_, ns, key) return 1, extstate[ns .. ":" .. key] or "" end,
    SetProjExtState = function(_, ns, key, value) extstate[ns .. ":" .. key] = tostring(value or ""); return 1 end,
    TimeMap_curFrameRate = function() return 23.976 end,
  }
  local function encode(value)
    return tostring(value or ""):gsub("([^%w%-%_%. ])", function(c) return ("%%%02X"):format(c:byte()) end)
  end
  local function decode(value)
    return tostring(value or ""):gsub("%%(%x%x)", function(h) return string.char(tonumber(h, 16)) end)
  end
  local function serialize_metadata(metadata)
    local fields = {}
    for key, value in pairs(metadata or {}) do fields[#fields + 1] = encode(key) .. "=" .. encode(value) end
    table.sort(fields); return table.concat(fields, "&")
  end
  local function deserialize_metadata(value)
    local result = {}
    for pair in tostring(value or ""):gmatch("([^&]+)") do
      local key, item = pair:match("^([^=]*)=(.*)$")
      result[decode(key)] = decode(item)
    end
    return result
  end
  local ReaADR = {
    EXT_NAMESPACE = "ReaADRTools",
    DEFAULT_OVERLAY_SETTINGS = {},
    session_revision = function() return tonumber(extstate["ReaADRTools:session_revision"]) or 0 end,
    bump_session_revision = function()
      local value = (tonumber(extstate["ReaADRTools:session_revision"]) or 0) + 1
      extstate["ReaADRTools:session_revision"] = tostring(value)
      return value
    end,
  }
  dofile(root .. "/scripts/ReaADR_Core_Persistence.lua")(ReaADR, {
    project = function() return 0 end,
    sanitize_token = function(v) return tostring(v or "") end,
    string_to_bool = function(v, d) if v == "" then return d end return v == "1" end,
    bool_to_string = function(v) return v and "1" or "0" end,
    encode_cache_field = encode, decode_cache_field = decode,
    serialize_metadata = serialize_metadata, deserialize_metadata = deserialize_metadata,
    normalize_status = function(v) return v == "" and "Not Recorded" or v end,
    CUE_CACHE_FIELDS = { "id", "character", "start_time", "end_time", "line", "notes", "direction", "cue_type", "source_line", "status", "script_id", "script_name", "script_revision", "import_timestamp", "metadata" },
  })
  return ReaADR, extstate
end

test("session model round trip preserves escaped fields and metadata", function()
  local adr = make_persistence()
  local session = adr.build_adr_session({{
    id = "01", character = "Miyuki 雪", start_time = 1, end_time = 2,
    line = "tab\tline\nnext=", notes = "", script_id = "script-1",
    script_name = "A=B", metadata = { empty = "", unicode = "café", delimiters = "\t\n=" },
  }}, { session_name = "Session\nOne", project_metadata = { empty = "", path = "a\tb=c\n雪" } })
  session.session_state.active_script_id = "script-1"
  adr.save_adr_session(session)
  local loaded = assert(adr.load_adr_session())
  eq(loaded.session_name, "Session\nOne")
  eq(loaded.project_metadata.empty, "")
  eq(loaded.project_metadata.path, "a\tb=c\n雪")
  eq(loaded.cues[1].line, "tab\tline\nnext=")
  eq(loaded.cues[1].metadata.unicode, "café")
  eq(loaded.cues[1].metadata.empty, "")
  eq(loaded.session_state.active_script_id, "script-1")
end)

test("valid empty session differs from missing and invalid models", function()
  local adr, extstate = make_persistence()
  local cues, _, code = adr.load_session_cues()
  eq(cues, nil); eq(code, "missing_session_model")
  extstate["ReaADRTools:adr_session_model_v1"] = "session\tsession_id="
  cues, _, code = adr.load_session_cues()
  eq(cues, nil); eq(code, "invalid_session_model")
  adr.save_adr_session(adr.build_adr_session({}, { session_name = "Empty" }))
  cues, _, code = adr.load_session_cues()
  eq(#cues, 0); eq(code, "session_model")
end)

test("cue edit preserves session envelope import identity and stable IDs", function()
  local adr = make_persistence()
  local original = adr.build_adr_session({{
    id = "01", character = "A", start_time = 1, end_time = 2, line = "old",
    script_id = "s1", script_name = "Script", import_timestamp = "original",
  }}, { session_name = "Named", project_metadata = { studio = "X" } })
  original.session_state.active_script_id = "s1"
  original.session_state.custom_state = "keep"
  original.import_registry[1].file_hash = "original-file-identity"
  original.scripts[1].metadata = { draft = "yes" }
  adr.save_adr_session(original)
  local caller = {{ id = "01", character = "A", start_time = 1, end_time = 3, line = "new", script_id = "s1", script_name = "Script" }}
  adr.save_session_cues(caller, { last_operation = "edit" })
  eq(caller[1].session_cue_id, nil, "caller table mutated")
  local loaded = assert(adr.load_adr_session())
  eq(loaded.session_name, "Named")
  eq(loaded.project_metadata.studio, "X")
  eq(loaded.import_registry[1].file_hash, "original-file-identity")
  eq(loaded.session_state.active_script_id, "s1")
  eq(loaded.session_state.custom_state, "keep")
  eq(loaded.scripts[1].metadata.draft, "yes")
  eq(loaded.session_id, original.session_id)
  eq(loaded.cues[1].session_cue_id, original.cues[1].session_cue_id)
  eq(loaded.cues[1].region_id, original.cues[1].region_id)
  eq(loaded.tracks[1].track_id, original.tracks[1].track_id)
end)

test("model-only snapshot restore publishes one new revision", function()
  local adr, extstate = make_persistence()
  adr.save_adr_session(adr.build_adr_session({}, { session_name = "Before" }))
  extstate["ReaADRTools:session_revision"] = "5"
  local snapshot = adr.create_session_snapshot("test")
  extstate["ReaADRTools:session_revision"] = "6"
  adr.save_adr_session(adr.build_adr_session({}, { session_name = "After" }))
  assert(adr.restore_session_model_snapshot(snapshot, "test rollback"))
  eq(extstate["ReaADRTools:session_revision"], "7")
  eq(assert(adr.load_adr_session()).session_name, "Before")
end)

test("record arm isolation and every cleanup route restore all tracks", function()
  for _, route in ipairs({ "normal", "external", "escape", "error" }) do
    local tracks = {{ arm = 1 }, { arm = 0 }, { arm = 1 }}
    local api = {
      CountTracks = function() return #tracks end,
      GetTrack = function(_, index) return tracks[index + 1] end,
      GetMediaTrackInfo_Value = function(track) return track.arm end,
      SetMediaTrackInfo_Value = function(track, _, value) track.arm = value end,
    }
    local manager = dofile(root .. "/scripts/ReaADR_Record_Arm.lua")(api, 0)
    assert(manager.capture_and_isolate(tracks[2]))
    eq(tracks[1].arm, 0, route); eq(tracks[2].arm, 1, route); eq(tracks[3].arm, 0, route)
    manager.restore(); manager.restore()
    eq(tracks[1].arm, 1, route); eq(tracks[2].arm, 0, route); eq(tracks[3].arm, 1, route)
  end
end)

test("failed synchronization balances undo and recovers partial model/project mutation", function()
  local object_count, begin_count, end_count, undo_count, ui_balance = 2, 0, 0, 0, 0
  local model, model_snapshot = "before", "before"
  local before
  local last_description
  _G.reaper = {
    Undo_BeginBlock = function() begin_count = begin_count + 1; before = object_count end,
    Undo_EndBlock = function(description) end_count = end_count + 1; last_description = description end,
    Undo_CanUndo2 = function() return last_description end,
    Undo_DoUndo2 = function() undo_count = undo_count + 1; object_count = before end,
    PreventUIRefresh = function(delta) ui_balance = ui_balance + delta end,
  }
  local adr = {}
  dofile(root .. "/scripts/ReaADR_Core_Transactions.lua")(adr, { project = function() return 0 end })
  local result = adr.with_project_transaction({ description = "outer" }, function()
    model = "partially updated"
    object_count = 3
    local inner = adr.with_project_transaction({ description = "inner" }, function()
      object_count = 4
      error("halfway")
    end)
    if not inner then error("sync failed") end
  end)
  if not result then model = model_snapshot end
  eq(result, nil); eq(object_count, 2); eq(begin_count, 1); eq(end_count, 1); eq(undo_count, 1)
  eq(model, "before")
  local ui_result = adr.with_ui_refresh_suppressed(function() error("render failed") end)
  eq(ui_result, nil); eq(ui_balance, 0, "UI refresh imbalance")
end)

test("character and lane keys normalize deterministically", function()
  _G.reaper = { GetProjExtState = function() return 1, "" end }
  local adr = { EXT_NAMESPACE = "ReaADRTools", load_overlay_settings = function() return {} end }
  local lane_key = function(character, lane) return character:gsub("%s+", "_"):lower() .. ".lane" .. lane end
  dofile(root .. "/scripts/ReaADR_Core_Characters.lua")(adr, {
    first_nonempty = function(value, fallback) return value ~= "" and value or fallback end,
    project = function() return 0 end,
    sanitize_token = function(value) return tostring(value):gsub("%s+", "_"):gsub("[^%w_]", "") end,
    get_character_lane_key = function() return lane_key end,
    get_setup_preroll_seconds = function() return function() return 3 end end,
    get_assign_character_lanes = function() return function(cues) for _, cue in ipairs(cues) do cue._reaadr_lane = cue._reaadr_lane or 1 end end end,
  })
  eq(adr.character_filter_key("  Lead Actor! "), "_lead_actor_")
  eq(adr.character_filter_target_key("Lead Actor", 2), "lead_actor.lane2")
  eq(adr.encode_character_filter({ { character = "Lead Actor", lane = 2 }, "Beta" }), "beta,lead_actor.lane2")
end)

test("destructive ownership predicate ignores user objects", function()
  local ownership = dofile(root .. "/scripts/ReaADR_Core_Ownership.lua")
  local keys = { cue_1 = true }
  eq(ownership.cue_audio_item_matches("cue_audio", "cue_1", keys), true)
  eq(ownership.cue_audio_item_matches("", "cue_1", keys), false)
  eq(ownership.cue_audio_item_matches("cue_audio", "user_item", keys), false)
  eq(ownership.generated_track_role("character"), true)
  eq(ownership.generated_track_role("user"), false)
end)

io.write(("%d passed, %d failed\n"):format(passed, failed))
if failed > 0 then os.exit(1) end
