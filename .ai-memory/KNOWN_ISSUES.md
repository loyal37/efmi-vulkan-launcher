# EFMI Vulkan Launcher - Known issues and findings

## F-001 What XXMI does when it starts the game (2026-11-11, read from its configuration, its log and its public source)

- Makes sure the game is closed.
- Outside Steam (the launcher does the same since D-011): ends a running `Games.exe`, then starts `<official launcher>\<version>\Games.exe` with `CREATE_SUSPENDED | CREATE_NO_WINDOW`, starts `Endfield.exe` as its child (working directory the game folder, normal priority), ends `Games.exe` when the game runs.
- Arguments: `-force-d3d11` in the DX11 configuration, none for Vulkan. `launch_options` is used only with `use_launch_options` (false here).
- `configure_game` and `window_mode` do nothing for EFMI: the EFMI package does not implement `configure_game_settings`.
- For DX11 only: rewrites `d3dx.ini` and optimises the Mods' INI files. Not the launcher's business.
- Injects by polling for the process name and running `LoadLibraryW` in a remote thread (`3dmloader.dll`), the process not suspended.

## F-002 The game (2026-11-11, read from the files)

`Endfield.exe` is x64 with a `requireAdministrator` manifest; the folder holds `AntiCheatExpert` and `ACE-ADVT.sys`.
