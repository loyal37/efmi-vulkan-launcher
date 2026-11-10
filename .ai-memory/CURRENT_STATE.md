# EFMI Vulkan Launcher - Current development state

## 0.1.0 in work (2026-11-11): core and first screen; waiting for the user's word on the main screen

- **Builds**: `python scripts/build.py` makes `dist/efvl_core.exe`, `dist/EFMI Vulkan Launcher.exe` and `dist/ui/`.
- **Sandbox** (`python tests/check_core.py`, 34 checks, all passing; the "game" is a copy of `efmi_vk_sandbox.exe` named Endfield.exe in a folder with Chinese characters, the layer the release archive 2.1.1, everything under `build/tests`):
  - settings saved and read back; prefill from a stand-in XXMI configuration, that file unchanged;
  - install from a local archive into the layer's folder (a setting): version shown, a cut archive, a path outside the folder, a missing DLL and a wrong `.sha256` refused with nothing written; an update keeps `settings\`; `m2_mods` of `efmi_vk.cfg` follows the EFMI path and every other line stays the package's; nothing written into the EFMI folder;
  - start: created suspended, `efmi_vk_inject.dll` loaded, resumed. The layer's own log: loaded into the program with the inject entry, "the Vulkan loader is not loaded yet", then 108 of 144 exports redirected without "already loaded", the Mods folder of the settings used, the program rendered and ended with 0.
- **Window**: renders the main screen and the settings drawer (`--screenshot`, pictures in `docs/preview/`). Shown to the user 2026-11-11; no answer yet.
- `python scripts/check_text.py` passes.

### Not verified

- **The real game: nothing.** No start, no injection, no elevation prompt. Whether the game accepts a plain start is open (DECISIONS O-2).
- The elevated path of the core (`runas`, result file): written, never run (the sandbox needs no elevation).
- The window's buttons (folder and file dialogs, install, start through `efvl_core.exe`): written, not clicked by a check or by hand.
- The message for a missing WebView2 runtime: written, not seen (the runtime is installed here).
- An EFMI path outside ASCII in the layer's `efmi_vk.cfg` (the launcher writes UTF-8; how the layer reads it was not checked). Whether release 2.0.9, the only one online, takes an absolute `m2_mods`.
- `check_core.py` passed on its first run; it was not seen failing against a wrong core first.

### Not done

- EFMI Vulkan from the network and the launcher's own update (D-008).
- The prefill finds the XXMI configuration through XXMI's shortcuts (`find_xxmi_config`); only the reading of a named file is checked.
