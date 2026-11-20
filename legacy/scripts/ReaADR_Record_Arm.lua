return function(reaper_api, project)
  project = project or 0
  local snapshot = nil

  local manager = {}
  local function valid(track)
    return type(reaper_api.ValidatePtr2) ~= "function"
      or reaper_api.ValidatePtr2(project, track, "MediaTrack*")
  end

  function manager.capture_and_isolate(target_track)
    if not target_track then
      return nil, "No ADR recording track is available."
    end
    if not snapshot then
      snapshot = {}
      for index = 0, reaper_api.CountTracks(project) - 1 do
        local track = reaper_api.GetTrack(project, index)
        snapshot[#snapshot + 1] = {
          track = track,
          armed = reaper_api.GetMediaTrackInfo_Value(track, "I_RECARM"),
        }
      end
    end
    for _, entry in ipairs(snapshot) do
      if valid(entry.track) then
        reaper_api.SetMediaTrackInfo_Value(entry.track, "I_RECARM", entry.track == target_track and 1 or 0)
      end
    end
    return true
  end

  function manager.restore()
    if not snapshot then
      return true
    end
    local saved = snapshot
    snapshot = nil
    for _, entry in ipairs(saved) do
      if valid(entry.track) then
        reaper_api.SetMediaTrackInfo_Value(entry.track, "I_RECARM", entry.armed)
      end
    end
    return true
  end

  function manager.has_snapshot()
    return snapshot ~= nil
  end

  return manager
end
