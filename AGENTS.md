# EFMI Vulkan Launcher - working agreement

- When resuming work, read `.ai-memory/CURRENT_STATE.md` and `.ai-memory/TASKS.md` first; goal, architecture, constraints and earlier decisions are in the other memory documents.
- Modify only this project, `F:\EFMI Vulkan Launcher`. Dependencies the project needs installed (the WebView2 SDK and the like) go to `F:\vulkan`, the folder the user named for them. Read-only during development: the XXMI/EFMI installation (launcher configuration, `d3d11.dll`, `d3dx.ini`, `Core`, existing Mods), the game installation, and the projects `F:\efmi vulkan` and `F:\EFMI MCP`. The XXMI launcher's configuration is never changed, by the agent or by the product.
- "Install EFMI Vulkan" is a feature for the user. Developing and testing it uses temporary folders inside this project (`build/tests`), never the real EFMI folder.
- The agent does not start the real game and does not inject into it. The first real start is the user's click, and which account is used is the user's decision.
- Nothing is ever done to evade, hide from or fight the anti-cheat: only the plainest injection (a process created suspended, then a remote LoadLibrary). If the game blocks something, stop and report; do not look for a way around.
- The injection flow is tested with `F:\efmi vulkan\dist\efmi_vk_sandbox.exe` (a Vulkan test program, copied into `build/tests`), never the game. Windows opened by tests go on the secondary screen and do not take the focus.
- No system-wide state: no layer registration in the registry, no global environment variables.
- Keep "builds", "works in the sandbox" and "works in the real game" strictly apart. A sandbox result never stands in for the game.
- First a check that fails, then the change, then the checks of the touched area. Every verified change is committed. No remote is configured; nothing is pushed until the user names a repository.
- Before ending a round, update the current state, the tasks and the session log; a new decision or problem goes into its document as well. Record evidence and the scope of verification; do not paste long logs.
- Version numbers are three single digits that advance like a counter: 0.9.9 is followed by 1.0.0. No position holds 10 or more.
- Code, documents and commit messages are in English. The Chinese text of the interface lives in one file, `ui/strings.zh-CN.json`; `scripts/check_text.py` checks that no other file holds Chinese characters. Conversation with the user is in Chinese. What the user asks otherwise wins.
- The interface is plain and quiet (user): no sentence of explanation per control, no paragraphs, no internal state or jargon on screen. The main button reads "start game" in Chinese (`start` in the strings file); the interface never says "Vulkan start" or the like. A new screen is shown to the user before work spreads from it.
