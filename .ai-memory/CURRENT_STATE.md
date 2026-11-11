# EFMI Vulkan Launcher - Current development state

## 0.1.0 in work (2026-11-11): core and first screen; waiting for the user's word on the main screen

- **Builds**: `python scripts/build.py` makes `dist/efvl_core.exe`, `dist/EFMI Vulkan Launcher.exe` and `dist/ui/`.
- **Sandbox** (`python tests/check_core.py`, 34 checks, all passing; the "game" is a copy of `efmi_vk_sandbox.exe` named Endfield.exe in a folder with Chinese characters, the layer the release archive 2.1.1, everything under `build/tests`):
  - settings saved and read back; prefill from a stand-in XXMI configuration, that file unchanged;
  - install from a local archive into the layer's folder (a setting): version shown, a cut archive, a path outside the folder, a missing DLL and a wrong `.sha256` refused with nothing written; an update keeps `settings\`; `m2_mods` of `efmi_vk.cfg` follows the EFMI path and every other line stays the package's; nothing written into the EFMI folder;
  - start: created suspended, `efmi_vk_inject.dll` loaded, resumed. The layer's own log: loaded into the program with the inject entry, "the Vulkan loader is not loaded yet", then 108 of 144 exports redirected without "already loaded", the Mods folder of the settings used, the program rendered and ended with 0.
- **Start, edges** (`python tests/check_inject.py`, 20 checks, the new ones failing before each change): game, EFMI and layer folders named outside ASCII (the layer read the EFMI path from `efmi_vk.cfg` as UTF-8); a program that ends at once is `exited_early` with its exit code; a running one refuses a second start; the game is the child of the stand-in official launcher's newest `Games.exe`, which is gone afterwards; a Steam path starts without one; no official launcher is `launcher_not_set`; a file that is no DLL (alone or second) and a 32-bit program are refused with no process left; a program with a `requireAdministrator` manifest answers `elevation_required` (740); `launcher.log` records the starts.
- **This machine, read-only (2026-11-11)**: `efvl_core prefill` into a test data folder found the XXMI configuration through its shortcut and took the game folder, the EFMI folder and `D:\Hypergryph Launcher` (a `Games.exe` found there). Nothing was started.
- **Network** (`python tests/check_update.py`, 23 checks, failing before the change; a stand-in server on this machine): the latest release offered and installed, the same and an older version no update, a newer one installed with `settings\\` kept, a damaged download refused with the installed layer untouched, an error answer and no server told as `network_failed` within seconds, a local archive still installing. Every request carried the User-Agent `EFMI-Vulkan-Launcher` and no header beyond the plain ones.
- **Real network, by hand, once (2026-11-11)**: `efvl_core update` against GitHub took v2.0.9 into `build/tests/online-209`; started in the sandbox it loaded with the absolute `m2_mods` (2.0.9 takes it).
- **Window** (`python tests/check_ui.py`, 16 checks; the real program, its page's buttons clicked by a script, shown without taking the focus): the main button leads to the settings while the layer is missing, the install button installs from the stand-in server and the version shows, a second update says nothing is newer, the main button then starts the stand-in game through `efvl_core.exe` and the layer's log shows the early load. Pictures in `docs/preview/`. The user accepted the main screen (2026-11-11).
- `python scripts/check_text.py` passes.
- Pushed to the public repository `loyal37/efmi-vulkan-launcher` at the user's word (2026-11-11).

### Not verified

- **The real game: nothing.** No start, no injection, no elevation prompt. Whether the game accepts the start as the child of a suspended `Games.exe` is known only from XXMI doing so; the launcher's own first start is the user's.
- The elevated path of the core (`runas`, result file, injection from the elevated copy): `python tests/check_inject.py --elevated` runs it against a stand-in that requires administrator rights; it needs one UAC prompt accepted and has not been run. The real `Games.exe` created by the core: not tried (whether it needs elevation itself is unknown; the core would elevate then).
- The native folder and file dialogs of the window (choosing a path, the local archive): written, not exercised.
- The message for a missing WebView2 runtime: written, not seen (the runtime is installed here).
-
- `check_core.py` passed on its first run; it was not seen failing against a wrong core first.

### Not done

- The launcher's own automatic update (D-008): no release of the launcher exists yet.
- The official launcher's wallpaper (user, 2026-11-11): not a file on this machine; see TASKS.
