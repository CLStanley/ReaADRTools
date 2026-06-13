-- Toggle the active ReaADR character filter.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local ReaADR = dofile(script_dir() .. "/ReaADR_Core.lua")

local targets = ReaADR.available_filter_targets()
if not targets or #targets == 0 then
  ReaADR.message("No characters were found to filter.")
  return
end

local selected = {}
for _, target in ipairs(ReaADR.current_active_filter_targets(targets)) do
  selected[target.key] = true
end

local function selected_count()
  local count = 0
  for _, target in ipairs(targets) do
    if selected[target.key] then
      count = count + 1
    end
  end
  return count
end

local state = {
  width = 420,
  height = math.min(860, math.max(440, 210 + (#targets * 34))),
  min_width = 360,
  min_height = 420,
  last_mouse = 0,
  closed = false,
  hide_regions = ReaADR.character_filter_hides_regions(),
}

local rows = {}
local hide_regions_row = {}
local buttons = {}

local function layout(content_top)
  content_top = tonumber(content_top) or 108
  state.width = math.max(state.min_width, gfx.w or state.width)
  state.height = math.max(state.min_height, gfx.h or state.height)
  for index, target in ipairs(targets) do
    rows[index] = { x = 24, y = content_top + ((index - 1) * 32), w = state.width - 48, h = 26, target = target }
  end
  local button_y = state.height - 46
  hide_regions_row = { x = 24, y = button_y - 34, w = state.width - 48, h = 24 }
  buttons = {
    all = { x = 24, y = button_y, w = 70, h = 28, label = "All" },
    none = { x = 104, y = button_y, w = 70, h = 28, label = "None" },
    apply = { x = state.width - 178, y = button_y, w = 74, h = 28, label = "Apply" },
    cancel = { x = state.width - 94, y = button_y, w = 70, h = 28, label = "Cancel" },
  }
end

local function inside(rect, x, y)
  return x >= rect.x and x <= rect.x + rect.w and y >= rect.y and y <= rect.y + rect.h
end

local function draw_button(rect)
  local theme = ReaADR.ui_theme()
  ReaADR.set_gfx_color(theme.panel_alt)
  gfx.rect(rect.x, rect.y, rect.w, rect.h, true)
  ReaADR.set_gfx_color(theme.border)
  gfx.rect(rect.x, rect.y, rect.w, rect.h, false)
  gfx.setfont(1, "Arial", 14)
  ReaADR.set_gfx_color(theme.text)
  local tw, th = gfx.measurestr(rect.label)
  gfx.x = rect.x + (rect.w - tw) * 0.5
  gfx.y = rect.y + (rect.h - th) * 0.5
  gfx.drawstr(rect.label)
end

local function apply_filter()
  local active = {}
  local active_labels = {}
  for _, target in ipairs(targets) do
    if selected[target.key] then
      active[#active + 1] = target
      active_labels[#active_labels + 1] = target.label
    end
  end

  if #active == #targets then
    ReaADR.set_active_character_filter({})
  elseif #active == 0 then
    ReaADR.set_active_character_filter({ "__none__" })
  else
    ReaADR.set_active_character_filter(active)
  end
  ReaADR.set_character_filter_hides_regions(state.hide_regions)

  local summary = ReaADR.apply_character_filter()
  local label = #active == #targets and "All Characters" or (#active == 0 and "None" or table.concat(active_labels, ", "))
  ReaADR.message(
    ("Character filter: %s\n\nActive cue/dialogue tracks: %d\nMuted cue/dialogue tracks: %d\nHidden ruler regions: %d\nVisible ruler regions: %d"):format(
      label,
      summary.active_tracks or 0,
      summary.muted_tracks or 0,
      summary.hidden_regions or 0,
      summary.visible_regions or 0
    )
  )
end

local function close()
  if state.closed then
    return
  end
  state.closed = true
  ReaADR.save_window_state("character_filter")
  gfx.quit()
end

local function frame()
  local theme = ReaADR.ui_theme()
  ReaADR.set_gfx_color(theme.bg)
  gfx.rect(0, 0, state.width, state.height, true)

  local header = ReaADR.draw_window_header(
    "Character Filter",
    ("Selected: %d of %d"):format(selected_count(), #targets),
    { x = 24, y = 18, width = state.width - 48, height = 74 }
  )
  layout(math.max(108, header.content_y + 4))

  for _, row in ipairs(rows) do
    local key = row.target.key
    local checked = selected[key] == true
    ReaADR.set_gfx_color(theme.panel)
    gfx.rect(row.x, row.y, row.w, row.h, true)
    ReaADR.set_gfx_color(theme.border)
    gfx.rect(row.x, row.y, row.w, row.h, false)
    ReaADR.set_gfx_color(checked and theme.accent_gold or theme.panel_alt)
    gfx.rect(row.x + 6, row.y + 5, 14, 14, true)
    ReaADR.set_gfx_color(theme.border)
    gfx.rect(row.x + 6, row.y + 5, 14, 14, false)
    gfx.setfont(1, "Arial", 15)
    ReaADR.set_gfx_color(theme.text)
    gfx.x = row.x + 28
    gfx.y = row.y + 4
    gfx.drawstr(row.target.label)
  end

  ReaADR.set_gfx_color(theme.panel)
  gfx.rect(hide_regions_row.x, hide_regions_row.y, hide_regions_row.w, hide_regions_row.h, true)
  ReaADR.set_gfx_color(theme.border)
  gfx.rect(hide_regions_row.x, hide_regions_row.y, hide_regions_row.w, hide_regions_row.h, false)
  ReaADR.set_gfx_color(state.hide_regions and theme.accent_gold or theme.panel_alt)
  gfx.rect(hide_regions_row.x + 6, hide_regions_row.y + 5, 14, 14, true)
  ReaADR.set_gfx_color(theme.border)
  gfx.rect(hide_regions_row.x + 6, hide_regions_row.y + 5, 14, 14, false)
  gfx.setfont(1, "Arial", 15)
  ReaADR.set_gfx_color(theme.text)
  gfx.x = hide_regions_row.x + 28
  gfx.y = hide_regions_row.y + 4
  gfx.drawstr("Hide inactive ruler regions")

  draw_button(buttons.all)
  draw_button(buttons.none)
  draw_button(buttons.apply)
  draw_button(buttons.cancel)

  gfx.update()

  local char = gfx.getchar()
  if char < 0 or char == 27 then
    close()
    return
  elseif char == 13 then
    apply_filter()
    return
  end

  local mouse = gfx.mouse_cap % 2
  if mouse == 1 and state.last_mouse == 0 then
    for _, row in ipairs(rows) do
      if inside(row, gfx.mouse_x, gfx.mouse_y) then
        local key = row.target.key
        selected[key] = not selected[key]
      end
    end

    if inside(hide_regions_row, gfx.mouse_x, gfx.mouse_y) then
      state.hide_regions = not state.hide_regions
    elseif inside(buttons.all, gfx.mouse_x, gfx.mouse_y) then
      for _, target in ipairs(targets) do
        selected[target.key] = true
      end
    elseif inside(buttons.none, gfx.mouse_x, gfx.mouse_y) then
      selected = {}
    elseif inside(buttons.apply, gfx.mouse_x, gfx.mouse_y) then
      apply_filter()
    elseif inside(buttons.cancel, gfx.mouse_x, gfx.mouse_y) then
      close()
      return
    end
  end
  state.last_mouse = mouse

  reaper.defer(frame)
end

local restored = ReaADR.init_persistent_window("character_filter", "ReaADR Character Filter", {
  width = state.width,
  height = state.height,
})
state.width = restored.width
state.height = restored.height
frame()
