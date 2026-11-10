# EFMI Vulkan Launcher - Architecture

> Verified state and versions are in CURRENT_STATE.md, never here.

## How the layer enters the game

`efmi_vk_inject.dll` is loaded with `LoadLibraryW`. Nothing calls it; it registers a loader notification and, when `vulkan-1.dll` is mapped, redirects that module's export entries in memory (`F:\efmi vulkan\layer\src\bootstrap.cpp`). It must therefore be in the process before the game loads the Vulkan loader: the launcher creates the process suspended, loads the DLL from a remote thread and only then resumes. In the layer's log an early load reads "the Vulkan loader is not loaded yet, waiting for it" and then "N of M exports redirected" without "it was already loaded".

The layer reads `efmi_vk.cfg` from its own folder; `m2_mods = auto` takes the first folder upward that holds `Mods` and `Core\EFMI`. Its log is `efmi_vk_layer.log` beside it (`EFMI_VK_LOG` names another file).

## The layer's release

`EFMI-Vulkan-X.Y.Z.zip` with a `.sha256` beside it, one folder `EFMI-Vulkan`: `efmi_vk_inject.dll`, `dxcompiler.dll`, `efmi-vkd3d-shader.dll`, `SPIRV-Tools-shared.dll`, `efmi_vk.cfg`, `README.txt`, `LICENSE.txt`, `THIRD_PARTY_NOTICES.md`, `licenses\`. Built by `F:\efmi vulkan\scripts\build.py release`. The DLL holds the text `efmi_vk_layer %s%s loaded into` and its version as a string.

## Parts

| Part | What |
| --- | --- |
| `core/src/launch.*` | `launch()`: CreateProcessW suspended, per DLL a remote thread running LoadLibraryW, ResumeThread; any failure ends the process that never ran. `process_running()`. |
| `core/src/settings.*` | `data\settings.json` (`game_path`, `efmi_path`, `layer_path`, `prefilled`), the XXMI prefill, `launcher_status()` (what the page shows). |
| `core/src/install.*` | The archive read whole in memory (miniz), checked, then written; the install record `efvl-install.json` (version from the archive's name); `set_layer_mods()` writes `m2_mods`. |
| `core/src/actions.*` | What the command line and the window both call: prefill, set paths, install, start. Every answer is JSON with `ok` and `code`. |
| `core/src/main.cpp` | `efvl_core.exe`. Starts itself elevated when the program needs it; the elevated copy answers through `--result`. |
| `shell/src/main.cpp` | The window: WebView2 on the virtual host `efvl.example` mapped to `ui\`, requests `{id, cmd}` from the page (`state`, `browse`, `install_file`, `start`), native folder and file dialogs, `--screenshot`. A start runs `efvl_core.exe start` hidden. |
| `ui/` | `index.html`, `app.css`, `app.js`; `strings.zh-CN.json` holds every Chinese text, error messages by code, and the shell's native message. |
| `scripts/build.py`, `scripts/check_text.py`, `tests/check_core.py` | Build and checks. |
