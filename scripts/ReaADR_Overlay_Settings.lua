-- Toggle ADR video overlay features for the current REAPER project.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local ReaADR = dofile(script_dir() .. "/ReaADR_Core.lua")
local settings = ReaADR.load_overlay_settings()

local rows = {
  { key = "enabled", label = "Enable video overlays" },
  { key = "show_cue_id", label = "Cue ID" },
  { key = "show_character", label = "Character name" },
  { key = "show_dialogue", label = "Dialogue line" },
  { key = "show_countdown", label = "Countdown before cue" },
  { key = "show_streamer", label = "Streamer bar" },
  { key = "show_flash", label = "Flash at cue start" },
  { key = "show_status", label = "Standby / take / clear status" },
}

local mouse_was_down = false
local dirty = false

gfx.init("ReaADR Overlay Settings", 430, 370)

local function save()
  ReaADR.save_overlay_settings(settings)
  dirty = false
end

local function draw_checkbox(x, y, checked, label)
  gfx.set(0.15, 0.16, 0.18, 1)
  gfx.rect(x, y, 18, 18, 0)
  if checked then
    gfx.set(0.1, 0.75, 1, 1)
    gfx.rect(x + 4, y + 4, 10, 10, 1)
  end
  gfx.set(0.92, 0.92, 0.92, 1)
  gfx.x = x + 30
  gfx.y = y - 1
  gfx.drawstr(label)
end

local function hit(x, y, w, h)
  return gfx.mouse_x >= x and gfx.mouse_x <= x + w and gfx.mouse_y >= y and gfx.mouse_y <= y + h
end

local function draw_button(x, y, w, h, label)
  local hovered = hit(x, y, w, h)
  if hovered then
    gfx.set(0.18, 0.42, 0.55, 1)
  else
    gfx.set(0.12, 0.26, 0.34, 1)
  end
  gfx.rect(x, y, w, h, 1)
  gfx.set(1, 1, 1, 1)
  gfx.x = x + 18
  gfx.y = y + 8
  gfx.drawstr(label)
  return hovered
end

local function loop()
  gfx.set(0.08, 0.09, 0.10, 1)
  gfx.rect(0, 0, gfx.w, gfx.h, 1)

  gfx.setfont(1, "Arial", 20)
  gfx.set(1, 1, 1, 1)
  gfx.x = 20
  gfx.y = 18
  gfx.drawstr("ReaADR Video Overlay Settings")

  gfx.setfont(1, "Arial", 16)
  local mouse_down = gfx.mouse_cap % 2 == 1
  local clicked = mouse_down and not mouse_was_down

  local y = 58
  for _, row in ipairs(rows) do
    draw_checkbox(24, y, settings[row.key], row.label)
    if clicked and hit(20, y - 4, 360, 28) then
      settings[row.key] = not settings[row.key]
      dirty = true
    end
    y = y + 32
  end

  gfx.set(0.72, 0.72, 0.72, 1)
  gfx.x = 24
  gfx.y = y + 2
  gfx.drawstr("Preroll seconds")

  local minus_hover = draw_button(170, y - 5, 36, 30, "-")
  gfx.set(0.92, 0.92, 0.92, 1)
  gfx.x = 220
  gfx.y = y + 2
  gfx.drawstr(string.format("%.1f", settings.preroll_seconds))
  local plus_hover = draw_button(282, y - 5, 36, 30, "+")

  if clicked and minus_hover then
    settings.preroll_seconds = math.max(0, settings.preroll_seconds - 0.5)
    dirty = true
  elseif clicked and plus_hover then
    settings.preroll_seconds = math.min(10, settings.preroll_seconds + 0.5)
    dirty = true
  end

  local save_hover = draw_button(24, gfx.h - 50, 92, 34, "Save")
  local close_hover = draw_button(130, gfx.h - 50, 92, 34, "Close")

  if dirty then
    gfx.set(1, 0.78, 0.2, 1)
    gfx.x = 240
    gfx.y = gfx.h - 40
    gfx.drawstr("Unsaved changes")
  end

  if clicked and save_hover then
    save()
  elseif clicked and close_hover then
    save()
    gfx.quit()
    return
  end

  mouse_was_down = mouse_down
  if gfx.getchar() >= 0 then
    reaper.defer(loop)
  elseif dirty then
    save()
  end
end

loop()
