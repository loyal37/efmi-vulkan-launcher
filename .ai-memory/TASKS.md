# EFMI Vulkan Launcher - Tasks

## First build (0.1.0)

- [x] Core: start suspended, inject, resume; checked against the sandbox program.
- [x] Settings: EFMI path and game path saved and read back; first-run prefill from the XXMI configuration (read-only).
- [x] Install EFMI Vulkan from a local release archive; installed version shown; update.
- [x] Shell and main screen built and shown.
- [x] The user accepted the main screen (2026-11-11).
- [x] A check that clicks the page's buttons (`tests/check_ui.py`).
- [ ] The elevated path tried with a program that needs it.

## Waiting for the user

- [ ] O-2: the official launcher's `Games.exe` as the game's parent, as XXMI does?
- [ ] Background: the official launcher's wallpaper is fetched by it from Hypergryph's servers, not kept as a file here. Fetch it the same way at run time, or let the user choose a picture? Never shipped in the repository.
- [ ] The first real start of the game (the user's click, the user's account).

## Next

- [x] EFMI Vulkan from the network: latest release, download, check, install (D-008).
- [ ] Automatic update of the launcher itself (D-008): releases in `loyal37/efmi-vulkan-launcher`, a packaging script, the replace-while-running step.

## Later milestones, not the first build

- [ ] Check before the start: which Mods will show, which are left out and why. Needs an offline check program shipped with the layer's release (today only the development tool `efmi_vk_character_test.exe --plan-folder-states`).
- [ ] The layer's running state shown.
- [ ] Game version check.
