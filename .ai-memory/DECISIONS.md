# EFMI Vulkan Launcher - Decisions

## D-001 Technology (2026-11-11, proposed, the user did not object)

The interface is HTML/CSS/JS in the WebView2 that Windows ships; the shell and the core are C++ (MSVC). A plain desktop program: no network server, no Electron, no packaged Python interface. When the WebView2 runtime is missing the shell says so plainly, and the core still starts the game from the command line.

## D-002 The launcher injects by itself (user, 2026-11-11)

The user accepts the anti-cheat and account risk. The game no longer gets the layer through XXMI's injection, so the XXMI launcher's configuration is never changed. Only the plainest method: the process created suspended, a remote thread running `LoadLibraryW`, then resumed.

## D-003 What is read from XXMI/EFMI (2026-11-11)

Read-only: the EFMI folder's `Core`, `Mods` and `d3dx.ini` (by the layer), and on the first run the XXMI launcher configuration's `Importers.EFMI.Importer.importer_folder` and `game_folder` to prefill the settings.

## D-004 Elevation (2026-11-11)

`Endfield.exe` has a `requireAdministrator` manifest. The window is not elevated. The core is built `asInvoker`; when creating the game process answers ERROR_ELEVATION_REQUIRED it starts itself again elevated (`runas`, one UAC prompt) and hands the result back through a result file. Tests with the sandbox program need no elevation.

## D-005 Where the layer is installed (user, 2026-11-11; replaces the agent's first choice, the EFMI folder)

The user: "install EFMI Vulkan as EFMI is installed, with a folder of one's own choosing". The install folder is a setting (`layer_path`), by default `EFMI-Vulkan` beside the launcher. Nothing is written into the EFMI folder. The launcher writes one value of the installed `efmi_vk.cfg`, `m2_mods = <EFMI path>\Mods` (absolute; the Core is its sibling `Core\EFMI`), again whenever the EFMI path changes; every other line of the package's file stays. An update replaces the package's files and keeps everything else in the folder (`settings\` with the Mods' saved states, `efmi_vk_layer.log`). The DLL injected is the one in that folder. The user's development build in `F:\efmi vulkan\dist` is another project's output and read-only; using it from the launcher is to be asked first.

## D-006 Where the launcher keeps its own data (2026-11-11)

`data\` next to the programs (`settings.json`, the WebView2 user data, logs): portable, as the user's XXMI. `--data DIR` (and `EFVL_DATA_DIR` for the shell) names another folder; the checks use one under `build/tests`.

## D-007 Dependencies (2026-11-11)

Under `F:\vulkan`: the WebView2 SDK (NuGet `Microsoft.Web.WebView2`, static loader, so no loader DLL ships), miniz (ZIP reading, MIT), nlohmann/json (MIT). Nothing else.

## D-008 Updates over the network (user, 2026-11-11)

- The user: the launcher updates itself automatically as XXMI does. Open: which repository publishes the launcher.
- The user: "yes, update over the network" for EFMI Vulkan. Both sources: the network and a local archive. `loyal37/efmi-vulkan` is public (no token); releases are tag `v<version>` with the asset `EFMI-Vulkan-<version>.zip`; the latest through `https://api.github.com/repos/loyal37/efmi-vulkan/releases/latest`.
- The user: no new release is published while the layer is in development, so GitHub stays at v2.0.9 (2.1.0 to 2.1.2 exist as local archives only). "No newer version" is the normal answer; on the user's machine the usual source is a local archive.
- Download only: nothing is uploaded, a request carries no user name or path, the User-Agent is a generic program name. Versions compare as three single digits. A download is checked (a whole archive that holds `efmi_vk_inject.dll`) before installed files are replaced; nothing is replaced while the game runs. Without network the message is short and the local archive still works. Checks never use the real network (a local stand-in server).

## D-009 Vulkan only (user, 2026-11-11: "no DX11")

The launcher has one path, Vulkan. No DX11 entry or option anywhere, and the XXMI launcher is never called.

## D-010 XXMI Launcher as the reference for flows (user, 2026-11-11)

Update, install and start flows may follow XXMI Launcher's (`SpectrumQT/XXMI-Launcher`, public): how the latest version is found and downloaded, checked and put in place, backup and rollback, the record of what is installed, finding the game folder, the order and timeouts of a start, how errors are told. Its repository shows no licence, so until one is confirmed its behaviour is the reference and its code and assets are not copied. The interface is plainer than XXMI's; the reference is the logic.

## Open, the user's to decide

- O-1 (DX11) is decided: D-009.
- **O-2 The official launcher as the game's parent.** XXMI does not start `Endfield.exe` plainly: for installations outside Steam it starts the official launcher's `Games.exe` suspended and without a window, creates the game as that process's child, and ends `Games.exe` once the game runs (its source calls this the launcher requirement of the game; XXMI option `skip_platform_game_launcher`). Whether the game refuses a plain start is not known here: the agent does not start the game. Doing as XXMI does would present the game with a parent that is not the real one; under the rule "if the game blocks, stop and report" this is the user's decision, and it is not implemented.
