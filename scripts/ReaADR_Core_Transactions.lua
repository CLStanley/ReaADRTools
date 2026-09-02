return function(ReaADR, deps)
  local project = deps.project
  local depth = 0

  function ReaADR.transaction_active()
    return depth > 0
  end

  -- Nested helpers join the current transaction. Only the outer user action
  -- owns REAPER's undo block and, on failure, rolls project mutations back.
  function ReaADR.with_project_transaction(options, callback)
    options = options or {}
    local owner = options.manage_undo ~= false and depth == 0
    if owner then
      reaper.Undo_BeginBlock()
    end
    depth = depth + 1

    local results = table.pack(xpcall(callback, debug.traceback))
    depth = depth - 1
    local ok = results[1]

    if owner then
      local description = ok
        and (options.description or "ReaADR: project operation")
        or (options.failure_description or ((options.description or "ReaADR: project operation") .. " (failed)"))
      reaper.Undo_EndBlock(description, options.flags or -1)
      if not ok and options.rollback_on_error ~= false and type(reaper.Undo_DoUndo2) == "function" then
        -- Do not undo an unrelated prior user action if REAPER decided the
        -- failed block contained no project mutation and created no undo point.
        local can_undo = type(reaper.Undo_CanUndo2) == "function"
          and reaper.Undo_CanUndo2(project()) == description
        if can_undo then reaper.Undo_DoUndo2(project()) end
      end
    end

    if not ok then
      return nil, results[2]
    end
    return table.unpack(results, 2, results.n)
  end

  function ReaADR.with_ui_refresh_suppressed(callback)
    reaper.PreventUIRefresh(1)
    local results = table.pack(xpcall(callback, debug.traceback))
    reaper.PreventUIRefresh(-1)
    if not results[1] then return nil, results[2] end
    return table.unpack(results, 2, results.n)
  end
end
