-- ReaADR_Clean_Generated_Cues.lua
-- Character-specific cue clearing.
-- Presents a character selection dialog; removes cues, regions, and cue-audio
-- tracks for selected characters and marks them as completed.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local ReaADR = dofile(script_dir() .. "/ReaADR_Core.lua")

local cues = ReaADR.load_session_cues()
if not cues or #cues == 0 then
  ReaADR.message("No ADR session is loaded.\n\nImport a cue sheet first.")
  return
end

local characters = ReaADR.collect_characters(cues)
if #characters == 0 then
  ReaADR.message("No characters were found in the current session.")
  return
end

-- Pre-tick characters that are already marked completed
local selected = {}
for _, character in ipairs(characters) do
  if ReaADR.is_character_recording_completed(character) then
    selected[character] = true
  end
end

local state = {
  width     = 440,
  height    = math.min(820, math.max(400, 200 + (#characters * 34))),
  min_width  = 380,
  min_height = 360,
  last_mouse = 0,
  closed     = false,
}

local rows      = {}
local btn_all   = {}
local btn_none  = {}
local btn_clear = {}
local btn_cancel= {}
local last_layout_w, last_layout_h = 0, 0

local function layout()
  local w = math.max(state.min_width,  gfx.w or state.width)
  local h = math.max(state.min_height, gfx.h or state.height)
  if w == last_layout_w and h == last_layout_h then return end
  last_layout_w, last_layout_h = w, h
  state.width, state.height = w, h

  local content_top = 96
  for index, character in ipairs(characters) do
    rows[index] = { x = 24, y = content_top + (index - 1) * 32, w = w - 48, h = 26, character = character }
  end
  local by = h - 52
  btn_all    = { x = 24,       y = by, w = 66, h = 28, label = "All" }
  btn_none   = { x = 100,      y = by, w = 66, h = 28, label = "None" }
  btn_clear  = { x = w - 160,  y = by, w = 76, h = 28, label = "Clear" }
  btn_cancel = { x = w - 74,   y = by, w = 66, h = 28, label = "Cancel" }
end

local function inside(r, x, y)
  return x >= r.x and x <= r.x + r.w and y >= r.y and y <= r.y + r.h
end

local function selected_count()
  local n = 0
  for _, character in ipairs(characters) do
    if selected[character] then n = n + 1 end
  end
  return n
end

local function draw_button(rect, accent)
  local theme = ReaADR.ui_theme()
  local hover = inside(rect, gfx.mouse_x, gfx.mouse_y)
  ReaADR.set_gfx_color(accent or (hover and theme.highlight or theme.panel_alt))
  gfx.rect(rect.x, rect.y, rect.w, rect.h, true)
  ReaADR.set_gfx_color(theme.border)
  gfx.rect(rect.x, rect.y, rect.w, rect.h, false)
  gfx.setfont(1, "Arial", 14)
  ReaADR.set_gfx_color(theme.text)
  local tw = gfx.measurestr(rect.label)
  gfx.x = rect.x + (rect.w - tw) * 0.5
  gfx.y = rect.y + 6
  gfx.drawstr(rect.label)
end

local function do_clear()
  local to_clear = {}
  for _, character in ipairs(characters) do
    if selected[character] then
      to_clear[#to_clear + 1] = character
    end
  end
  if #to_clear == 0 then
    ReaADR.message("No characters were selected.")
    return
  end

  local answer = reaper.ShowMessageBox(
    ("Clear cues for %d character(s)?\n\n%s\n\nThis removes their cues, regions, and cue tracks.\nRecording tracks and recorded audio are not affected."):format(
      #to_clear, table.concat(to_clear, ", ")
    ),
    "ReaADR – Clear Character Cues",
    4
  )
  if answer ~= 6 then return end

  local summary = ReaADR.clear_cues_for_characters(to_clear)
  ReaADR.message(
    ("Cleared %d cue(s), %d region(s), %d cue track(s)."):format(
      summary.cues_removed, summary.regions_removed, summary.tracks_removed
    )
  )
  state.closed = true
  ReaADR.save_window_state("clear_character_cues")
  gfx.quit()
end

local function frame()
  local theme = ReaADR.ui_theme()
  layout()
  ReaADR.set_gfx_color(theme.bg)
  gfx.rect(0, 0, state.width, state.height, true)

  ReaADR.draw_window_header(
    "Clear Character Cues",
    ("Select characters to clear  (%d of %d selected)"):format(selected_count(), #characters),
    { x = 20, y = 14, width = state.width - 40, height = 64 }
  )

  for _, row in ipairs(rows) do
    local checked = selected[row.character] == true
    local completed = ReaADR.is_character_recording_completed(row.character)
    ReaADR.set_gfx_color(theme.panel)
    gfx.rect(row.x, row.y, row.w, row.h, true)
    ReaADR.set_gfx_color(theme.border)
    gfx.rect(row.x, row.y, row.w, row.h, false)
    ReaADR.set_gfx_color(checked and theme.accent_gold or theme.panel_alt)
    gfx.rect(row.x + 6, row.y + 5, 14, 14, true)
    ReaADR.set_gfx_color(theme.border)
    gfx.rect(row.x + 6, row.y + 5, 14, 14, false)
    gfx.setfont(1, "Arial", 14)
    ReaADR.set_gfx_color(theme.text)
    gfx.x = row.x + 28
    gfx.y = row.y + 5
    gfx.drawstr(row.character)
    if completed then
      ReaADR.set_gfx_color(theme.muted)
      gfx.x = row.x + row.w - 110
      gfx.drawstr("(completed)")
    end
  end

  draw_button(btn_all)
  draw_button(btn_none)
  draw_button(btn_clear, selected_count() > 0 and { 0.55, 0.10, 0.10, 1.0 } or nil)
  draw_button(btn_cancel)

  gfx.update()

  local char = gfx.getchar()
  if char < 0 or char == 27 then
    state.closed = true
    ReaADR.save_window_state("clear_character_cues")
    gfx.quit()
    return
  elseif char == 13 then
    do_clear()
    return
  end

  local mouse = gfx.mouse_cap % 2
  if mouse == 1 and state.last_mouse == 0 then
    for _, row in ipairs(rows) do
      if inside(row, gfx.mouse_x, gfx.mouse_y) then
        selected[row.character] = not selected[row.character]
      end
    end
    if inside(btn_all, gfx.mouse_x, gfx.mouse_y) then
      for _, character in ipairs(characters) do selected[character] = true end
    elseif inside(btn_none, gfx.mouse_x, gfx.mouse_y) then
      selected = {}
    elseif inside(btn_clear, gfx.mouse_x, gfx.mouse_y) then
      do_clear()
      return
    elseif inside(btn_cancel, gfx.mouse_x, gfx.mouse_y) then
      state.closed = true
      ReaADR.save_window_state("clear_character_cues")
      gfx.quit()
      return
    end
  end
  state.last_mouse = mouse

  reaper.defer(frame)
end

ReaADR.init_persistent_window("clear_character_cues", "ReaADR \xe2\x80\x93 Clear Character Cues", {
  width  = state.width,
  height = state.height,
})

frame()
