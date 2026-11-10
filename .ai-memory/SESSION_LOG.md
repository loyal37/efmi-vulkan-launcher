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
