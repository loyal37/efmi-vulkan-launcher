# EFMI Vulkan Launcher - Project overview

> Created 2026-11-11. The structure follows `F:\efmi vulkan\.ai-memory`.

## Goal

A launcher of its own for Arknights: Endfield with EFMI Vulkan (`F:\efmi vulkan`, GitHub `loyal37/efmi-vulkan`): it starts the game on its Vulkan renderer and loads the EFMI Vulkan layer (`efmi_vk_inject.dll`) itself, so the user no longer edits three XXMI launcher settings to switch. It is also the front end of a future complete Vulkan loader for this game.

The user cares about how it looks: plain, mainstream taste, no screen full of explanations.

## Components and technology

- `core/`: C++17, MSVC x64. A static library (settings, XXMI prefill, install, launch and injection) and the command-line program `efvl_core.exe`. Only the core is elevated, and only when the game needs it.
- `shell/`: the window, a Win32 host of WebView2 (`EFMI Vulkan Launcher.exe`). Not elevated.
- `ui/`: HTML, CSS, JS shown in WebView2. All Chinese text in `ui/strings.zh-CN.json`.
- `scripts/`: build and checks. `tests/`: the checks' programs.
- A plain desktop program: no server, no Electron, no packaged Python.

## Entry points on this machine

- Project: `F:\EFMI Vulkan Launcher`. Dependencies: `F:\vulkan`.
- MSVC: `D:\Microsoft` (Build Tools 2022 17.14, Windows SDK 10.0.26100.0). Python: `D:\Python312\python.exe`. Git: `D:\git`.
- EFMI folder (read-only): `D:\XXMI-Launcher-Portable-v2.1.6\EFMI`. Game (read-only): `D:\Hypergryph Launcher\games\EndField Game\Endfield.exe` (x64, manifest `requireAdministrator`).
- XXMI launcher configuration (read-only): `D:\XXMI-Launcher-Portable-v2.1.6\XXMI-Launcher-Portable-v2.1.6\XXMI Launcher Config.json`.
- Sandbox program for injection tests: `F:\efmi vulkan\dist\efmi_vk_sandbox.exe`. Release archives of the layer: `F:\efmi vulkan\.worktrees\fix\build\release\EFMI-Vulkan-X.Y.Z.zip`.
- WebView2 runtime installed here: 157.0.4341.42.

## Long-term constraints

See `AGENTS.md`: only this project is modified; the real game is never started or injected by the agent; nothing against the anti-cheat; no system-wide state; sandbox results are not game results.
