# Migration Priorities

ReaADR Tools is not considered a supported runtime while the Lua-to-C++ migration is in progress. Migration completeness and native parity take priority over preserving transitional Lua behavior.

## Working rules

1. Treat the Lua implementation as the behavioral and UI reference, not as a runtime that must remain production-safe during migration.
2. Prefer completing native C++ ownership of a workflow over maintaining duplicate Lua/C++ implementations.
3. Do not spend migration time on cleanup, architectural polish, or new features unless required to complete native parity safely.
4. Preserve the canonical `adr_session_model_v1` data contract and ownership/transaction safety rules during the migration.
5. Keep temporary compatibility code only when it materially helps compare behavior or complete the migration. Remove redundant transitional code during the dedicated cleanup phase after parity.
6. The Cue Manager is the highest-priority user-facing surface. Its native window, behavior, editing, navigation, recording integration, Cue Info integration, docking, layout, and cross-platform parity take precedence over lower-visibility migration work.
7. Windows, Linux, and macOS are target platforms. A native UI workflow is not considered fully migrated while a supported platform still falls back to a reduced compatibility presentation.

## Migration order

1. Complete Cue Manager UI and workflow parity.
2. Complete native recording and Cue Info integration from the Manager and standalone commands.
3. Complete remaining session, import, filtering, generation, overlay, and report workflows in C++.
4. Remove Lua workflow ownership once native equivalents are complete enough for parity testing.
5. Run full cross-platform parity and regression testing.
6. Begin the cleanup/refactor phase only after migration and parity are substantially complete.
7. Add missing/new product features after the migrated baseline is stable.
