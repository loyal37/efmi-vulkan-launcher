// What the launcher does, as the command line and the window both call it. Every action answers
// with JSON: {"ok": bool, "code": text, ...}; the interface turns a code into its message.
#pragma once

#include "util.h"

#include <utility>
#include <vector>

namespace efvl {

// Looks for the XXMI launcher's configuration once and fills empty paths from it.
json action_prefill(const fs::path& data, const fs::path& xxmi_config = fs::path());
// Saves the paths given (a null member leaves that one as it is) and points the installed layer
// at the EFMI folder.
json action_set_paths(const fs::path& data, const json& paths);
// Installs a local archive into the layer's folder.
json action_install(const fs::path& data, const fs::path& archive, const std::string& source = "local");
// Starts the game with the layer loaded. Codes beside launch()'s: game_not_set, efmi_not_set,
// layer_not_installed, game_running. `args` are appended to the game's command line; `env` sets
// variables for the game's process only (the checks use both, the window neither).
json action_start(const fs::path& data, bool wait, const std::wstring& args = std::wstring(),
                  const std::vector<std::pair<std::wstring, std::wstring>>& env = {});

} // namespace efvl
