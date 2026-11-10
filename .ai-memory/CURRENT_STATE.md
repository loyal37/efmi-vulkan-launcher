# EFMI Vulkan Launcher - Current development state

## 2026-11-11: project started

- Repository initialised (branch main, no remote), working agreement and project memory written.
- Read, nothing changed: how the layer enters the game and how its release is packed (`F:\efmi vulkan`), what XXMI does at a start (KNOWN_ISSUES F-001).
- Nothing built yet.

## 2026-11-11: core sources begun, nothing built (session cut short by the usage limit)

- Dependencies unpacked under `F:\vulkan`: `webview2-1.0.2903.40` (include and x64 static loader), `miniz-3.0.2`, `nlohmann-json-3.11.3\json.hpp`.
- Written, never compiled: `core/src/util.*`, `launch.*` (suspended start, remote LoadLibraryW, resume), `settings.*` (settings, XXMI prefill through its shortcuts, status), `install.h` (interface only).
- Missing: `core/src/install.cpp` (miniz, `.sha256` check, record `efvl-install.json`), `core/src/main.cpp` (command line, self-elevation on ERROR_ELEVATION_REQUIRED), `scripts/build.py`, `scripts/check_text.py`, the sandbox injection check (`tests/`), the shell, the page, the strings file.
- Not verified: anything. No build, no sandbox run, no game.
