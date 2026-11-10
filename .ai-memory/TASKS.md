# EFMI Vulkan Launcher - Tasks

## First build (0.1.0)

- [ ] Core: start suspended, inject, resume; checked against the sandbox program.
- [ ] Settings: EFMI path and game path saved and read back; first-run prefill from the XXMI configuration (read-only).
- [ ] Install EFMI Vulkan from a local release archive; installed version shown; update.
- [ ] Shell and main screen; shown to the user before anything spreads from it.

## Waiting for the user

- [ ] O-2: the official launcher's `Games.exe` as the game's parent, as XXMI does?
- [ ] D-008: which repository publishes the launcher itself?
- [ ] The first real start of the game (the user's click, the user's account).

## Next

- [ ] EFMI Vulkan from the network: latest release, download, check, install (D-008).
- [ ] Automatic update of the launcher itself (D-008; needs its repository).

## Later milestones, not the first build

- [ ] Check before the start: which Mods will show, which are left out and why. Needs an offline check program shipped with the layer's release (today only the development tool `efmi_vk_character_test.exe --plan-folder-states`).
- [ ] The layer's running state shown.
- [ ] Game version check.
