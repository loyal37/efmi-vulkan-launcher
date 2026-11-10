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
| `core/src` | Library: `settings` (data folder, JSON), `xxmi` (prefill), `install` (archive, installed version), `launch` (suspended start, injection, resume, elevation). `main.cpp`: the command line `efvl_core.exe`. |
| `shell/src` | The window: WebView2 host, the bridge between the page and the library, starts the core for a launch. |
| `ui/` | The page. `strings.zh-CN.json` holds every Chinese text, for the page and for the shell's native dialogs. |
| `scripts/`, `tests/` | Build, checks. |
