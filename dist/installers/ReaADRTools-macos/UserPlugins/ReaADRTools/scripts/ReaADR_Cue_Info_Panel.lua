-- Live ADR cue information panel.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local ReaADR = dofile(script_dir() .. "/ReaADR_Core.lua")

local state = {
  width = 680,
  height = 360,
  closed = false,
}

local function draw_label(label, value, x, y)
  gfx.setfont(1, "Arial", 14)
  gfx.set(0.62, 0.66, 0.70, 1)
  gfx.x = x
  gfx.y = y
  gfx.drawstr(label)
  gfx.setfont(1, "Arial", 22)
  gfx.set(1, 1, 1, 1)
  gfx.x = x
  gfx.y = y + 18
  gfx.drawstr(tostring(value or ""))
end

local function frame()
  gfx.set(0.10, 0.11, 0.12, 1)
  gfx.rect(0, 0, state.width, state.height, true)

  local cue = ReaADR.active_cue()
  local frame_rate = reaper.TimeMap_curFrameRate(0)
  if not frame_rate or frame_rate <= 0 then
    frame_rate = 24
  end

  gfx.setfont(1, "Arial", 24)
  gfx.set(1, 1, 1, 1)
  gfx.x = 24
  gfx.y = 20
  gfx.drawstr("ADR Cue Information")

  if cue then
    local now = ReaADR.current_timeline_position()
    local cue_start = tonumber(cue.start_time) or 0
    local countdown = math.max(0, cue_start - now)
    local take_count = ReaADR.count_recorded_takes_for_cue(cue)

    draw_label("Cue", cue.id or "", 24, 70)
    draw_label("Character", cue.character or "", 180, 70)
    draw_label("Status", cue.status or "Not Recorded", 408, 70)

    draw_label("Start", ReaADR.format_timecode(cue.start_time, frame_rate), 24, 136)
    draw_label("End", ReaADR.format_timecode(cue.end_time, frame_rate), 220, 136)
    draw_label("Length", ("%.2fs"):format(ReaADR.cue_duration(cue)), 416, 136)

    draw_label("Countdown", ("%.2fs"):format(countdown), 24, 202)
    draw_label("Take Count", tostring(take_count), 220, 202)
    draw_label("Current Position", ReaADR.format_timecode(now, frame_rate), 416, 202)

    gfx.setfont(1, "Arial", 16)
    gfx.set(0.62, 0.66, 0.70, 1)
    gfx.x = 24
    gfx.y = 278
    gfx.drawstr("Line")
    gfx.setfont(1, "Arial", 24)
    gfx.set(1, 1, 1, 1)
    gfx.x = 24
    gfx.y = 302
    gfx.drawstr(tostring(cue.line or ""))
  else
    gfx.setfont(1, "Arial", 18)
    gfx.set(0.86, 0.88, 0.90, 1)
    gfx.x = 24
    gfx.y = 82
    gfx.drawstr("No ADR cues were found.")
  end

  gfx.update()
  local char = gfx.getchar()
  if char < 0 or char == 27 then
    gfx.quit()
    return
  end
  reaper.defer(frame)
end

gfx.init("ReaADR Cue Information", state.width, state.height)
frame()
