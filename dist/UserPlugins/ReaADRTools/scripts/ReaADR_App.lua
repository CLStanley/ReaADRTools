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
      { label = "Generate Cues from Markers/Regions", script = "ReaADR_Generate_Cues.lua", hint = "Create ADR cue items from existing project markers or regions without importing a cue sheet." },
    },
  },
  cues = {
    title = "Cue Management",
    actions = {
      { label = "Open Cue Manager", script = "ReaADR_Cue_Manager.lua", hint = "Browse cues, jump around the session, update cue status, and refresh the active overlay." },
      { label = "Character Filter", script = "ReaADR_Character_Filter.lua", hint = "Enable or disable character tracks for focused recording passes." },
    },
  },
  session = {
    title = "Session Tools",
    actions = {
      { label = "Open Cue Manager", script = "ReaADR_Cue_Manager.lua", hint = "Review the generated cue list and make cue-level changes." },
      { label = "Validate Session", app_action = "validate_session", hint = "Check cue timing, missing fields, overlap splits, and preserved metadata." },
      { label = "Refresh Video Overlay", app_action = "refresh_overlay", hint = "Rebuild only the video overlay from the current project/session data." },
      { label = "Rebuild Session From Cache", app_action = "rebuild_session", hint = "Recreate tracks, regions, cue audio, and overlays from the last imported session cache." },
      { label = "Clean Generated Cue Items", script = "ReaADR_Clean_Generated_Cues.lua", hint = "Remove ReaADR-generated cue items without deleting user recordings." },
    },
  },
  reports = {
    title = "Reports",
    actions = {
      { label = "Export Cue Sheet CSV", script = "ReaADR_Export_Cue_Sheet.lua", hint = "Export regions and cues to a flexible CSV for editing or reimporting later." },
    },
  },
  preferences = {
    title = "Preferences",
    actions = {
      { label = "Overlay Settings", script = "ReaADR_Overlay_Settings.lua", hint = "Choose which cue, timecode, visual cue, dialogue, status, and metadata fields appear over video." },
      { label = "Configure Quick Actions", app_action = "configure_quick_actions", hint = "Choose what the top-level ReaADR Tools quick-action menu slots run." },
    },
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

App.manager_order = { "import", "cues", "session", "reports", "preferences", "help" }

App.quick_action_choices = {
  { key = "import", label = "Import Cue Sheet", script = "ReaADR_Import_Cue_Sheet.lua" },
  { key = "cue_manager", label = "Open Cue Manager", script = "ReaADR_Cue_Manager.lua" },
  { key = "export_reports", label = "Export Reports", app_action = "export_reports" },
  { key = "overlay_settings", label = "Overlay Settings", script = "ReaADR_Overlay_Settings.lua" },
  { key = "character_filter", label = "Character Filter", script = "ReaADR_Character_Filter.lua" },
  { key = "refresh_overlay", label = "Refresh Video Overlay", app_action = "refresh_overlay" },
  { key = "validate", label = "Validate Session", app_action = "validate_session" },
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
      "Import Cue Sheet accepts CSV and TSV files. Columns can be in any order.",
      "",
      "Required ADR fields are cue number, character, start time, and dialogue. End time is recommended. Unknown columns are preserved as cue metadata when possible.",
      "",
      "Use the column mapping step when a studio sheet uses names like Role, In, Out, or Line instead of ReaADR's default names.",
    }, "\n"),
  },
  cues = {
    title = "Cue Management",
    keywords = "cue manager status navigation next previous jump character filter selected cue",
    body = table.concat({
      "Cue Manager lets you browse cues, jump to cues, set cue status, refresh the overlay, and open the cue information panel.",
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
      "Export Cue Sheet CSV is useful when building a project inside REAPER first, then filling in dialogue or metadata later in a spreadsheet.",
    }, "\n"),
  },
  quick_actions = {
    title = "Quick Actions",
    keywords = "quick action customize menu top menu shortcuts manager",
    body = table.concat({
      "The top-level ReaADR Tools menu keeps Open Manager first, followed by four configurable quick-action slots.",
      "",
      "Use Manager > Preferences > Configure Quick Actions to choose what each slot runs. Restart REAPER after changing them if you want the native menu labels to update.",
    }, "\n"),
  },
}

function App.run_script(script_name)
  if not script_name or script_name == "" then
    return false
  end
  dofile(App.base_dir .. "/" .. script_name)
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

function App.configure_quick_actions()
  local state = {
    width = 620,
    height = 360,
    last_mouse = 0,
  }

  local rows = {}
  for slot = 1, 4 do
    rows[slot] = { x = 24, y = 82 + ((slot - 1) * 48), w = 560, h = 34, slot = slot }
  end

  local done = { x = 470, y = 294, w = 114, h = 34, label = "Done" }

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
    local mouse_x, mouse_y = reaper.GetMousePosition()
    gfx.init("ReaADR Help", 0, 0, 0, mouse_x, mouse_y)
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
  return App.run_named_action("Overlay Settings")
end

function App.export_reports()
  local ReaADR = App.ReaADR
  local cues = ReaADR.load_last_import_cues()
  if not cues then
    cues = ReaADR.navigation_cues()
  end
  cues = cues or {}
  if #cues == 0 then
    ReaADR.message("No cue data was found to export.")
    return false
  end

  local reports = {
    { label = "Cue Sheet CSV", suffix = "cue_sheet", export = ReaADR.export_cues_to_csv },
    { label = "Recording Report CSV", suffix = "recording_report", export = ReaADR.export_recording_report },
    { label = "Timing Report CSV", suffix = "timing_report", export = ReaADR.export_timing_report },
    { label = "Session Metadata CSV", suffix = "session_metadata", export = ReaADR.export_session_metadata_report },
  }

  local labels = {}
  for index, report in ipairs(reports) do
    labels[index] = report.label
  end

  local mouse_x, mouse_y = reaper.GetMousePosition()
  gfx.init("ReaADR Reports", 0, 0, 0, mouse_x, mouse_y)
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
  local default_path = project_path .. "/reaadr_" .. report.suffix .. ".csv"
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

function App.validate_session()
  local ReaADR = App.ReaADR
  local cues = ReaADR.session_cues()
  local validation = ReaADR.validate_cues(cues or {}, { preroll_seconds = ReaADR.load_overlay_settings().preroll_seconds })
  ReaADR.message(ReaADR.validation_summary_text(validation):gsub("\nBuild this ADR session%?$", ""))
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
  local cues, err = ReaADR.load_last_import_cues()
  if not cues then
    ReaADR.message("No cached ReaADR session found:\n\n" .. tostring(err))
    return false
  end
  local answer = reaper.ShowMessageBox("Rebuild tracks, regions, cue audio, and overlay from the cached ReaADR session?", "ReaADR", 4)
  if answer ~= 6 then
    return false
  end
  local progress = ReaADR.create_progress_window("Rebuilding ReaADR Session")
  local summary, setup_error = ReaADR.setup_project(cues, {
    cue_audio_path = App.base_dir .. "/../assets/cue.wav",
    overlay_settings = ReaADR.load_overlay_settings(),
    on_progress = progress.update,
  })
  progress.close()
  if not summary then
    ReaADR.message("Session rebuild failed:\n\n" .. tostring(setup_error))
    return false
  end
  ReaADR.message(("Rebuilt %d cue(s).\n\nTracks: %d created, %d reused\nRegions: %d created, %d updated\nOverlay: %s"):format(
    summary.cue_count,
    summary.tracks_created,
    summary.tracks_reused,
    summary.regions_created,
    summary.regions_updated,
    summary.overlay_fx_status
  ))
  return true
end

local function session_summary(ReaADR)
  local cues = ReaADR.load_last_import_cues()
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
  local hover = inside(rect, gfx.mouse_x, gfx.mouse_y)
  gfx.set(hover and 0.22 or 0.20, hover and 0.32 or 0.22, hover and 0.38 or 0.24, 1)
  gfx.rect(rect.x, rect.y, rect.w, rect.h, true)
  gfx.set(0.62, 0.66, 0.70, 1)
  gfx.rect(rect.x, rect.y, rect.w, rect.h, false)
  gfx.setfont(1, "Arial", 15)
  gfx.set(1, 1, 1, 1)
  gfx.x = rect.x + 12
  gfx.y = rect.y + 8
  gfx.drawstr(rect.label)
  return hover
end

function App.open_manager()
  local ReaADR = App.ReaADR
  local summary = session_summary(ReaADR)
  local state = {
    width = 880,
    height = 600,
    tab = "import",
    last_mouse = 0,
    closed = false,
  }

  local tab_rects = {}
  local tab_x = 24
  for _, key in ipairs(App.manager_order) do
    local module = App.modules[key]
    local w = math.max(104, (#module.title * 8) + 26)
    tab_rects[#tab_rects + 1] = { key = key, x = tab_x, y = 70, w = w, h = 30, label = module.title }
    tab_x = tab_x + w + 8
  end

  local function close()
    if state.closed then
      return
    end
    state.closed = true
    gfx.quit()
  end

  local function frame()
    local hover_hint = ""
    gfx.set(0.11, 0.12, 0.13, 1)
    gfx.rect(0, 0, state.width, state.height, true)

    gfx.setfont(1, "Arial", 22)
    gfx.set(1, 1, 1, 1)
    gfx.x = 24
    gfx.y = 20
    gfx.drawstr("ReaADR Tools Manager")

    gfx.setfont(1, "Arial", 14)
    gfx.set(0.78, 0.81, 0.84, 1)
    gfx.x = 24
    gfx.y = 48
    gfx.drawstr(("Session: %d cues, %d characters"):format(summary.cue_count, summary.character_count))

    for _, tab in ipairs(tab_rects) do
      local active = tab.key == state.tab
      local hovered = inside(tab, gfx.mouse_x, gfx.mouse_y)
      gfx.set(active and 0.28 or 0.17, active and 0.32 or 0.19, active and 0.36 or 0.21, 1)
      gfx.rect(tab.x, tab.y, tab.w, tab.h, true)
      gfx.set(0.58, 0.62, 0.66, 1)
      gfx.rect(tab.x, tab.y, tab.w, tab.h, false)
      gfx.setfont(1, "Arial", 14)
      gfx.set(1, 1, 1, 1)
      gfx.x = tab.x + 12
      gfx.y = tab.y + 8
      gfx.drawstr(tab.label)
      if hovered then
        hover_hint = "Open " .. tab.label .. " tools."
      end
    end

    local module = App.modules[state.tab]
    local buttons = {}
    local y = 128
    gfx.setfont(1, "Arial", 18)
    gfx.set(1, 1, 1, 1)
    gfx.x = 24
    gfx.y = y
    gfx.drawstr(module.title)
    y = y + 38

    for index, action in ipairs(module.actions or {}) do
      local rect = { x = 24, y = y + ((index - 1) * 44), w = 320, h = 34, label = action.label, action = action }
      buttons[#buttons + 1] = rect
      if draw_button(rect) and action.hint then
        hover_hint = action.hint
      end
    end

    if state.tab == "cues" and #summary.characters > 0 then
      gfx.setfont(1, "Arial", 14)
      gfx.set(0.78, 0.81, 0.84, 1)
      gfx.x = 382
      gfx.y = 166
      gfx.drawstr("Characters")
      gfx.x = 382
      gfx.y = 190
      gfx.drawstr(table.concat(summary.characters, ", "))
    end

    local quick_y = 166
    if state.tab == "preferences" then
      gfx.setfont(1, "Arial", 14)
      gfx.set(0.78, 0.81, 0.84, 1)
      gfx.x = 382
      gfx.y = quick_y
      gfx.drawstr("Current quick actions")
      for slot = 1, 4 do
        local action = App.get_quick_action(slot)
        gfx.x = 382
        gfx.y = quick_y + (slot * 24)
        gfx.drawstr(("Quick Action %d: %s"):format(slot, action and action.label or "Not configured"))
      end
    elseif state.tab == "help" then
      gfx.setfont(1, "Arial", 14)
      gfx.set(0.78, 0.81, 0.84, 1)
      gfx.x = 382
      gfx.y = quick_y
      gfx.drawstr("Search by action, workflow, or keyword.")
      gfx.x = 382
      gfx.y = quick_y + 26
      gfx.drawstr("Examples: import, overlay, SMPTE, filter, reports")
    end

    if hover_hint ~= "" then
      gfx.set(0.08, 0.09, 0.10, 1)
      gfx.rect(18, state.height - 54, state.width - 36, 34, true)
      gfx.set(0.35, 0.40, 0.45, 1)
      gfx.rect(18, state.height - 54, state.width - 36, 34, false)
      gfx.setfont(1, "Arial", 13)
      gfx.set(0.86, 0.90, 0.94, 1)
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
    end

    local mouse = gfx.mouse_cap % 2
    if mouse == 1 and state.last_mouse == 0 then
      for _, tab in ipairs(tab_rects) do
        if inside(tab, gfx.mouse_x, gfx.mouse_y) then
          state.tab = tab.key
        end
      end
      for _, button in ipairs(buttons) do
        if inside(button, gfx.mouse_x, gfx.mouse_y) then
          close()
          run_action(button.action)
          return
        end
      end
    end
    state.last_mouse = mouse

    reaper.defer(frame)
  end

  gfx.init("ReaADR Tools Manager", state.width, state.height)
  frame()
end

return App
