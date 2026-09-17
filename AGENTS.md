# Project Specific Rules - ReaADR Tools

## Architecture Context
The project is incrementally migrating to an entirely C++ REAPER extension. Follow `docs/CPP_MIGRATION.md` and preserve these separation-of-concerns boundaries:
- **Native Domain Core (`extension/reaadr_core/`)**: C++ model and workflow rules that do not include REAPER SDK headers.
- **Native REAPER Layer (`extension/`)**: C++ commands, project adapters, transactions, UI, and REAPER integration.
- **Transitional Core Logic (`scripts/ReaADR_Core.lua`)**: The current implementation for features that have not yet moved to C++.
- **App Layer (`scripts/ReaADR_App.lua`)**: Orchestrates UI behavior and routes menu interactions to appropriate logic.
- **Feature Scripts**: Transitional thin wrappers that delegate to Core or App until their feature is cut over to C++.

## Development Rules
1. **Source of Truth**: All modifications must ensure the `adr_session_model_v1` in project extstate remains synchronized with visible REAPER elements.
2. **Core first approach**: Implement new persistence and workflow rules in the REAPER-independent C++ domain core before exposing them through native commands or UI. Do not add new Lua implementations unless they are required to maintain a feature that has not yet been migrated.
3. **Transaction Management**: Always wrap batch updates or riskier operations (like bulk cue processing) in transaction blocks to ensure "Undo" consistency.
4. **Style**:
   - Use Lua's standard convention for the scripts folder.
   - Follow the variable naming and table structure established in `ReaADR_Core_Persistence` and `ReaADR_Core_Transactions`.
   - Add concise comments for public interfaces, compatibility constraints, ownership rules, transaction boundaries, and non-obvious algorithms.
   - Comments should explain intent and tradeoffs—especially *why* behavior exists—not restate syntax. Keep comments synchronized when behavior changes.
5. **Update Protocol**: When a user asks to "update" or "refresh" logic, check if the update applies to both the data model and the view layer (the sync pipeline).

## Verification and Completion Rules
These rules are mandatory for local agents, cloud agents, and interactive coding assistants.

1. **Never claim unverified success.** Do not say or imply that a change "works", "is fixed", "passes", "builds", "is complete", or is otherwise successful unless you actually ran the corresponding verification during the current task and observed it succeed.
   - Code inspection, reasoning, or a clean-looking implementation is not a substitute for execution.
   - A previously green CI run does not verify a new change.
   - If the environment prevents verification, say exactly what was not run and why. Describe the change as implemented but unverified rather than successful.
2. **Run the verification that matches the change.** Use the narrowest relevant check while iterating, then run the repository's appropriate final build/test checks before declaring completion when the environment permits it.
   - Domain/core behavior changes should run the relevant native tests.
   - Extension, REAPER integration, build-system, or UI changes should run the applicable extension build/tests.
   - Cross-platform-sensitive changes are not considered cross-platform verified unless the relevant platform builds/tests or CI jobs actually ran successfully.
   - REAPER runtime/UI behavior is not considered verified merely because it compiles; when an actual REAPER smoke test cannot be run, state that limitation explicitly.
3. **Inspect the final diff before stopping.** After the last edit and before reporting completion, inspect the complete final diff against the intended base/current task scope. Check for accidental edits, debug code, truncated files, generated artifacts, unrelated formatting churn, stale comments/docs, and missing pieces of the requested change.
4. **Check repository state before and after editing.** Before making changes, inspect the working tree/status so pre-existing user changes are not overwritten or misattributed. Before stopping, inspect status again and account for every changed/untracked file relevant to the task.
5. **Do not silently broaden scope.** Make the smallest coherent change that satisfies the task. Preserve existing architecture and behavior outside that scope. If a newly discovered problem is not required to complete the task, report it separately instead of opportunistically rewriting adjacent code.
6. **Do not hide failures.** If a build, test, lint, command, or CI job fails, report the failure accurately and investigate failures caused by the current change. Never summarize a partially failing verification set as passing.
7. **Keep evidence with the conclusion.** A completion report should state what changed, exactly what verification was run and its result, and any verification that remains outstanding. Use concrete command names, test suites, CI jobs, or runtime checks rather than vague statements such as "tested thoroughly."

## Agent Workflow
Unless the task explicitly requires a different process, use this sequence:
1. Inspect repository status and relevant architecture/docs/code before editing.
2. Identify the smallest coherent implementation slice and the verification needed to prove it.
3. Make focused changes without overwriting unrelated user work.
4. Run targeted verification while iterating.
5. Run the appropriate final verification available in the current environment.
6. Inspect the final diff and repository status.
7. Only then report completion, clearly separating verified behavior from anything that still requires REAPER, another OS, CI, or user testing.

## Task Handling
- For **New Features**, check the C++ domain core first, then inspect `ReaADR_Core.lua` for transitional behavior that must be ported or kept compatible.
- For **Bug Fixes**, identify whether the bug is in the Data Model, the Rendering/Sync pipeline, or the UI logic.
- For **Migration Work**, treat the Lua implementation as a behavior/parity reference while moving real workflow ownership to C++. Preserve the canonical session model, ownership/deletion boundaries, undo/rollback semantics, and the host-provided SWELL/modstub architecture.
