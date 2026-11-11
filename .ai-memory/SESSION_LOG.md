# EFMI Vulkan Launcher - Session log

## 2026-11-11 Session 1

- Started from a session of the EFMI Vulkan project with the brief for the first build.
- Read the reference project's memory, release script and bootstrap; read XXMI's configuration, log and public source for what a start does (F-001). Found the `Games.exe` parent (O-2).
- The user, mid-session: the launcher itself updates automatically, as XXMI (D-008).
- Dependencies fetched; core sources begun (not compiled). Stopped by the usage limit before the first build; CURRENT_STATE lists what is missing.
- Resumed. The user through the starting session: no DX11 (D-009), EFMI Vulkan from the network too (D-008), no new releases for now, the layer's folder a setting (D-005), XXMI Launcher as the reference for flows (D-010).
- Core built: launch, settings, prefill, install, actions, command line. `tests/check_core.py` 34/34 in the sandbox. Commit a8c606f.
- Window and page built, screenshots sent to the user. Commits 303b7d7, 0a8d2ea.
- Open at the end: the user's word on the main screen, O-2, network install and update.
- The user accepted the main screen. Network install and update built (`core/src/update.*`, `tests/check_update.py` 23/23 after failing), tried once against GitHub (v2.0.9, sandbox). The window got the update button and a script hook for checks (`tests/check_ui.py` 16/16).
- The user: push the launcher to a public repository; asked whether the official launcher's wallpaper can be had (not a local file; open).
- The user: injection first, wallpaper and releases set aside; then "do it XXMI's way" for the parent (D-011). Built: early exit, launcher.log, the Games.exe parent, the official launcher's folder as a setting; `tests/check_inject.py` 20/20, the other three checks passing again (35, 23, 16). Read-only prefill on this machine found all three folders.
