# Refresh comparison prototype implementation plan

Goal: two manually operated Qt 6 applications comparing QLabel and Qt Quick Text.
Architecture: shared C++ latest-value driver, precomputed string pool and bounded in-memory timing samples; separate frontends with equivalent fixed-size text cells.
Tech stack: Qt 6.5+, C++17, CMake.
Spec: approved discussion in this chat (10–80 ms, latest-value wins, manual testing).

User explicitly requested implementation first without automated tests. Work in this empty non-Git directory; no worktree/commits apply. Build verification is distinct from runtime tests; if Qt is unavailable, report that limitation.

- [x] Shared model/controller: validated setup, monotonic-clock batch selection, precise timer, precomputed text, bounded samples, stop at latest batch, CSV export.
- [x] Widgets frontend: fixed grid, scroll area, manual controls and statistics.
- [x] Qt Quick frontend: equivalent eagerly created fixed grid and manual controls/statistics.
- [x] Documentation: build/run instructions, metric definitions, limitations and manual comparison steps.

Review focus: invalid configuration, repeated start/stop, final latest value, sample capacity, offscreen objects, QML resource paths, no per-tick disk I/O.

Verification: both Release targets compiled successfully with installed Qt 6.10.2 MSVC2022_64 and MSVC 19.44. Windows SDK discovery required sandbox escalation. windeployqt completed for both targets. Read-only independent review found one no-op percentile issue, corrected by recording updated and excluding no-op submissions. No applications, automated tests or benchmarks were run, as requested. Runtime behavior remains unverified.
