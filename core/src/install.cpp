#include "install.h"

#include "launch.h"
#include "settings.h"

#include <miniz.h>

#include <map>
#include <regex>

namespace efvl {

json InstallResult::to_json() const
{
    json j = {{"code", code}, {"ok", code == "ok"}};
    if (!detail.empty())
        j["detail"] = detail;
    if (!version.empty())
        j["version"] = version;
    return j;
}

std::string archive_version(const fs::path& archive)
{
    const std::string name = text_of(archive.filename());
    std::smatch m;
    static const std::regex pattern(R"((\d\.\d\.\d)(?=\.zip$|[^\d.]|$))", std::regex::icase);
    return std::regex_search(name, m, pattern) ? m[1].str() : std::string();
}

int compare_versions(const std::string& a, const std::string& b)
{
    auto number = [](const std::string& v) {
        static const std::regex pattern(R"(^v?(\d)\.(\d)\.(\d)$)");
        std::smatch m;
        if (!std::regex_match(v, m, pattern))
            return -1;
        return (m[1].str()[0] - '0') * 100 + (m[2].str()[0] - '0') * 10 + (m[3].str()[0] - '0');
    };
    return number(a) - number(b);
}

namespace {

const char kPrefix[] = "EFMI-Vulkan/";

// A name inside the archive that stays inside the folder: no drive, no root, no "..".
bool safe_name(const std::string& name)
{
    if (name.empty() || name[0] == '/' || name[0] == '\\' || name.find(':') != std::string::npos ||
        name.find('\\') != std::string::npos)
        return false;
    size_t at = 0;
    while (at <= name.size()) {
        size_t end = name.find('/', at);
        if (end == std::string::npos)
            end = name.size();
        const std::string part = name.substr(at, end - at);
        if (part == ".." || part == ".")
            return false;
        at = end + 1;
    }
    return true;
}

// Every file of the archive, by its path below EFMI-Vulkan/. Empty with `problem` set when the
// archive is not a whole release of the layer.
std::map<std::string, std::string> read_archive(const std::string& bytes, std::string& problem)
{
    std::map<std::string, std::string> files;
    mz_zip_archive zip = {};
    if (!mz_zip_reader_init_mem(&zip, bytes.data(), bytes.size(), 0)) {
        problem = "not a ZIP archive";
        return files;
    }
    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < count && problem.empty(); i++) {
        mz_zip_archive_file_stat stat = {};
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) {
            problem = "an entry cannot be read";
            break;
        }
        const std::string name = stat.m_filename;
        if (mz_zip_reader_is_file_a_directory(&zip, i))
            continue;
        if (name.compare(0, sizeof(kPrefix) - 1, kPrefix) != 0 || !safe_name(name)) {
            problem = "an entry outside EFMI-Vulkan/: " + name;
            break;
        }
        size_t size = 0;
        void* data = mz_zip_reader_extract_to_heap(&zip, i, &size, 0); // checks the entry's CRC
        if (!data) {
            problem = "damaged entry: " + name;
            break;
        }
        files[name.substr(sizeof(kPrefix) - 1)].assign(static_cast<const char*>(data), size);
        mz_free(data);
    }
    mz_zip_reader_end(&zip);
    if (problem.empty() && !files.count(narrow(kLayerDll)))
        problem = "no efmi_vk_inject.dll in the archive";
    if (!problem.empty())
        files.clear();
    return files;
}

// Whether a file can be replaced now: a DLL loaded in a process cannot.
bool replaceable(const fs::path& path)
{
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE | DELETE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND;
    CloseHandle(f);
    return true;
}

} // namespace

InstallResult install_archive(const fs::path& archive, const fs::path& folder, const std::string& source)
{
    InstallResult result;
    std::string bytes;
    if (!read_file(archive, bytes)) {
        result.code = "archive_not_found";
        result.detail = text_of(archive);
        return result;
    }
    const std::string digest = sha256(bytes.data(), bytes.size());
    fs::path sums = archive;
    sums += L".sha256";
    std::string recorded;
    if (read_file(sums, recorded)) {
        recorded = recorded.substr(0, recorded.find_first_of(" \t\r\n"));
        for (char& ch : recorded)
            ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
        if (recorded != digest) {
            result.code = "checksum_mismatch";
            return result;
        }
    }
    std::string problem;
    const std::map<std::string, std::string> files = read_archive(bytes, problem);
    if (files.empty()) {
        result.code = "archive_invalid";
        result.detail = problem;
        return result;
    }

    // Nothing is touched before everything is known to be replaceable.
    if (process_running(kGameExe)) {
        result.code = "game_running";
        return result;
    }
    for (const auto& file : files) {
        if (!replaceable(folder / path_of(file.first))) {
            result.code = "in_use";
            result.detail = file.first;
            return result;
        }
    }

    // A configuration the user changed is not lost with the update.
    const fs::path config = folder / kLayerConfig;
    std::string old_config;
    auto new_config = files.find(narrow(kLayerConfig));
    if (new_config != files.end() && read_file(config, old_config) && old_config != new_config->second) {
        fs::path previous = config;
        previous += L".previous";
        write_file(previous, old_config);
    }
    for (const auto& file : files) {
        if (!write_file(folder / path_of(file.first), file.second)) {
            result.code = "write_failed";
            result.detail = file.first;
            return result;
        }
    }
    result.version = archive_version(archive);
    const json record = {{"version", result.version}, {"archive", text_of(archive.filename())}, {"sha256", digest},
                         {"source", source}};
    if (!write_file(folder / kInstallRecord, record.dump(2) + "\n")) {
        result.code = "write_failed";
        result.detail = narrow(kInstallRecord);
    }
    return result;
}

bool set_layer_mods(const fs::path& folder, const fs::path& efmi_folder)
{
    const fs::path config = folder / kLayerConfig;
    std::string text;
    if (!read_file(config, text))
        return false;
    // The layer reads "key = value" lines; forward slashes, as its own release check writes the path.
    std::string mods = text_of(efmi_folder / L"Mods");
    for (char& ch : mods)
        if (ch == '\\')
            ch = '/';
    const std::string line = "m2_mods = " + mods;
    std::string out;
    bool replaced = false;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        const bool last = end == std::string::npos;
        if (last)
            end = text.size();
        std::string current = text.substr(at, end - at);
        const bool cr = !current.empty() && current.back() == '\r';
        if (cr)
            current.pop_back();
        static const std::regex key(R"(^\s*m2_mods\s*=.*$)");
        if (std::regex_match(current, key)) {
            current = line;
            replaced = true;
        }
        out += current;
        if (cr)
            out += '\r';
        if (!last)
            out += '\n';
        at = end + 1;
    }
    if (!replaced) {
        if (!out.empty() && out.back() != '\n')
            out += '\n';
        out += line + "\n";
    }
    return out == text || write_file(config, out);
}

json layer_state(const fs::path& folder)
{
    std::error_code ec;
    const fs::path dll = folder / kLayerDll;
    const bool installed = !folder.empty() && fs::is_regular_file(dll, ec);
    std::string version;
    if (installed) {
        const json record = read_json(folder / kInstallRecord);
        if (record.is_object())
            version = record.value("version", std::string());
    }
    return {{"installed", installed}, {"version", version}, {"dll", installed ? text_of(dll) : std::string()}};
}

} // namespace efvl
