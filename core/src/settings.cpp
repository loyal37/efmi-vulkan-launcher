#include "settings.h"

#include "install.h"

#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <vector>

namespace efvl {

fs::path data_dir(const fs::path& named)
{
    return named.empty() ? program_dir() / L"data" : named;
}

Settings load_settings(const fs::path& data)
{
    Settings s;
    const json j = read_json(data / L"settings.json");
    if (j.is_object()) {
        s.game_path = j.value("game_path", std::string());
        s.efmi_path = j.value("efmi_path", std::string());
        s.layer_path = j.value("layer_path", std::string());
        s.launcher_path = j.value("launcher_path", std::string());
        s.prefilled = j.value("prefilled", false);
    }
    return s;
}

bool save_settings(const fs::path& data, const Settings& s)
{
    const json j = {{"game_path", s.game_path}, {"efmi_path", s.efmi_path}, {"layer_path", s.layer_path}, {"launcher_path", s.launcher_path}, {"prefilled", s.prefilled}};
    return write_file(data / L"settings.json", j.dump(2, ' ', false, json::error_handler_t::replace) + "\n");
}

fs::path layer_dir(const Settings& settings)
{
    return settings.layer_path.empty() ? program_dir() / kLayerFolder : path_of(settings.layer_path);
}

fs::path find_games_exe(const fs::path& folder)
{
    fs::path best;
    std::vector<int> best_version;
    std::error_code ec;
    if (folder.empty())
        return best;
    for (fs::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec)) {
        const fs::path exe = it->path() / L"Games.exe";
        if (!it->is_directory(ec) || !fs::is_regular_file(exe, ec))
            continue;
        // Folders are version numbers (1.6.0); each part compared as a number.
        std::vector<int> version;
        for (const wchar_t* p = it->path().filename().c_str(); *p;) {
            wchar_t* next = nullptr;
            version.push_back(static_cast<int>(wcstol(p, &next, 10)));
            if (next == p)
                break;
            p = *next == L'.' ? next + 1 : next + wcslen(next);
        }
        if (best.empty() || version > best_version) {
            best = exe;
            best_version = version;
        }
    }
    return best;
}

fs::path official_launcher_dir(const Settings& settings)
{
    if (!settings.launcher_path.empty())
        return path_of(settings.launcher_path);
    const fs::path guess = path_of(settings.game_path).parent_path().parent_path();
    return !settings.game_path.empty() && !find_games_exe(guess).empty() ? guess : fs::path();
}

bool needs_official_launcher(const fs::path& game_folder)
{
    for (const fs::path& part : game_folder)
        if (_wcsicmp(part.c_str(), L"steamapps") == 0)
            return false;
    return true;
}

bool game_folder_valid(const fs::path& folder)
{
    std::error_code ec;
    return !folder.empty() && fs::is_regular_file(folder / kGameExe, ec);
}

bool efmi_folder_valid(const fs::path& folder)
{
    std::error_code ec;
    return !folder.empty() && fs::is_directory(folder / L"Mods", ec) && fs::is_directory(folder / L"Core" / L"EFMI", ec);
}

namespace {

// The file a shortcut points at, or empty.
fs::path shortcut_target(IShellLinkW* link, IPersistFile* file, const fs::path& shortcut)
{
    if (FAILED(file->Load(shortcut.c_str(), STGM_READ)))
        return fs::path();
    wchar_t target[MAX_PATH * 4] = {};
    if (FAILED(link->GetPath(target, static_cast<int>(sizeof(target) / sizeof(target[0])), nullptr, SLGP_RAWPATH)))
        return fs::path();
    return fs::path(target);
}

fs::path known_folder(REFKNOWNFOLDERID id)
{
    PWSTR raw = nullptr;
    fs::path out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &raw)))
        out = raw;
    CoTaskMemFree(raw);
    return out;
}

} // namespace

fs::path find_xxmi_config()
{
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    fs::path found;
    IShellLinkW* link = nullptr;
    IPersistFile* file = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link))) &&
        SUCCEEDED(link->QueryInterface(IID_PPV_ARGS(&file)))) {
        const fs::path roots[] = {known_folder(FOLDERID_Desktop), known_folder(FOLDERID_PublicDesktop),
                                  known_folder(FOLDERID_Programs), known_folder(FOLDERID_CommonPrograms)};
        std::error_code ec;
        for (const fs::path& root : roots) {
            if (root.empty() || !found.empty())
                continue;
            for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
                 !ec && it != end && found.empty(); it.increment(ec)) {
                if (it.depth() > 2) {
                    it.disable_recursion_pending();
                    continue;
                }
                if (_wcsicmp(it->path().extension().c_str(), L".lnk"))
                    continue;
                const fs::path target = shortcut_target(link, file, it->path());
                if (_wcsicmp(target.filename().c_str(), L"XXMI Launcher.exe"))
                    continue;
                // The program is in Resources\Bin of the launcher's folder; the configuration at its top.
                fs::path folder = target.parent_path();
                for (int up = 0; up < 3 && found.empty(); up++, folder = folder.parent_path()) {
                    const fs::path config = folder / L"XXMI Launcher Config.json";
                    if (fs::is_regular_file(config, ec))
                        found = config;
                }
            }
        }
    }
    if (file)
        file->Release();
    if (link)
        link->Release();
    if (SUCCEEDED(init))
        CoUninitialize();
    return found;
}

bool prefill_from_xxmi(Settings& settings, const fs::path& config)
{
    const json j = read_json(config);
    if (!j.is_object())
        return false;
    const json::json_pointer at("/Importers/EFMI/Importer");
    if (!j.contains(at) || !j[at].is_object())
        return false;
    const json& importer = j[at];
    bool any = false;
    auto take = [&](const char* key, std::string& into, bool (*valid)(const fs::path&)) {
        if (!into.empty() || !importer.contains(key) || !importer[key].is_string())
            return;
        fs::path folder = path_of(importer[key].get<std::string>());
        if (folder.empty())
            return;
        // XXMI keeps a folder of its own relative to its launcher's folder ("EFMI/").
        if (folder.is_relative())
            folder = config.parent_path() / folder;
        std::error_code ec;
        folder = fs::weakly_canonical(folder, ec).make_preferred();
        if (ec || !valid(folder))
            return;
        into = text_of(folder);
        any = true;
    };
    take("game_folder", settings.game_path, game_folder_valid);
    take("importer_folder", settings.efmi_path, efmi_folder_valid);
    take("launcher_folder", settings.launcher_path, [](const fs::path& folder) { return !find_games_exe(folder).empty(); });
    return any;
}

json launcher_status(const fs::path& data)
{
    const Settings s = load_settings(data);
    const fs::path game = path_of(s.game_path), efmi = path_of(s.efmi_path);
    const bool game_ok = game_folder_valid(game), efmi_ok = efmi_folder_valid(efmi);
    json layer = layer_state(layer_dir(s));
    const fs::path official = official_launcher_dir(s);
    const bool launcher_needed = needs_official_launcher(game);
    const bool launcher_ok = !launcher_needed || !find_games_exe(official).empty();
    const bool ready = game_ok && efmi_ok && launcher_ok && layer.value("installed", false);
    return {{"version", EFVL_VERSION},
            {"game_path", s.game_path},
            {"efmi_path", s.efmi_path},
            {"game_ok", game_ok},
            {"layer_path", text_of(layer_dir(s))},
            {"efmi_ok", efmi_ok},
            {"launcher_path", text_of(official)},
            {"launcher_needed", launcher_needed},
            {"launcher_ok", launcher_ok},
            {"layer", layer},
            {"ready", ready}};
}

} // namespace efvl
