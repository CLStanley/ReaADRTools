-- ReaADR_Record_Cue.lua
-- Record the active ADR cue.
-- Positions the cursor at the preroll point, arms the character recording track,
-- records, and stops automatically when the cue end is reached.
-- Loop mode re-arms and repeats takes without manual intervention.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local ReaADR = dofile(script_dir() .. "/ReaADR_Core.lua")

local cue = ReaADR.active_cue()
if not cue then
  ReaADR.message(
    "No ADR cue is selected.\n\n" ..
    "Navigate to a cue using the Cue Manager or Next/Previous Cue first."
  )
  return
end

local settings     = ReaADR.load_overlay_settings()
local preroll      = math.max(0, tonumber(settings.preroll_seconds) or 3.0)
local cue_start    = tonumber(cue.start_time) or 0
local cue_end      = tonumber(cue.end_time) or cue_start
local record_start = math.max(0, cue_start - preroll)
local frame_rate   = reaper.TimeMap_curFrameRate(0)

-- Locate all recording tracks tagged for this character.
-- Uses ReaADR.character_filter_key to match the same token Core.lua stores.
local function find_rec_tracks(character)
  local token = ReaADR.character_filter_key(character)
  local tracks = {}
  for i = 0, reaper.CountTracks(0) - 1 do
    local track = reaper.GetTrack(0, i)
    local _, role = reaper.GetSetMediaTrackInfo_String(track, "P_EXT:ReaADR.role", "", false)
    if role == "character" then
      local _, key = reaper.GetSetMediaTrackInfo_String(track, "P_EXT:ReaADR.key", "", false)
      -- Stored key format: "Token.laneN" — compare lowercased token portion only.
      local key_char = tostring(key or ""):lower():match("^(.-)%.lane") or ""
      if key_char == token then
        tracks[#tracks + 1] = track
      end
    end
  end
  return tracks
end

local rec_tracks = find_rec_tracks(cue.character)

-- Cache track names once — they don't change while the window is open.
local track_label
do
  if #rec_tracks == 0 then
    track_label = nil  -- error shown in draw
  else
    local names = {}
    for _, t in ipairs(rec_tracks) do
      local _, n = reaper.GetSetMediaTrackInfo_String(t, "P_NAME", "", false)
      names[#names + 1] = tostring(n or "")
    end
    track_label = "Track: " .. table.concat(names, ", ")
  end
end

local IDLE      = "idle"
local RECORDING = "recording"
local LOOP_WAIT = "loop_wait"  -- stop sent, waiting for transport to settle

local state = {
  mode       = IDLE,
  loop       = false,
  take_count = 0,
  width      = 540,
  height     = 290,
  min_width  = 460,
  min_height = 250,
  last_mouse = 0,
  status_msg = "",
}

local theme    = ReaADR.ui_theme()
local btn_rec  = {}
local btn_loop = {}
local btn_stop = {}
local last_layout_w, last_layout_h = 0, 0

local function layout()
  local w = math.max(state.min_width,  gfx.w or state.width)
  local h = math.max(state.min_height, gfx.h or state.height)
  if w == last_layout_w and h == last_layout_h then return end
  last_layout_w, last_layout_h = w, h
  state.width, state.height = w, h

  local bw = 128
  local bh = 34
  local by = h - 58
  btn_rec  = { x = 24,        y = by, w = bw, h = bh }
  btn_loop = { x = 24+bw+14,  y = by, w = bw, h = bh }
  btn_stop = { x = w-bw-24,   y = by, w = bw, h = bh }
end

local function inside(r, x, y)
  return x >= r.x and x <= r.x + r.w and y >= r.y and y <= r.y + r.h
end

local function draw_btn(rect, label, fill)
  local hover = inside(rect, gfx.mouse_x, gfx.mouse_y)
  ReaADR.set_gfx_color(fill or (hover and theme.highlight or theme.panel_alt))
  gfx.rect(rect.x, rect.y, rect.w, rect.h, true)
  ReaADR.set_gfx_color(theme.border)
  gfx.rect(rect.x, rect.y, rect.w, rect.h, false)
  gfx.setfont(1, "Arial", 14)
  ReaADR.set_gfx_color(theme.text)
  gfx.x = rect.x + 10
  gfx.y = rect.y + 9
  gfx.drawstr(label)
end

local function arm_tracks()
  for _, t in ipairs(rec_tracks) do
    reaper.SetMediaTrackInfo_Value(t, "I_RECARM", 1)
  end
end

local function disarm_tracks()
  for _, t in ipairs(rec_tracks) do
    reaper.SetMediaTrackInfo_Value(t, "I_RECARM", 0)
  end
end

local function finalize_takes()
  if state.take_count > 0 then
    ReaADR.set_cue_status_at_position("Recorded", cue_start + 0.001)
    ReaADR.refresh_overlay_silent()
  end
  state.status_msg = ("%d take%s recorded."):format(
    state.take_count, state.take_count == 1 and "" or "s")
end

-- finalize_stop updates state only; does not send a Stop command.
-- Use this when the transport already stopped on its own.
local function finalize_stop(loop_after)
  if loop_after then
    state.mode = LOOP_WAIT
  else
    state.mode = IDLE
    disarm_tracks()
    finalize_takes()
  end
end

-- stop_take sends the Stop command then finalizes state.
-- Use this when we need to halt an in-progress recording.
local function stop_take(loop_after)
  reaper.Main_OnCommand(1016, 0)  -- Stop
  finalize_stop(loop_after)
end

local function start_take()
  reaper.SetEditCurPos(record_start, true, false)
  arm_tracks()
  reaper.Main_OnCommand(1013, 0)  -- Record
  state.mode       = RECORDING
  state.take_count = state.take_count + 1
  state.status_msg = ("Recording take %d\xe2\x80\xa6"):format(state.take_count)
  ReaADR.set_active_overlay_cue(cue)
  ReaADR.refresh_overlay_silent()
end

local function frame()
  layout()
  ReaADR.set_gfx_color(theme.bg)
  gfx.rect(0, 0, state.width, state.height, true)

  local header = ReaADR.draw_window_header(
    "Record Cue",
    ("Cue %s  \xe2\x80\x93  %s"):format(
      tostring(cue.id or "?"), tostring(cue.character or "")
    ),
    { x = 20, y = 14, width = state.width - 40, height = 64 }
  )

  local y = header.content_y + 6

  -- Dialogue
  gfx.setfont(1, "Arial", 13)
  ReaADR.set_gfx_color(theme.muted)
  gfx.x, gfx.y = 24, y
  local line = tostring(cue.line or "")
  if #line > 72 then line = line:sub(1, 69) .. "\xe2\x80\xa6" end
  gfx.drawstr(line ~= "" and line or "(no dialogue)")
  y = y + 20

  -- Timecode and duration
  gfx.x, gfx.y = 24, y
  local tc  = ReaADR.format_timecode(cue_start, frame_rate)
  local dur = ("%.1fs cue  +  %.1fs preroll"):format(cue_end - cue_start, preroll)
  gfx.drawstr(tc .. "   " .. dur)
  y = y + 20

  -- Track
  gfx.x, gfx.y = 24, y
  if not track_label then
    ReaADR.set_gfx_color(theme.accent_red)
    gfx.drawstr("No recording track found for: " .. tostring(cue.character or ""))
  else
    ReaADR.set_gfx_color(theme.text)
    gfx.drawstr(track_label)
  end
  y = y + 26

  -- Status
  gfx.setfont(1, "Arial", 14)
  gfx.x, gfx.y = 24, y
  if state.mode == RECORDING then
    ReaADR.set_gfx_color(theme.accent_red)
    gfx.drawstr("\xe2\x97\x8f " .. state.status_msg)
  elseif state.mode == LOOP_WAIT then
    ReaADR.set_gfx_color(theme.accent_gold)
    gfx.drawstr("\xe2\x86\xba Looping\xe2\x80\xa6")
  elseif state.status_msg ~= "" then
    ReaADR.set_gfx_color(theme.accent_green)
    gfx.drawstr(state.status_msg)
  end

  -- Buttons
  draw_btn(btn_rec,  "Record",
    state.mode == RECORDING and { 0.55, 0.10, 0.10, 1.0 } or nil)
  draw_btn(btn_loop, state.loop and "Loop: ON" or "Loop: OFF",
    state.loop and theme.accent_green or nil)
  draw_btn(btn_stop, "Stop")

  gfx.update()

  -- Transport monitoring
  if state.mode == RECORDING then
    local play_state = reaper.GetPlayState()
    if play_state == 0 then
      -- Transport stopped externally; don't re-send Stop.
      finalize_stop(false)
    elseif reaper.GetPlayPosition() >= cue_end then
      stop_take(state.loop)
    end
  elseif state.mode == LOOP_WAIT then
    if reaper.GetPlayState() == 0 then
      start_take()
    end
  end

  -- Close / Escape
  local char = gfx.getchar()
  if char < 0 or char == 27 then
    if state.mode ~= IDLE then
      stop_take(false)
    end
    ReaADR.save_window_state("record_cue")
    gfx.quit()
    return
  end

  -- Mouse clicks
  local mouse = gfx.mouse_cap % 2
  if mouse == 1 and state.last_mouse == 0 then
    if inside(btn_rec, gfx.mouse_x, gfx.mouse_y) then
      if state.mode == IDLE then start_take() end
    elseif inside(btn_loop, gfx.mouse_x, gfx.mouse_y) then
      state.loop = not state.loop
    elseif inside(btn_stop, gfx.mouse_x, gfx.mouse_y) then
      if state.mode ~= IDLE then stop_take(false) end
    end
  end
  state.last_mouse = mouse

  reaper.defer(frame)
end

ReaADR.init_persistent_window("record_cue", "ReaADR \xe2\x80\x93 Record Cue", {
  width  = state.width,
  height = state.height,
})

frame()
