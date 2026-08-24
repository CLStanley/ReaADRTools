# Project Specific Rules - ReaADR Tools

## Architecture Context
The project follows a strict separation of concerns:
- **Native Layer (`extension/`)**: C++ logic for REAPER integration and non-Lua tasks (e.g., `ReaADR_ReadXlsxAsTsv`). Keep this layer thin.
- **Core Logic (`scripts/ReaADR_Core.lua`)**: The "Source of Truth". Handles state persistence, session models, data validation, and core utility functions.
- **App Layer (`scripts/ReaADR_App.lua`)**: Orchestrates UI behavior and routes menu interactions to appropriate logic.
- **Feature Scripts**: Thin wrappers (e.g., `ReaADR_Import_Cue_Sheet.lua`) that delegate work to Core or App layers.

## Development Rules
1.  **Source of Truth**: All modifications must ensure the `adr_session_model_v1` in project extstate remains synchronized with visible REAPER elements. 
2.  **Core first approach**: If a new feature requires data persistence, it must be implemented as an addition to `ReaADR_Core.lua` before being exposed via an App-layer UI.
3.  **Transaction Management**: Always wrap batch updates or riskier operations (like bulk cue processing) in transaction blocks to ensure "Undo" consistency.
4.  **Style**: 
    - Use Lua's standard convention for the scripts folder.
    - Follow the variable naming and table structure established in `ReaADR_Core_Persistence` and `ReaADR_Core_Transactions`.
5.  **Update Protocol**: When a user asks to "update" or "refresh" logic, check if the update applies to both the data model and the view layer (the sync pipeline).

## Task Handling
- For **New Features**, start by checking `ReaADR_Core.lua` to see if the required functionality already exists in a generic form before creating new scripts.
- For **Bug Fixes**, identify whether the bug is in the Data Model, the Rendering/Sync pipeline, or the UI logic.
