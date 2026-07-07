-- ReaADR application framework.
-- Centralizes user-facing workflows while existing feature scripts remain as
-- internal modules during the architecture transition.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local App = {}
App.base_dir = script_dir()
App.ReaADR = dofile(App.base_dir .. "/ReaADR_Core.lua")

App.modules = {
  import = {
    title = "Import",
    actions = {
      { label = "Import Cue Sheet", script = "ReaADR_Import_Cue_Sheet.lua", hint = "Import CSV or TSV script data, map columns, create tracks, regions, cue audio, and overlay metadata." },
      { label = "Detect Dialogue From Selected Media", script = "ReaADR_Detect_Dialogue.lua", hint = "Analyze selected audio/video media and create editable ADR cues from detected speech regions." },
      { label = "Generate Cues from Markers/Regions", script = "ReaADR_Generate_Cues.lua", hint = "Create ADR cue items from existing project markers or regions without importing a cue sheet." },
    },
  },
  cues = {
    title = "Cue Management",
    actions = {
      { label = "Open Cue Manager",   script = "ReaADR_Cue_Manager.lua",  hint = "Browse cues, jump around the session, update cue status, and refresh the active overlay." },
    },
  },
  session = {
    title = "Session Tools",
    actions = {
      { label = "Check Session", app_action = "validate_session", hint = "Check cue timing, missing fields, overlap splits, metadata, and generated session items." },
      { label = "Refresh Session", app_action = "rebuild_session", hint = "Repair missing or changed generated tracks, cue regions, cue audio, and overlays." },
      { label = "Update Cues From Regions", app_action = "update_cues_from_regions", hint = "Save current ReaADR region start/end times back to the cue session, then rebuild cue audio and overlays." },
      { label = "Clear Character Cues",      script = "ReaADR_Clean_Generated_Cues.lua", hint = "Select characters whose cues, regions, and cue tracks should be removed after their recording session is complete. Recording tracks and takes are preserved." },
    },
  },
  reports = {
    title = "Reports",
    actions = {
      { label = "Export Cue Sheet CSV", script = "ReaADR_Export_Cue_Sheet.lua", hint = "Export regions and cues to a flexible CSV for editing or reimporting later." },
    },
  },
  overlay = {
    title = "Video Overlays",
    actions = {
      { label = "Refresh Video Overlay", script = "ReaADR_Overlay.lua",         hint = "Rebuild the video overlay FX from the current session cue data." },
    },
  },
  preferences = {
    title = "Preferences",
    actions = {},
  },
  help = {
    title = "Help",
    actions = {
      { label = "Search Help", app_action = "search_help", hint = "Search the built-in user guide by action, workflow, or keyword." },
      { label = "Import Help", app_action = "help_import", hint = "Show import, column mapping, metadata, and session build guidance." },
      { label = "Cue Management Help", app_action = "help_cues", hint = "Show navigation, cue status, filtering, and cue manager guidance." },
      { label = "Overlay Help", app_action = "help_overlay", hint = "Show video overlay settings and metadata display guidance." },
      { label = "Reports Help", app_action = "help_reports", hint = "Show export and report workflow guidance." },
      { label = "Quick Actions Help", app_action = "help_quick_actions", hint = "Explain how customizable top-menu quick actions work." },
    },
  },
}

App.manager_order = { "import", "cues", "session", "reports", "overlay", "preferences", "help" }

App.overlay_rows = {
  { key = "enabled", label = "Enable video overlays" },
  { key = "show_cue_id", label = "Cue ID" },
  { key = "show_character", label = "Character name" },
  { key = "show_cue_timecode", label = "Cue timecode" },
  { key = "show_project_timer", label = "Live project timer" },
  { key = "show_dialogue", label = "Dialogue line" },
  { key = "show_direction", label = "Notes" },
  { key = "show_cue_type", label = "Cue type" },
  { key = "show_visual_cue", label = "Visual cue indicator" },
  { key = "show_streamer", label = "Streamer bar" },
  { key = "show_flash", label = "Flash at cue start" },
  { key = "show_status", label = "Standby / take / clear status" },
  { key = "show_metadata", label = "Studio metadata" },
}

App.overlay_background_rows = {
  { key = "bg_project_timer", label = "Timeline SMPTE background" },
  { key = "bg_cue_id", label = "Cue ID background" },
  { key = "bg_character", label = "Character background" },
  { key = "bg_cue_timecode", label = "Cue timecode background" },
  { key = "bg_dialogue", label = "Dialogue background" },
  { key = "bg_direction", label = "Notes background" },
  { key = "bg_cue_type", label = "Cue type background" },
  { key = "bg_status", label = "Status background" },
  { key = "bg_metadata", label = "Metadata background" },
}

App.overlay_text_color_choices = {
  { key = "white", label = "White" },
  { key = "yellow", label = "Yellow" },
}

App.metadata_field_labels = { "PGID", "MID", "Media Time", "Watermark Timestamp", "Asset Date Code", "Project Name" }

App.quick_action_choices = {
  { key = "import",           label = "Import Cue Sheet",       script = "ReaADR_Import_Cue_Sheet.lua" },
  { key = "cue_manager",      label = "Open Cue Manager",        script = "ReaADR_Cue_Manager.lua" },
  { key = "record_cue",       label = "Record Current Cue",      script = "ReaADR_Record_Cue.lua" },
  { key = "export_reports",   label = "Export Reports",          app_action = "export_reports" },
  { key = "overlay_settings", label = "Video Overlays Tab",      app_action = "open_overlay_manager" },
  { key = "character_filter", label = "Character Filter",        script = "ReaADR_Character_Filter.lua" },
  { key = "refresh_overlay",  label = "Refresh Video Overlay",   app_action = "refresh_overlay" },
  { key = "validate",         label = "Check Session",           app_action = "validate_session" },
}

App.quick_action_defaults = {
  [1] = "import",
  [2] = "cue_manager",
  [3] = "export_reports",
  [4] = "overlay_settings",
}

App.help_topics = {
  import = {
    title = "Import Cue Sheet",
    keywords = "import script csv tsv column mapping spreadsheet metadata build session",
    body = table.concat({
      "Import Cue Sheet accepts CSV, TSV/TAB, Excel .xlsx files, Google Sheets CSV/TSV exports, and plain-text delimited tables such as .txt files.",
      "",
      "Required ADR fields are cue number, character, start time, and dialogue. End time is recommended. Unknown columns are preserved as cue metadata when possible.",
      "",
      "Use the column mapping step when a studio sheet uses names like Role, In, Out, or Line instead of ReaADR's default names.",
      "",
      "For best .xlsx results, use a simple first worksheet with one header row and cue data below it.",
    }, "\n"),
  },
  cues = {
    title = "Cue Management",
    keywords = "cue manager status navigation next previous jump character filter selected cue",
    body = table.concat({
      "Cue Manager lets you browse cues, jump to cues, edit cue fields inline, refresh the session, refresh the overlay, and open the cue information panel.",
      "",
      "Double-click status or cue type cells to choose values from inline dropdowns. Other editable cells use inline text editing.",
      "",
      "Refresh Session repairs generated tracks, cue regions, cue audio, lane assignments, filters, and overlay state from the saved ReaADR session.",
      "",
      "Cue status colors are used by the overlay and generated regions. Character Filter only mutes or unmutes character tracks, so regions stay intact for navigation.",
    }, "\n"),
  },
  overlay = {
    title = "Video Overlay",
    keywords = "overlay video settings smpte timecode media time metadata dialogue streamer visual cue",
    body = table.concat({
      "Overlay Settings controls each visible video element independently.",
      "",
      "The live SMPTE project timer can remain visible outside cue regions. Cue SMPTE, Media Time, dialogue, character, cue number, status, metadata, streamers, and flash indicators can be enabled or disabled.",
    }, "\n"),
  },
  reports = {
    title = "Reports and Export",
    keywords = "export reports csv cue sheet recording report timing metadata",
    body = table.concat({
      "Export Reports can create cue sheet, recording, timing, and session metadata CSV files.",
      "",
      "Session export currently supports CSV and JSON. EDL export is available for interchange, and AAF remains a future investigation.",
      "",
      "Export Cue Sheet CSV is useful when building a project inside REAPER first, then filling in dialogue or metadata later in a spreadsheet.",
    }, "\n"),
  },
  quick_actions = {
    title = "Quick Actions",
    keywords = "quick action customize menu top menu shortcuts manager",
    body = table.concat({
      "The top-level ReaADR Tools menu keeps Open Manager first, followed by four configurable quick-action slots.",
      "",
      "Use Manager > Preferences to choose what each slot runs with the inline dropdowns.",
    }, "\n"),
  },
}

function App.run_script(script_name)
  if not script_name or script_name == "" then
    return false
  end
  local path = App.base_dir .. "/" .. script_name
  if type(reaper.AddRemoveReaScript) == "function" and type(reaper.Main_OnCommand) == "function" then
    local command_id = reaper.AddRemoveReaScript(true, 0, path, true)
    if command_id and command_id > 0 then
      reaper.Main_OnCommand(command_id, 0)
      return true
    end
  end
  dofile(path)
  return true
end

function App.run_named_action(action_name)
  for _, module_key in ipairs(App.manager_order) do
    local module = App.modules[module_key]
    for _, action in ipairs(module.actions or {}) do
      if action.label == action_name then
        return App.run_script(action.script)
      end
    end
  end
  return false
end

local function run_action(action)
  if not action then
    return false
  end
  if action.script then
    return App.run_script(action.script)
  end
  if action.app_action and App[action.app_action] then
    return App[action.app_action]()
  end
  return false
end

local function quick_action_key(slot)
  return "quick_action_" .. tostring(slot)
end

local function find_quick_action(key)
  for _, action in ipairs(App.quick_action_choices) do
    if action.key == key then
      return action
    end
  end
  return nil
end

function App.get_quick_action(slot)
  local key = reaper.GetExtState("ReaADRTools", quick_action_key(slot))
  if key == "" then
    key = App.quick_action_defaults[slot]
  end
  return find_quick_action(key) or find_quick_action(App.quick_action_defaults[slot])
end

function App.run_quick_action(slot)
  local action = App.get_quick_action(slot)
  if not action then
    App.ReaADR.message("Quick Action " .. tostring(slot) .. " has not been configured.")
    return false
  end
  return run_action(action)
end

function App.open_overlay_manager()
  return App.launch_manager("overlay")
end

local function manager_slot_key(slot, suffix)
  return ("ui.manager_slot.%d.%s"):format(slot, suffix)
end

local function set_manager_launch_tab(slot, tab)
  reaper.SetProjExtState(0, "ReaADRTools", manager_slot_key(slot, "launch_tab"), tostring(tab or ""))
end

function App.consume_manager_launch_tab(slot)
  local _, tab = reaper.GetProjExtState(0, "ReaADRTools", manager_slot_key(slot, "launch_tab"))
  reaper.SetProjExtState(0, "ReaADRTools", manager_slot_key(slot, "launch_tab"), "")
  return tab ~= "" and tab or nil
end

local function claim_manager_slot(max_slots)
  max_slots = max_slots or 3
  local now = reaper.time_precise()
  for slot = 1, max_slots do
    local _, heartbeat = reaper.GetProjExtState(0, "ReaADRTools", manager_slot_key(slot, "heartbeat"))
    local _, active = reaper.GetProjExtState(0, "ReaADRTools", manager_slot_key(slot, "active"))
    local _, launching = reaper.GetProjExtState(0, "ReaADRTools", manager_slot_key(slot, "launching"))
    local last_seen = tonumber(heartbeat) or 0
    local launch_seen = tonumber(launching) or 0
    local slot_busy = active == "1" and (now - last_seen) <= 2.0
    local slot_launching = launch_seen > 0 and (now - launch_seen) <= 2.0
    if not slot_busy and not slot_launching then
      reaper.SetProjExtState(0, "ReaADRTools", manager_slot_key(slot, "active"), "1")
      reaper.SetProjExtState(0, "ReaADRTools", manager_slot_key(slot, "heartbeat"), tostring(now))
      reaper.SetProjExtState(0, "ReaADRTools", manager_slot_key(slot, "launching"), tostring(now))
      return slot
    end
  end
  return nil
end

function App.launch_manager(initial_tab)
  App.ReaADR.show_video_window()
  local slot = claim_manager_slot(3)
  if not slot then
    App.ReaADR.message("Three ReaADR manager windows are already open. Close one before opening another.")
    return false
  end
  set_manager_launch_tab(slot, initial_tab)
  local path = App.base_dir .. "/ReaADR_Open_Manager_" .. tostring(slot) .. ".lua"
  local command_id = reaper.AddRemoveReaScript(true, 0, path, true)
  if command_id and command_id > 0 then
    reaper.Main_OnCommand(command_id, 0)
    return true
  end
  reaper.SetProjExtState(0, "ReaADRTools", manager_slot_key(slot, "active"), "")
  reaper.SetProjExtState(0, "ReaADRTools", manager_slot_key(slot, "heartbeat"), "")
  reaper.SetProjExtState(0, "ReaADRTools", manager_slot_key(slot, "launching"), "")
  App.ReaADR.message("Could not open the ReaADR manager.")
  return false
end

function App.configure_quick_actions()
  local state = {
    width = 620,
    height = 360,
    min_width = 520,
    min_height = 320,
    last_mouse = 0,
  }

  local rows = {}
  local done = {}

  local function layout()
    state.width = math.max(state.min_width, gfx.w or state.width)
    state.height = math.max(state.min_height, gfx.h or state.height)
    for slot = 1, 4 do
      rows[slot] = { x = 24, y = 82 + ((slot - 1) * 48), w = state.width - 60, h = 34, slot = slot }
    end
    done = { x = state.width - 150, y = state.height - 66, w = 114, h = 34, label = "Done" }
  end

  local function inside(rect, x, y)
    return x >= rect.x and x <= rect.x + rect.w and y >= rect.y and y <= rect.y + rect.h
  end

  local function draw_rect_button(rect, label)
    local hover = inside(rect, gfx.mouse_x, gfx.mouse_y)
    gfx.set(hover and 0.22 or 0.18, hover and 0.34 or 0.24, hover and 0.40 or 0.28, 1)
    gfx.rect(rect.x, rect.y, rect.w, rect.h, true)
    gfx.set(0.62, 0.66, 0.70, 1)
    gfx.rect(rect.x, rect.y, rect.w, rect.h, false)
    gfx.setfont(1, "Arial", 15)
    gfx.set(1, 1, 1, 1)
    gfx.x = rect.x + 10
    gfx.y = rect.y + 8
    gfx.drawstr(label)
    return hover
  end

  local function choose_slot(slot)
    local labels = {}
    local current = App.get_quick_action(slot)
    for index, action in ipairs(App.quick_action_choices) do
      labels[index] = (current and action.key == current.key and "!" or "") .. action.label
    end
    local mouse_x, mouse_y = reaper.GetMousePosition()
    gfx.x = mouse_x
    gfx.y = mouse_y
    local choice = gfx.showmenu(table.concat(labels, "|"))
    local selected = App.quick_action_choices[choice]
    if selected then
      reaper.SetExtState("ReaADRTools", quick_action_key(slot), selected.key, true)
    end
  end

  local function frame()
    layout()
    gfx.set(0.10, 0.11, 0.12, 1)
    gfx.rect(0, 0, state.width, state.height, true)
    gfx.setfont(1, "Arial", 22)
    gfx.set(1, 1, 1, 1)
    gfx.x = 24
    gfx.y = 20
    gfx.drawstr("Configure Quick Actions")
    gfx.setfont(1, "Arial", 13)
    gfx.set(0.76, 0.80, 0.84, 1)
    gfx.x = 24
    gfx.y = 50
    gfx.drawstr("Click a slot to choose the action it runs. Restart REAPER for top-menu label changes.")

    for slot, rect in ipairs(rows) do
      local action = App.get_quick_action(slot)
      draw_rect_button(rect, ("Quick Action %d: %s"):format(slot, action and action.label or "Not configured"))
    end
    draw_rect_button(done, "Done")

    gfx.update()
    local char = gfx.getchar()
    if char < 0 or char == 27 then
      gfx.quit()
      return
    elseif App.ReaADR.handle_gfx_transport_key(char, false) then
      reaper.defer(frame)
      return
    end

    local mouse = gfx.mouse_cap % 2
    if mouse == 1 and state.last_mouse == 0 then
      if inside(done, gfx.mouse_x, gfx.mouse_y) then
        gfx.quit()
        return
      end
      for slot, rect in ipairs(rows) do
        if inside(rect, gfx.mouse_x, gfx.mouse_y) then
          choose_slot(slot)
        end
      end
    end
    state.last_mouse = mouse
    reaper.defer(frame)
  end

  gfx.init("Configure ReaADR Quick Actions", state.width, state.height)
  frame()
  return true
end

local function split_metadata_fields(value)
  local fields = {}
  for field in tostring(value or ""):gmatch("([^,]+)") do
    field = field:match("^%s*(.-)%s*$")
    if field ~= "" then
      fields[#fields + 1] = field
    end
  end
  for index = 1, #App.metadata_field_labels do
    if fields[index] == nil then
      fields[index] = App.metadata_field_labels[index]
    end
  end
  return fields
end

local function edit_metadata_fields(settings)
  local fields = split_metadata_fields(settings.metadata_fields)
  local ok, values = reaper.GetUserInputs(
    "Studio Metadata Overlay Fields",
    #App.metadata_field_labels,
    table.concat(App.metadata_field_labels, ","),
    table.concat(fields, ",")
  )
  if not ok then
    return false
  end
  local updated = {}
  for value in (values .. ","):gmatch("([^,]*),") do
    value = value:match("^%s*(.-)%s*$")
    if value ~= "" then
      updated[#updated + 1] = value
    end
  end
  settings.metadata_fields = table.concat(updated, ",")
  return true
end

local function apply_overlay_profile(settings, name)
  local profiles = {
    actor = {
      enabled = true, show_cue_id = true, show_character = true, show_dialogue = true,
      show_cue_timecode = true, show_project_timer = true, show_visual_cue = true,
      show_direction = true, show_cue_type = false, show_streamer = true,
      show_flash = true, show_status = true, show_metadata = false,
      bg_project_timer = false, bg_cue_id = false, bg_character = false,
      bg_cue_timecode = false, bg_dialogue = true, bg_direction = false,
      bg_cue_type = false, bg_status = false, bg_metadata = false,
    },
    engineer = {
      enabled = true, show_cue_id = true, show_character = true, show_dialogue = true,
      show_cue_timecode = true, show_project_timer = true, show_visual_cue = true,
      show_direction = true, show_cue_type = true, show_streamer = true,
      show_flash = true, show_status = true, show_metadata = true,
      bg_project_timer = true, bg_cue_id = false, bg_character = false,
      bg_cue_timecode = true, bg_dialogue = true, bg_direction = false,
      bg_cue_type = false, bg_status = false, bg_metadata = false,
    },
    studio = {
      enabled = true, show_cue_id = true, show_character = true, show_dialogue = true,
      show_cue_timecode = true, show_project_timer = true, show_visual_cue = true,
      show_direction = false, show_cue_type = true, show_streamer = true,
      show_flash = true, show_status = true, show_metadata = true,
      bg_project_timer = true, bg_cue_id = false, bg_character = false,
      bg_cue_timecode = true, bg_dialogue = true, bg_direction = false,
      bg_cue_type = false, bg_status = false, bg_metadata = false,
    },
    minimal = {
      enabled = true, show_cue_id = true, show_character = false, show_dialogue = true,
      show_cue_timecode = false, show_project_timer = true, show_visual_cue = true,
      show_direction = false, show_cue_type = false, show_streamer = true,
      show_flash = false, show_status = false, show_metadata = false,
      bg_project_timer = false, bg_cue_id = false, bg_character = false,
      bg_cue_timecode = false, bg_dialogue = true, bg_direction = false,
      bg_cue_type = false, bg_status = false, bg_metadata = false,
    },
  }
  for key, value in pairs(profiles[name] or {}) do
    settings[key] = value
  end
end

local function save_overlay_settings(settings)
  App.ReaADR.save_overlay_settings(settings)
  return App.ReaADR.refresh_overlay_fx_from_project(settings)
end

local function choose_quick_action(slot)
  local labels = {}
  local current = App.get_quick_action(slot)
  for index, action in ipairs(App.quick_action_choices) do
    labels[index] = (current and action.key == current.key and "!" or "") .. action.label
  end
  local mouse_x, mouse_y = reaper.GetMousePosition()
  gfx.x = mouse_x
  gfx.y = mouse_y
  local choice = gfx.showmenu(table.concat(labels, "|"))
  local selected = App.quick_action_choices[choice]
  if selected then
    reaper.SetExtState("ReaADRTools", quick_action_key(slot), selected.key, true)
    return true
  end
  return false
end

local function show_help_topic(topic)
  if not topic then
    return false
  end
  App.ReaADR.message(topic.title .. "\n\n" .. topic.body)
  return true
end

function App.help_import()
  return show_help_topic(App.help_topics.import)
end

function App.help_cues()
  return show_help_topic(App.help_topics.cues)
end

function App.help_overlay()
  return show_help_topic(App.help_topics.overlay)
end

function App.help_reports()
  return show_help_topic(App.help_topics.reports)
end

function App.help_quick_actions()
  return show_help_topic(App.help_topics.quick_actions)
end

function App.search_help()
  local ok, query = reaper.GetUserInputs("Search ReaADR Help", 1, "Search:", "")
  if not ok then
    return false
  end
  query = tostring(query or ""):lower():match("^%s*(.-)%s*$")
  if query == "" then
    local labels = {}
    local topics = {}
    for _, key in ipairs({ "import", "cues", "overlay", "reports", "quick_actions" }) do
      topics[#topics + 1] = App.help_topics[key]
      labels[#labels + 1] = App.help_topics[key].title
    end
    gfx.init("ReaADR Help", 0, 0, 0)
    local choice = gfx.showmenu(table.concat(labels, "|"))
    gfx.quit()
    return show_help_topic(topics[choice])
  end

  local matches = {}
  local output = {}
  for _, topic in pairs(App.help_topics) do
    local haystack = (topic.title .. " " .. topic.keywords .. " " .. topic.body):lower()
    if haystack:find(query, 1, true) then
      matches[#matches + 1] = topic
      output[#output + 1] = topic.title .. "\n" .. topic.body
    end
  end
  if #matches == 0 then
    App.ReaADR.message("No help topics matched: " .. query)
    return false
  end
  App.ReaADR.message("Help results for: " .. query .. "\n\n" .. table.concat(output, "\n\n---\n\n"))
  return true
end

function App.import_script()
  return App.run_named_action("Import Cue Sheet")
end

function App.preferences()
  return App.launch_manager("preferences")
end

function App.export_reports()
  local ReaADR = App.ReaADR
  local cues = ReaADR.load_session_cues()
  if not cues then
    cues = ReaADR.navigation_cues()
  end
  cues = cues or {}
  if #cues == 0 then
    ReaADR.message("No cue data was found to export.")
    return false
  end

  local reports = {
    { label = "Cue Sheet CSV",        suffix = "cue_sheet",         export = ReaADR.export_cues_to_csv },
    { label = "Recording Report CSV", suffix = "recording_report",  export = ReaADR.export_recording_report },
    { label = "Timing Report CSV",    suffix = "timing_report",     export = ReaADR.export_timing_report },
    { label = "Session Metadata CSV", suffix = "session_metadata",  export = ReaADR.export_session_metadata_report },
    { label = "Full Session JSON",    suffix = "session",           export = ReaADR.export_session_json,    ext = "json" },
    { label = "EDL (CMX 3600)",       suffix = "session",           export = ReaADR.export_cues_to_edl,     ext = "edl" },
  }

  local labels = {}
  for index, report in ipairs(reports) do
    labels[index] = report.label
  end

  gfx.init("ReaADR Reports", 0, 0, 0)
  local choice = gfx.showmenu(table.concat(labels, "|"))
  gfx.quit()

  local report = reports[choice]
  if not report then
    return false
  end

  local project_path = reaper.GetProjectPath("")
  if not project_path or project_path == "" then
    project_path = "."
  end
  local default_path = project_path .. "/reaadr_" .. report.suffix .. "." .. (report.ext or "csv")
  local ok, value = reaper.GetUserInputs("Export " .. report.label, 1, "Output path:", default_path)
  if not ok or value == "" then
    return false
  end

  local success, err = report.export(cues, value)
  if not success then
    ReaADR.message("Report export failed:\n\n" .. tostring(err))
    return false
  end

  ReaADR.message(("Exported %s:\n\n%s"):format(report.label, value))
  return true
end

function App.toggle_qa_mode()
  local ReaADR = App.ReaADR
  local currently_on = ReaADR.qa_mode_enabled()
  ReaADR.set_qa_mode(not currently_on)
  ReaADR.message(
    ("QA mode %s.\n\n%s"):format(
      not currently_on and "enabled" or "disabled",
      not currently_on
        and "Verbose logging is now active. Import, refresh, delete, and error events will be written to the REAPER console."
        or  "Logging is now limited to warnings and errors only."
    )
  )
  return true
end

local function refresh_session_workflow(ReaADR, confirm_message)
  local cues, err = ReaADR.load_session_cues()
  if not cues then
    ReaADR.message("No ReaADR session was found:\n\n" .. tostring(err))
    return false
  end
  local drift = ReaADR.detect_session_drift and ReaADR.detect_session_drift({ cues = cues })
  if drift and (drift.modified_regions or 0) > 0 and not confirm_message then
    local answer = reaper.ShowMessageBox(
      ("Detected %d cue region(s) whose timing differs from the saved session.\n\nRefresh Session will restore the saved cue timing and overwrite those moved regions.\n\nChoose No, then use Update Cues From Regions if the moved regions are the timing you want to keep.\n\nContinue with Refresh Session?"):format(drift.modified_regions or 0),
      "ReaADR Refresh Session",
      4
    )
    if answer ~= 6 then
      return false
    end
  end
  if confirm_message then
    local answer = reaper.ShowMessageBox(confirm_message, "ReaADR", 4)
    if answer ~= 6 then
      return false
    end
  end

  local progress = ReaADR.create_progress_window("Refreshing ReaADR Session")
  local summary, setup_error = ReaADR.resolve_session_drift("refresh", {
    on_progress = progress.update,
  })
  progress.close()
  if not summary then
    ReaADR.message("Session refresh failed:\n\n" .. tostring(setup_error))
    return false
  end

  local rebuild = summary.rebuild or {}
  ReaADR.message(("Session refreshed.\n\nCues: %d\nTracks: %d created, %d reused\nRegions: %d created, %d updated\nCue audio: %d created, %d updated, %d skipped\nOverlay: %s"):format(
    rebuild.cue_count or 0,
    rebuild.tracks_created or 0,
    rebuild.tracks_reused or 0,
    rebuild.regions_created or 0,
    rebuild.regions_updated or 0,
    rebuild.cue_audio_created or 0,
    rebuild.cue_audio_updated or 0,
    rebuild.cue_audio_skipped or 0,
    rebuild.overlay_fx_status or "not_configured"
  ))
  return true
end

function App.update_cues_from_regions()
  local ReaADR = App.ReaADR
  local progress = ReaADR.create_progress_window("Updating Cues From Regions")
  local summary, err = ReaADR.update_session_cues_from_regions({
    on_progress = progress.update,
    source = "manager_update_cues_from_regions",
  })
  progress.close()
  if not summary then
    ReaADR.message("Could not update cues from regions:\n\n" .. tostring(err))
    return false
  end
  ReaADR.message(
    ("Updated %d cue(s) from current region timing.\n\nMissing ReaADR regions: %d"):format(
      summary.changed_cues or 0,
      summary.missing_regions or 0
    )
  )
  return true
end

function App.validate_session()
  local ReaADR = App.ReaADR
  local cues = ReaADR.load_session_cues()
  if not cues then
    cues = ReaADR.session_cues()
  end
  local summary, err = ReaADR.sync_validate({ cues = cues or {} }, {
    preroll_seconds = ReaADR.load_overlay_settings().preroll_seconds,
    include_drift = true,
  })
  if not summary then
    ReaADR.message("Session check failed:\n\n" .. tostring(err))
    return false
  end

  local text = ReaADR.validation_summary_text(summary.validation):gsub("\nBuild this ADR session%?$", "")
  local drift = summary.drift
  if drift then
    local drift_lines = {
      "",
      "Generated session items:",
      "Missing tracks: " .. tostring(drift.missing_tracks or 0),
      "Missing cue regions: " .. tostring(drift.missing_regions or 0),
      "Changed cue regions: " .. tostring(drift.modified_regions or 0),
      "Missing cue audio: " .. tostring(drift.missing_cue_audio or 0),
      "Extra cue audio: " .. tostring(drift.unexpected_cue_audio or 0),
    }
    if drift.has_drift then
      drift_lines[#drift_lines + 1] = ""
      drift_lines[#drift_lines + 1] = "Refresh Session can repair these generated items."
    end
    text = text .. "\n" .. table.concat(drift_lines, "\n")
  end
  if drift and drift.has_drift then
    local answer = reaper.ShowMessageBox(text .. "\n\nRefresh the session now?", "ReaADR Session Check", 4)
    if answer == 6 then
      return refresh_session_workflow(ReaADR, nil)
    end
  else
    ReaADR.message(text)
  end
  return true
end

function App.refresh_overlay()
  local ReaADR = App.ReaADR
  local status, err = ReaADR.refresh_overlay_fx_from_project()
  if status then
    ReaADR.message("Video overlay refreshed: " .. tostring(status))
  else
    ReaADR.message("Video overlay refresh failed:\n\n" .. tostring(err))
  end
  return status ~= nil
end

function App.rebuild_session()
  local ReaADR = App.ReaADR
  local cues = ReaADR.load_session_cues()
  local drift = cues and ReaADR.detect_session_drift and ReaADR.detect_session_drift({ cues = cues })
  if drift and (drift.modified_regions or 0) > 0 then
    local answer = reaper.ShowMessageBox(
      ("Detected %d cue region(s) whose timing differs from the saved session.\n\nYes: refresh from saved cues and overwrite moved regions.\nNo: cancel so you can use Update Cues From Regions instead."):format(drift.modified_regions or 0),
      "ReaADR Refresh Session",
      4
    )
    if answer ~= 6 then
      return false
    end
  end
  return refresh_session_workflow(
    ReaADR,
    "Refresh generated tracks, cue regions, cue audio, and video overlays from the saved ReaADR session?"
  )
end

local function session_summary(ReaADR)
  local cues = ReaADR.load_session_cues()
  if not cues then
    cues = ReaADR.navigation_cues()
  end
  cues = cues or {}

  local characters = ReaADR.collect_characters(cues)
  return {
    cue_count = #cues,
    character_count = #characters,
    characters = characters,
  }
end

local function inside(rect, x, y)
  return x >= rect.x and x <= rect.x + rect.w and y >= rect.y and y <= rect.y + rect.h
end

local function draw_button(rect)
  local ReaADR = App.ReaADR
  local theme = ReaADR.ui_theme()
  local hover = inside(rect, gfx.mouse_x, gfx.mouse_y)
  ReaADR.set_gfx_color(hover and theme.accent_blue or theme.panel_alt)
  gfx.rect(rect.x, rect.y, rect.w, rect.h, true)
  ReaADR.set_gfx_color(theme.border)
  gfx.rect(rect.x, rect.y, rect.w, rect.h, false)
  gfx.setfont(1, "Arial", 15)
  ReaADR.set_gfx_color(theme.text)
  gfx.x = rect.x + 12
  gfx.y = rect.y + 8
  gfx.drawstr(rect.label)
  return hover
end

local function trim_to_width(text, max_w, font_size)
  text = tostring(text or "")
  gfx.setfont(1, "Arial", font_size or 14)
  if gfx.measurestr(text) <= max_w then
    return text
  end
  local trimmed = text
  while #trimmed > 1 and gfx.measurestr(trimmed .. "...") > max_w do
    trimmed = trimmed:sub(1, #trimmed - 1)
  end
  return trimmed .. "..."
end

local function layout_wrapped_buttons(start_x, start_y, available_w, specs, options)
  options = options or {}
  local gap_x = options.gap_x or 10
  local gap_y = options.gap_y or 10
  local height = options.height or 32
  local font_size = options.font_size or 15
  local buttons = {}
  local x = start_x
  local y = start_y
  local row_max_h = height

  gfx.setfont(1, "Arial", font_size)
  for _, spec in ipairs(specs or {}) do
    local w = math.max(spec.min_w or 96, math.ceil(gfx.measurestr(spec.label or "") + 24))
    if x > start_x and (x + w) > (start_x + available_w) then
      x = start_x
      y = y + row_max_h + gap_y
    end
    buttons[#buttons + 1] = {
      x = x,
      y = y,
      w = w,
      h = height,
      label = spec.label,
      profile = spec.profile,
    }
    x = x + w + gap_x
  end

  local next_y = y + row_max_h
  return buttons, next_y
end

local function layout_checkbox_grid(start_x, start_y, available_w, rows, options)
  options = options or {}
  local min_col_w = options.min_col_w or 320
  local gap_x = options.gap_x or 18
  local row_h = options.row_h or 32
  local cols = math.max(1, math.floor((available_w + gap_x) / (min_col_w + gap_x)))
  local col_w = math.floor((available_w - ((cols - 1) * gap_x)) / cols)
  local entries = {}

  for index, row in ipairs(rows or {}) do
    local col = (index - 1) % cols
    local row_index = math.floor((index - 1) / cols)
    local x = start_x + (col * (col_w + gap_x))
    local y = start_y + (row_index * row_h)
    entries[#entries + 1] = {
      row = row,
      rect = { x = x, y = y - 4, w = col_w, h = 26 },
      box_x = x,
      box_y = y,
      text_x = x + 30,
      text_y = y - 1,
      text_w = col_w - 34,
    }
  end

  local rows_used = math.max(1, math.ceil(#(rows or {}) / cols))
  local next_y = start_y + (rows_used * row_h)
  return entries, next_y
end

function App.open_manager(initial_tab, instance_slot)
  local ReaADR = App.ReaADR
  ReaADR.show_video_window()
  local summary = session_summary(ReaADR)
  local state = {
    width = 1040,
    height = 880,
    min_width = 1040,
    min_height = 880,
    tab = initial_tab or "import",
    last_mouse = 0,
    closed = false,
    overlay_settings = ReaADR.load_overlay_settings(),
    overlay_dirty = false,
    overlay_message = "",
    overlay_message_until = 0,
    overlay_scroll = 0,
    overlay_content_h = 0,
    quick_action_dropdown = nil,
    last_heartbeat = 0,
    instance_slot = tonumber(instance_slot),
    dragging_overlay_scrollbar = false,
    overlay_scrollbar_drag_offset = 0,
  }

  local tab_rects = {}
  local tab_x = 24
  for _, key in ipairs(App.manager_order) do
    local module = App.modules[key]
    local w = math.max(104, (#module.title * 8) + 26)
    tab_rects[#tab_rects + 1] = { key = key, x = tab_x, y = 110, w = w, h = 30, label = module.title }
    tab_x = tab_x + w + 8
  end

  local function close()
    if state.closed then
      return
    end
    state.closed = true
    if state.instance_slot then
      reaper.SetProjExtState(0, "ReaADRTools", manager_slot_key(state.instance_slot, "active"), "")
      reaper.SetProjExtState(0, "ReaADRTools", manager_slot_key(state.instance_slot, "heartbeat"), "")
      reaper.SetProjExtState(0, "ReaADRTools", manager_slot_key(state.instance_slot, "launching"), "")
      reaper.SetProjExtState(0, "ReaADRTools", manager_slot_key(state.instance_slot, "launch_tab"), "")
    end
    gfx.quit()
  end

  local function frame()
    if (gfx.w or state.width) < state.min_width or (gfx.h or state.height) < state.min_height then
      local dock, x, y = 0, nil, nil
      local ok, current_dock, current_x, current_y = pcall(gfx.dock, -1, 0, 0, 0, 0)
      if ok then
        dock = tonumber(current_dock) or 0
        x = tonumber(current_x)
        y = tonumber(current_y)
      end
      if x ~= nil and y ~= nil then
        gfx.init("ReaADR Tools Manager", state.min_width, state.min_height, dock, x, y)
      else
        gfx.init("ReaADR Tools Manager", state.min_width, state.min_height, dock)
      end
    end
    state.width = math.max(state.min_width, gfx.w or state.width)
    state.height = math.max(state.min_height, gfx.h or state.height)
    if state.instance_slot and (reaper.time_precise() - state.last_heartbeat) >= 0.5 then
      state.last_heartbeat = reaper.time_precise()
      reaper.SetProjExtState(0, "ReaADRTools", manager_slot_key(state.instance_slot, "active"), "1")
      reaper.SetProjExtState(0, "ReaADRTools", manager_slot_key(state.instance_slot, "heartbeat"), tostring(state.last_heartbeat))
      reaper.SetProjExtState(0, "ReaADRTools", manager_slot_key(state.instance_slot, "launching"), "")
    end
    local hover_hint = ""
    local special_click = {}
    local theme = ReaADR.ui_theme()
    ReaADR.set_gfx_color(theme.bg)
    gfx.rect(0, 0, state.width, state.height, true)

    local header = ReaADR.draw_window_header(
      "ReaADR Tools Manager",
      ("Session: %d cues, %d characters"):format(summary.cue_count, summary.character_count),
      { x = 24, y = 20, width = state.width - 48, height = 78 }
    )

    for _, tab in ipairs(tab_rects) do
      local active = tab.key == state.tab
      local hovered = inside(tab, gfx.mouse_x, gfx.mouse_y)
      ReaADR.set_gfx_color(active and theme.accent_blue or theme.panel_alt)
      gfx.rect(tab.x, tab.y, tab.w, tab.h, true)
      ReaADR.set_gfx_color(theme.border)
      gfx.rect(tab.x, tab.y, tab.w, tab.h, false)
      gfx.setfont(1, "Arial", 14)
      ReaADR.set_gfx_color(theme.text)
      gfx.x = tab.x + 12
      gfx.y = tab.y + 8
      gfx.drawstr(tab.label)
      if hovered then
        hover_hint = "Open " .. tab.label .. " tools."
      end
    end

    local module = App.modules[state.tab]
    local buttons = {}
    local overlay_scrollbar_rect = nil
    local overlay_scrollbar_thumb = nil
    local overlay_max_scroll = 0
    local overlay_viewport_top = 0
    local overlay_viewport_bottom = 0
    local y = math.max(156, header.content_y + 6)
    gfx.setfont(1, "Arial", 18)
    ReaADR.set_gfx_color(theme.text)
    gfx.x = 24
    gfx.y = y
    gfx.drawstr(module.title)
    y = y + 38

    if state.tab ~= "overlay" then
      for index, action in ipairs(module.actions or {}) do
        local rect = { x = 24, y = y + ((index - 1) * 44), w = 320, h = 34, label = action.label, action = action }
        buttons[#buttons + 1] = rect
        if draw_button(rect) and action.hint then
          hover_hint = action.hint
        end
      end
    end

    if state.tab == "cues" then
      gfx.setfont(1, "Arial", 14)
      ReaADR.set_gfx_color(theme.muted)
      gfx.x = 382
      gfx.y = 166
      gfx.drawstr("Project Cue Summary")
      gfx.x = 382
      gfx.y = 190
      gfx.drawstr(("Cues: %d"):format(summary.cue_count))
      gfx.x = 382
      gfx.y = 214
      gfx.drawstr(("Characters: %d"):format(summary.character_count))
      if #summary.characters > 0 then
        gfx.x = 382
        gfx.y = 238
        local characters = table.concat(summary.characters, ", ")
        if #characters > 78 then
          characters = characters:sub(1, 75) .. "..."
        end
        gfx.drawstr(characters)
      end
    end

    local quick_y = y
    if state.tab == "preferences" then
      gfx.setfont(1, "Arial", 14)
      ReaADR.set_gfx_color(theme.muted)
      gfx.x = 24
      gfx.y = quick_y
      gfx.drawstr("Quick Access Menu")
      for slot = 1, 4 do
        local action = App.get_quick_action(slot)
        local rect = { x = 24, y = quick_y + 24 + ((slot - 1) * 44), w = 420, h = 34, label = ("Quick Action %d: %s"):format(slot, action and action.label or "Not configured") }
        special_click[#special_click + 1] = { rect = rect, kind = "quick", slot = slot }
        if draw_button(rect) then
          hover_hint = "Choose which action this top-menu quick slot runs."
        end
        if state.quick_action_dropdown == slot then
          for index, choice in ipairs(App.quick_action_choices) do
            local option_rect = {
              x = rect.x + rect.w + 12,
              y = rect.y + ((index - 1) * 30),
              w = 240,
              h = 26,
              label = choice.label,
            }
            special_click[#special_click + 1] = { rect = option_rect, kind = "quick_option", slot = slot, action_key = choice.key }
            draw_button(option_rect)
          end
        end
      end
      gfx.setfont(1, "Arial", 13)
      ReaADR.set_gfx_color(theme.muted)
      gfx.x = 24
      gfx.y = quick_y + 214
      gfx.drawstr("Quick-action labels update when REAPER rebuilds the native ReaADR menu.")
      local remember_layout = ReaADR.window_layout_enabled()
      local remember_rect = { x = 24, y = quick_y + 258, w = 340, h = 26 }
      local hover_preview = ReaADR.cue_hover_preview_enabled()
      local hover_preview_rect = { x = 24, y = quick_y + 294, w = 340, h = 26 }
      special_click[#special_click + 1] = { rect = remember_rect, kind = "remember_layout" }
      special_click[#special_click + 1] = { rect = hover_preview_rect, kind = "hover_preview" }
      ReaADR.set_gfx_color(theme.panel_alt)
      gfx.rect(remember_rect.x, remember_rect.y, 18, 18, false)
      if remember_layout then
        ReaADR.set_gfx_color(theme.accent_gold)
        gfx.rect(remember_rect.x + 4, remember_rect.y + 4, 10, 10, true)
      end
      gfx.setfont(1, "Arial", 14)
      ReaADR.set_gfx_color(theme.text)
      gfx.x = remember_rect.x + 30
      gfx.y = remember_rect.y - 1
      gfx.drawstr("Remember ReaADR window layout per project")
      ReaADR.set_gfx_color(theme.panel_alt)
      gfx.rect(hover_preview_rect.x, hover_preview_rect.y, 18, 18, false)
      if hover_preview then
        ReaADR.set_gfx_color(theme.accent_gold)
        gfx.rect(hover_preview_rect.x + 4, hover_preview_rect.y + 4, 10, 10, true)
      end
      gfx.setfont(1, "Arial", 14)
      ReaADR.set_gfx_color(theme.text)
      gfx.x = hover_preview_rect.x + 30
      gfx.y = hover_preview_rect.y - 1
      gfx.drawstr("Show cue text preview on hover")
    elseif state.tab == "overlay" then
      local settings = state.overlay_settings
      local left = 24
      local content_w = state.width - 70
      local section_y_base = y
      overlay_viewport_top = section_y_base
      local overlay_viewport_h = math.max(140, state.height - section_y_base - 86)
      overlay_viewport_bottom = overlay_viewport_top + overlay_viewport_h
      local section_y = section_y_base - (state.overlay_scroll or 0)

      local function overlay_visible(rect)
        return rect and rect.y + rect.h >= overlay_viewport_top and rect.y <= overlay_viewport_bottom
      end

      local function add_overlay_click(rect, kind, extra)
        if overlay_visible(rect) then
          local entry = { rect = rect, kind = kind }
          for key, value in pairs(extra or {}) do
            entry[key] = value
          end
          special_click[#special_click + 1] = entry
        end
      end

      local function draw_overlay_button(rect)
        if overlay_visible(rect) then
          return draw_button(rect)
        end
        return false
      end

      local function draw_overlay_text(text, x, y_pos, font_size, color)
        if y_pos >= overlay_viewport_top - 24 and y_pos <= overlay_viewport_bottom then
          gfx.setfont(1, "Arial", font_size)
          ReaADR.set_gfx_color(color)
          gfx.x = x
          gfx.y = y_pos
          gfx.drawstr(text)
        end
      end

      local refresh_rect = { x = left, y = section_y, w = 188, h = 34, label = "Refresh Video Overlay" }
      add_overlay_click(refresh_rect, "overlay_refresh")
      if draw_overlay_button(refresh_rect) then
        hover_hint = "Rebuild the video overlay FX from the current session cue data."
      end

      draw_overlay_text("Profiles", left, section_y + 52, 14, theme.muted)

      local profile_buttons, after_profiles_y = layout_wrapped_buttons(left, section_y + 76, content_w, {
        { label = "Actor", profile = "actor", min_w = 92 },
        { label = "Engineer", profile = "engineer", min_w = 102 },
        { label = "Studio", profile = "studio", min_w = 92 },
        { label = "Minimal", profile = "minimal", min_w = 92 },
      }, { gap_x = 10, gap_y = 10, height = 30, font_size = 15 })
      for _, rect in ipairs(profile_buttons) do
        add_overlay_click(rect, "profile", { profile = rect.profile })
        if draw_overlay_button(rect) then
          hover_hint = "Apply a common overlay visibility preset."
        end
      end

      local overlay_label_y = after_profiles_y + 22
      draw_overlay_text("Overlay Elements", left, overlay_label_y, 14, theme.muted)

      local overlay_entries, after_overlay_y = layout_checkbox_grid(left, overlay_label_y + 24, content_w, App.overlay_rows, {
        min_col_w = 320,
        gap_x = 18,
        row_h = 32,
      })
      for _, entry in ipairs(overlay_entries) do
        add_overlay_click(entry.rect, "overlay_toggle", { key = entry.row.key })
        if overlay_visible(entry.rect) then
          ReaADR.set_gfx_color(theme.panel_alt)
          gfx.rect(entry.box_x, entry.box_y, 18, 18, false)
          if settings[entry.row.key] then
            ReaADR.set_gfx_color(theme.accent_gold)
            gfx.rect(entry.box_x + 4, entry.box_y + 4, 10, 10, true)
          end
          gfx.setfont(1, "Arial", 14)
          ReaADR.set_gfx_color(theme.text)
          gfx.x = entry.text_x
          gfx.y = entry.text_y
          gfx.drawstr(trim_to_width(entry.row.label, entry.text_w, 14))
        end
      end

      local backgrounds_label_y = after_overlay_y + 18
      draw_overlay_text("Text Backgrounds", left, backgrounds_label_y, 14, theme.muted)

      local bg_entries, after_backgrounds_y = layout_checkbox_grid(left, backgrounds_label_y + 24, content_w, App.overlay_background_rows, {
        min_col_w = 320,
        gap_x = 18,
        row_h = 32,
      })
      for _, entry in ipairs(bg_entries) do
        add_overlay_click(entry.rect, "overlay_toggle", { key = entry.row.key })
        if overlay_visible(entry.rect) then
          ReaADR.set_gfx_color(theme.panel_alt)
          gfx.rect(entry.box_x, entry.box_y, 18, 18, false)
          if settings[entry.row.key] then
            ReaADR.set_gfx_color(theme.accent_gold)
            gfx.rect(entry.box_x + 4, entry.box_y + 4, 10, 10, true)
          end
          gfx.setfont(1, "Arial", 14)
          ReaADR.set_gfx_color(theme.text)
          gfx.x = entry.text_x
          gfx.y = entry.text_y
          gfx.drawstr(trim_to_width(entry.row.label, entry.text_w, 14))
        end
      end

      local controls_y = after_backgrounds_y + 22
      local metadata_rect = { x = left, y = controls_y + 24, w = 132, h = 32, label = "Edit Fields" }
      local color_label_y = controls_y + 82
      local white_rect = { x = left, y = color_label_y + 24, w = 220, h = 26, label = "White general text" }
      local yellow_rect = { x = left, y = color_label_y + 58, w = 220, h = 26, label = "Yellow general text" }
      local save_rect = { x = left, y = color_label_y + 102, w = 132, h = 34, label = "Save Overlay" }
      add_overlay_click(metadata_rect, "metadata")
      add_overlay_click(white_rect, "text_color", { value = "white" })
      add_overlay_click(yellow_rect, "text_color", { value = "yellow" })
      add_overlay_click(save_rect, "overlay_save")
      draw_overlay_button(metadata_rect)
      draw_overlay_button(save_rect)
      draw_overlay_text("Metadata fields", left, controls_y, 13, theme.muted)
      local fields = tostring(settings.metadata_fields or "")
      draw_overlay_text("Current: " .. trim_to_width(fields, math.max(180, content_w - 90), 13), left, metadata_rect.y + 42, 13, theme.muted)
      draw_overlay_text("General overlay text color", left, color_label_y, 13, theme.muted)
      for _, option in ipairs({
        { rect = white_rect, value = "white", label = "White general text" },
        { rect = yellow_rect, value = "yellow", label = "Yellow general text" },
      }) do
        if overlay_visible(option.rect) then
          local selected_color = ReaADR.overlay_text_mode(settings) == option.value
          ReaADR.set_gfx_color(theme.panel_alt)
          gfx.circle(option.rect.x + 10, option.rect.y + 13, 8, false, true)
          if selected_color then
            ReaADR.set_gfx_color(theme.accent_gold)
            gfx.circle(option.rect.x + 10, option.rect.y + 13, 4, true, true)
          end
          gfx.setfont(1, "Arial", 14)
          ReaADR.set_gfx_color(theme.text)
          gfx.x = option.rect.x + 24
          gfx.y = option.rect.y + 4
          gfx.drawstr(option.label)
        end
      end
      if overlay_visible(save_rect) then
        gfx.setfont(1, "Arial", 14)
        gfx.x = save_rect.x + save_rect.w + 16
        gfx.y = save_rect.y + 9
        if state.overlay_dirty then
          ReaADR.set_gfx_color(theme.accent_gold)
          gfx.drawstr("Unsaved overlay changes")
        elseif reaper.time_precise() < state.overlay_message_until then
          ReaADR.set_gfx_color(theme.accent_green)
          gfx.drawstr(state.overlay_message)
        end
      end

      state.overlay_content_h = math.max(0, (save_rect.y + save_rect.h + 22 + (state.overlay_scroll or 0)) - section_y_base)
      overlay_max_scroll = math.max(0, state.overlay_content_h - overlay_viewport_h)
      state.overlay_scroll = math.max(0, math.min(state.overlay_scroll or 0, overlay_max_scroll))
      if overlay_max_scroll > 0 then
        local bar_x = state.width - 22
        local bar_y = section_y_base
        local bar_h = overlay_viewport_h
        local thumb_h = math.max(32, bar_h * (bar_h / math.max(bar_h, state.overlay_content_h)))
        local thumb_y = bar_y + ((bar_h - thumb_h) * ((state.overlay_scroll or 0) / overlay_max_scroll))
        overlay_scrollbar_rect = { x = bar_x - 3, y = bar_y, w = 14, h = bar_h }
        overlay_scrollbar_thumb = { x = bar_x - 1, y = thumb_y, w = 10, h = thumb_h }
        ReaADR.set_gfx_color(theme.panel)
        gfx.rect(bar_x, bar_y, 8, bar_h, true)
        ReaADR.set_gfx_color(theme.border)
        gfx.rect(bar_x, bar_y, 8, bar_h, false)
        ReaADR.set_gfx_color(theme.accent_gold)
        gfx.rect(bar_x + 1, thumb_y, 6, thumb_h, true)
      end
    elseif state.tab == "help" then
      gfx.setfont(1, "Arial", 14)
      ReaADR.set_gfx_color(theme.muted)
      gfx.x = 382
      gfx.y = quick_y
      gfx.drawstr("Search by action, workflow, or keyword.")
      gfx.x = 382
      gfx.y = quick_y + 26
      gfx.drawstr("Examples: import, overlay, SMPTE, filter, reports")
    end

    if hover_hint ~= "" then
      ReaADR.set_gfx_color(theme.panel)
      gfx.rect(18, state.height - 54, state.width - 36, 34, true)
      ReaADR.set_gfx_color(theme.border)
      gfx.rect(18, state.height - 54, state.width - 36, 34, false)
      gfx.setfont(1, "Arial", 13)
      ReaADR.set_gfx_color(theme.text)
      gfx.x = 28
      gfx.y = state.height - 44
      local hint = hover_hint
      if #hint > 116 then
        hint = hint:sub(1, 113) .. "..."
      end
      gfx.drawstr(hint)
    end

    gfx.update()

    local char = gfx.getchar()
    if char < 0 or char == 27 then
      close()
      return
    elseif ReaADR.handle_gfx_transport_key(char, false) then
      reaper.defer(frame)
      return
    end

    if state.tab == "overlay" and gfx.mouse_wheel ~= 0 then
      state.overlay_scroll = math.max(0, math.min(overlay_max_scroll, (state.overlay_scroll or 0) - (gfx.mouse_wheel > 0 and 28 or -28)))
      gfx.mouse_wheel = 0
    end

    local mouse = gfx.mouse_cap % 2
    if state.dragging_overlay_scrollbar and mouse == 1 and overlay_scrollbar_rect and overlay_scrollbar_thumb then
      local travel = math.max(1, overlay_scrollbar_rect.h - overlay_scrollbar_thumb.h)
      local thumb_y = math.max(
        overlay_scrollbar_rect.y,
        math.min(gfx.mouse_y - (state.overlay_scrollbar_drag_offset or 0), overlay_scrollbar_rect.y + travel)
      )
      local ratio = (thumb_y - overlay_scrollbar_rect.y) / travel
      state.overlay_scroll = math.max(0, math.min(overlay_max_scroll, math.floor((ratio * overlay_max_scroll) + 0.5)))
    elseif state.dragging_overlay_scrollbar and mouse == 0 then
      state.dragging_overlay_scrollbar = false
      state.overlay_scrollbar_drag_offset = 0
    end

    if mouse == 1 and state.last_mouse == 0 then
      for _, tab in ipairs(tab_rects) do
        if inside(tab, gfx.mouse_x, gfx.mouse_y) then
          state.tab = tab.key
        end
      end
      if state.tab == "overlay" and overlay_scrollbar_thumb and inside(overlay_scrollbar_thumb, gfx.mouse_x, gfx.mouse_y) then
        state.dragging_overlay_scrollbar = true
        state.overlay_scrollbar_drag_offset = gfx.mouse_y - overlay_scrollbar_thumb.y
        state.last_mouse = mouse
        reaper.defer(frame)
        return
      elseif state.tab == "overlay" and overlay_scrollbar_rect and inside(overlay_scrollbar_rect, gfx.mouse_x, gfx.mouse_y) then
        local travel = math.max(1, overlay_scrollbar_rect.h - overlay_scrollbar_thumb.h)
        local thumb_y = math.max(
          overlay_scrollbar_rect.y,
          math.min(gfx.mouse_y - math.floor(overlay_scrollbar_thumb.h * 0.5), overlay_scrollbar_rect.y + travel)
        )
        local ratio = (thumb_y - overlay_scrollbar_rect.y) / travel
        state.overlay_scroll = math.max(0, math.min(overlay_max_scroll, math.floor((ratio * overlay_max_scroll) + 0.5)))
        state.last_mouse = mouse
        reaper.defer(frame)
        return
      end
      for _, button in ipairs(buttons) do
        if inside(button, gfx.mouse_x, gfx.mouse_y) then
          run_action(button.action)
          summary = session_summary(ReaADR)
        end
      end
      for _, entry in ipairs(special_click) do
        if inside(entry.rect, gfx.mouse_x, gfx.mouse_y) then
          if entry.kind == "quick" then
            state.quick_action_dropdown = state.quick_action_dropdown == entry.slot and nil or entry.slot
          elseif entry.kind == "quick_option" then
            reaper.SetExtState("ReaADRTools", quick_action_key(entry.slot), entry.action_key, true)
            state.quick_action_dropdown = nil
          elseif entry.kind == "remember_layout" then
            ReaADR.set_window_layout_enabled(not ReaADR.window_layout_enabled())
          elseif entry.kind == "hover_preview" then
            ReaADR.set_cue_hover_preview_enabled(not ReaADR.cue_hover_preview_enabled())
          elseif entry.kind == "profile" then
            apply_overlay_profile(state.overlay_settings, entry.profile)
            state.overlay_dirty = true
          elseif entry.kind == "overlay_toggle" then
            state.overlay_settings[entry.key] = not state.overlay_settings[entry.key]
            state.overlay_dirty = true
          elseif entry.kind == "overlay_refresh" then
            App.refresh_overlay()
          elseif entry.kind == "metadata" then
            if edit_metadata_fields(state.overlay_settings) then
              state.overlay_dirty = true
            end
          elseif entry.kind == "text_color" then
            if state.overlay_settings.text_color ~= entry.value then
              state.overlay_settings.text_color = entry.value
              state.overlay_dirty = true
            end
          elseif entry.kind == "overlay_save" then
            local status = save_overlay_settings(state.overlay_settings)
            state.overlay_dirty = false
            state.overlay_message = status and ("Saved: " .. tostring(status)) or "Saved"
            state.overlay_message_until = reaper.time_precise() + 2.0
          end
        end
      end
    end
    state.last_mouse = mouse

    reaper.defer(frame)
  end

  local mouse_x, mouse_y = 0, 0
  if reaper.GetMousePosition then
    mouse_x, mouse_y = reaper.GetMousePosition()
  end
  if mouse_x and mouse_y and (mouse_x ~= 0 or mouse_y ~= 0) then
    gfx.init("ReaADR Tools Manager", state.width, state.height, 0, math.max(0, mouse_x - math.floor(state.width / 2)), math.max(0, mouse_y - 80))
  else
    gfx.init("ReaADR Tools Manager", state.width, state.height, 0)
  end
  frame()
end

return App
