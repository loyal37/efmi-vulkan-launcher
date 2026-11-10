# EFMI Vulkan Launcher - Decisions

## D-001 Technology (2026-11-11, proposed, the user did not object)

The interface is HTML/CSS/JS in the WebView2 that Windows ships; the shell and the core are C++ (MSVC). A plain desktop program: no network server, no Electron, no packaged Python interface. When the WebView2 runtime is missing the shell says so plainly, and the core still starts the game from the command line.

## D-002 The launcher injects by itself (user, 2026-11-11)

The user accepts the anti-cheat and account risk. The game no longer gets the layer through XXMI's injection, so the XXMI launcher's configuration is never changed. Only the plainest method: the process created suspended, a remote thread running `LoadLibraryW`, then resumed.

## D-003 What is read from XXMI/EFMI (2026-11-11)

Read-only: the EFMI folder's `Core`, `Mods` and `d3dx.ini` (by the layer), and on the first run the XXMI launcher configuration's `Importers.EFMI.Importer.importer_folder` and `game_folder` to prefill the settings.

## D-004 Elevation (2026-11-11)

`Endfield.exe` has a `requireAdministrator` manifest. The window is not elevated. The core is built `asInvoker`; when creating the game process answers ERROR_ELEVATION_REQUIRED it starts itself again elevated (`runas`, one UAC prompt) and hands the result back through a result file. Tests with the sandbox program need no elevation.

## D-005 Where the layer is installed (2026-11-11, agent, open to the user)

`<EFMI folder>\EFMI-Vulkan\`, the layout of the layer's release README, where its `m2_mods = auto` finds `Mods` and `Core\EFMI` without any edit of its configuration. An update replaces the package's files and keeps everything else there (`settings\`, the log); a configuration file the user changed is kept as `efmi_vk.cfg.previous`. The alternative, a folder of the launcher's with an absolute `m2_mods`, would leave the EFMI folder untouched but needs the configuration rewritten whenever the EFMI path changes.

## D-006 Where the launcher keeps its own data (2026-11-11)

`data\` next to the programs (`settings.json`, the WebView2 user data, logs): portable, as the user's XXMI. `--data DIR` (and `EFVL_DATA_DIR` for the shell) names another folder; the checks use one under `build/tests`.

## D-007 Dependencies (2026-11-11)

Under `F:\vulkan`: the WebView2 SDK (NuGet `Microsoft.Web.WebView2`, static loader, so no loader DLL ships), miniz (ZIP reading, MIT), nlohmann/json (MIT). Nothing else.

## D-008 Updates over the network (user, 2026-11-11)

The user: the launcher updates itself automatically as XXMI does, and so does EFMI Vulkan. Not in the first build, which installs from a local archive. Open: which repository publishes the launcher, whether `loyal37/efmi-vulkan` is public and has releases (see TASKS).

## Open, the user's to decide

- **O-1 DX11 from this launcher too?** It could call `XXMI Launcher.exe --nogui --xxmi EFMI`. The user's first motive was that switching between DX11 and Vulkan is tedious, but only the main button was specified.
- **O-2 The official launcher as the game's parent.** XXMI does not start `Endfield.exe` plainly: for installations outside Steam it starts the official launcher's `Games.exe` suspended and without a window, creates the game as that process's child, and ends `Games.exe` once the game runs (its source calls this the launcher requirement of the game; XXMI option `skip_platform_game_launcher`). Whether the game refuses a plain start is not known here: the agent does not start the game. Doing as XXMI does would present the game with a parent that is not the real one; under the rule "if the game blocks, stop and report" this is the user's decision, and it is not implemented.
