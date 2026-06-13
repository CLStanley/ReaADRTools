-- Remove generated ReaADR cue items without deleting user recordings.

local function script_dir()
  local info = debug.getinfo(1, "S").source
  local path = info:sub(1, 1) == "@" and info:sub(2) or info
  return path:match("^(.*)[/\\]") or "."
end

local ReaADR = dofile(script_dir() .. "/ReaADR_Core.lua")

local answer = reaper.ShowMessageBox(
  "Remove generated ReaADR cue audio items?\n\nThis does not delete user recordings or project regions.",
  "ReaADR",
  4
)
if answer ~= 6 then
  return
end

local summary = ReaADR.cleanup_generated_items()
ReaADR.message(("Removed %d generated cue item(s)."):format(summary.removed_items))
