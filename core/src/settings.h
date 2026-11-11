// The launcher's own settings (data\settings.json) and what it reads from the XXMI launcher once.
#pragma once

#include "util.h"

namespace efvl {

inline constexpr wchar_t kGameExe[] = L"Endfield.exe";

struct Settings {
    std::string game_path; // the folder that holds Endfield.exe
    std::string efmi_path; // the EFMI folder: Mods and Core\EFMI
    std::string layer_path; // where EFMI Vulkan is installed; empty: EFMI-Vulkan beside the launcher
    std::string launcher_path; // the official launcher's folder; empty: looked for from the game folder
    bool prefilled = false; // the XXMI configuration was looked for once
};

// `named` when given, else data\ beside the program.
fs::path data_dir(const fs::path& named = fs::path());
Settings load_settings(const fs::path& data);
bool save_settings(const fs::path& data, const Settings& settings);

// The XXMI launcher's configuration file, found through the shortcuts it deploys (desktop, start
// menu). Empty when there is none. Nothing of XXMI is ever written.
fs::path find_xxmi_config();
// Fills the paths that are still empty from that file. False when it is missing or names neither.
bool prefill_from_xxmi(Settings& settings, const fs::path& config);

// The folder EFMI Vulkan is installed in: the setting, or its default.
fs::path layer_dir(const Settings& settings);

// The official launcher's Games.exe: the newest version folder of `folder` that holds one. Empty
// when there is none.
fs::path find_games_exe(const fs::path& folder);
// The official launcher's folder: the setting, or the folder two above the game's when it holds a
// Games.exe (<launcher>\games\EndField Game). Empty when neither does.
fs::path official_launcher_dir(const Settings& settings);
// A Steam installation is started plainly: it has no official launcher.
bool needs_official_launcher(const fs::path& game_folder);

bool game_folder_valid(const fs::path& folder);
bool efmi_folder_valid(const fs::path& folder);

// Everything the interface shows: settings, whether each path is usable, the installed layer.
json launcher_status(const fs::path& data);

} // namespace efvl
