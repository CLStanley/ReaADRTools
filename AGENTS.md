# Project Specific Rules - ReaADR Tools

## Architecture Context
The project is incrementally migrating to an entirely C++ REAPER extension. Follow `docs/CPP_MIGRATION.md` and preserve these separation-of-concerns boundaries:
- **Native Domain Core (`extension/reaadr_core/`)**: C++ model and workflow rules that do not include REAPER SDK headers.
- **Native REAPER Layer (`extension/`)**: C++ commands, project adapters, transactions, UI, and REAPER integration.
- **Transitional Core Logic (`scripts/ReaADR_Core.lua`)**: The current implementation for features that have not yet moved to C++.
- **App Layer (`scripts/ReaADR_App.lua`)**: Orchestrates UI behavior and routes menu interactions to appropriate logic.
- **Feature Scripts**: Transitional thin wrappers that delegate to Core or App until their feature is cut over to C++.

## Development Rules
1.  **Source of Truth**: All modifications must ensure the `adr_session_model_v1` in project extstate remains synchronized with visible REAPER elements. 
2.  **Core first approach**: Implement new persistence and workflow rules in the REAPER-independent C++ domain core before exposing them through native commands or UI. Do not add new Lua implementations unless they are required to maintain a feature that has not yet been migrated.
3.  **Transaction Management**: Always wrap batch updates or riskier operations (like bulk cue processing) in transaction blocks to ensure "Undo" consistency.
4.  **Style**: 
    - Use Lua's standard convention for the scripts folder.
    - Follow the variable naming and table structure established in `ReaADR_Core_Persistence` and `ReaADR_Core_Transactions`.
    - Add concise comments for public interfaces, compatibility constraints, ownership rules, transaction boundaries, and non-obvious algorithms.
    - Comments should explain intent and tradeoffs—especially *why* behavior exists—not restate syntax. Keep comments synchronized when behavior changes.
5.  **Update Protocol**: When a user asks to "update" or "refresh" logic, check if the update applies to both the data model and the view layer (the sync pipeline).

## Task Handling
- For **New Features**, check the C++ domain core first, then inspect `ReaADR_Core.lua` for transitional behavior that must be ported or kept compatible.
- For **Bug Fixes**, identify whether the bug is in the Data Model, the Rendering/Sync pipeline, or the UI logic.
