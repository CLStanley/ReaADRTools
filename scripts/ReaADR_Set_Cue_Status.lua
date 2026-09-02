-- Set the status for the cue under the edit cursor.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local ReaADR = dofile(script_dir() .. "/ReaADR_Core.lua")

local statuses = ReaADR.cue_statuses()
gfx.init("Set ADR Cue Status", 0, 0, 0)
local choice = gfx.showmenu(table.concat(statuses, "|"))
gfx.quit()

local status = statuses[choice]
if not status then
  return
end

local cue, err = ReaADR.set_cue_status_at_position(status)
if not cue then
  ReaADR.message("Cue status was not changed:\n\n" .. tostring(err))
  return
end

ReaADR.message(("Cue %s status set to %s.\n\nVideo overlay refreshed."):format(tostring(cue.id or ""), status))
