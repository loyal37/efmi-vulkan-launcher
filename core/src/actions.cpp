#include "actions.h"

#include "install.h"
#include "launch.h"
#include "settings.h"

#include <cstdio>

namespace efvl {

namespace {

json fail(const char* code)
{
    return {{"ok", false}, {"code", code}};
}

// The layer's configuration follows the EFMI path whenever either changes.
// One line per start in data\launcher.log: when, what was started with which DLL, how it ended.
void log_start(const fs::path& data, const LaunchRequest& request, const json& answer)
{
    SYSTEMTIME now;
    GetLocalTime(&now);
    char stamp[64];
    snprintf(stamp, sizeof(stamp), "%04u-%02u-%02u %02u:%02u:%02u", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute,
             now.wSecond);
    std::string text;
    read_file(data / L"launcher.log", text);
    if (text.size() > (1u << 20))
        text.erase(0, text.size() - (1u << 19));
    text += std::string(stamp) + " start " + text_of(request.exe) + " with " +
            (request.dlls.empty() ? std::string("no DLL") : text_of(request.dlls.front())) +
            (request.parent_exe.empty() ? std::string() : " as a child of " + text_of(request.parent_exe)) + ": " + dump(answer) + "\n";
    write_file(data / L"launcher.log", text);
}

void sync_layer(const Settings& s)
{
    const fs::path efmi = path_of(s.efmi_path);
    if (efmi_folder_valid(efmi))
        set_layer_mods(layer_dir(s), efmi);
}

} // namespace

json action_prefill(const fs::path& data, const fs::path& xxmi_config)
{
    Settings s = load_settings(data);
    bool filled = false;
    if (!s.prefilled || !xxmi_config.empty()) {
        const fs::path config = xxmi_config.empty() ? find_xxmi_config() : xxmi_config;
        if (!config.empty())
            filled = prefill_from_xxmi(s, config);
        s.prefilled = true;
        if (!save_settings(data, s))
            return fail("settings_write_failed");
        sync_layer(s);
    }
    return {{"ok", true}, {"code", "ok"}, {"filled", filled}};
}

json action_set_paths(const fs::path& data, const json& paths)
{
    Settings s = load_settings(data);
    auto take = [&](const char* key, std::string& into) {
        if (paths.contains(key) && paths[key].is_string())
            into = paths[key].get<std::string>();
    };
    take("game_path", s.game_path);
    take("efmi_path", s.efmi_path);
    take("layer_path", s.layer_path);
    take("launcher_path", s.launcher_path);
    if (!save_settings(data, s))
        return fail("settings_write_failed");
    sync_layer(s);
    return {{"ok", true}, {"code", "ok"}};
}

json action_install(const fs::path& data, const fs::path& archive, const std::string& source)
{
    const Settings s = load_settings(data);
    const InstallResult result = install_archive(archive, layer_dir(s), source);
    if (result.code == "ok")
        sync_layer(s);
    return result.to_json();
}

json action_start(const fs::path& data, bool wait, const std::wstring& args,
                  const std::vector<std::pair<std::wstring, std::wstring>>& env, DWORD settle_ms)
{
    const Settings s = load_settings(data);
    const fs::path game = path_of(s.game_path), efmi = path_of(s.efmi_path), layer = layer_dir(s);
    if (!game_folder_valid(game))
        return fail("game_not_set");
    // Outside Steam the game is started as the official launcher's child, as XXMI starts it.
    fs::path parent;
    if (needs_official_launcher(game)) {
        parent = find_games_exe(official_launcher_dir(s));
        if (parent.empty())
            return fail("launcher_not_set");
    }
    if (!efmi_folder_valid(efmi))
        return fail("efmi_not_set");
    std::error_code ec;
    if (!fs::is_regular_file(layer / kLayerDll, ec))
        return fail("layer_not_installed");
    if (process_running(kGameExe))
        return fail("game_running");
    set_layer_mods(layer, efmi);

    LaunchRequest request;
    request.exe = game / kGameExe;
    request.cwd = game;
    request.dlls.push_back(layer / kLayerDll);
    request.args = args;
    request.env = env;
    request.wait = wait;
    request.settle_ms = settle_ms;
    request.parent_exe = parent;
    const json answer = launch(request).to_json();
    log_start(data, request, answer);
    return answer;
}

} // namespace efvl
