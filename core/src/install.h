// Installing the EFMI Vulkan layer from its release archive (EFMI-Vulkan-X.Y.Z.zip, one folder
// EFMI-Vulkan) into a folder of the user's choosing. The layer reads efmi_vk.cfg from its own
// folder; the launcher writes one value of it, m2_mods, so that the layer finds the EFMI folder.
#pragma once

#include "util.h"

namespace efvl {

inline constexpr wchar_t kLayerFolder[] = L"EFMI-Vulkan";
inline constexpr wchar_t kLayerDll[] = L"efmi_vk_inject.dll";
inline constexpr wchar_t kLayerConfig[] = L"efmi_vk.cfg";
inline constexpr wchar_t kInstallRecord[] = L"efvl-install.json";

struct InstallResult {
    // ok, archive_not_found, archive_invalid, checksum_mismatch, game_running, in_use, write_failed
    std::string code = "ok";
    std::string detail;
    std::string version;
    json to_json() const;
};

// The version an archive holds, from its name (EFMI-Vulkan-2.1.1.zip); empty when it names none.
std::string archive_version(const fs::path& archive);
// Three single digits compared as a number: negative, zero or positive. An unreadable one is lowest.
int compare_versions(const std::string& a, const std::string& b);

// Checks the archive whole (every entry read, the .sha256 beside it when there is one), then
// replaces the package's files in `folder` and leaves everything else there (settings, the log).
// `source` goes into the install record ("local" or "network").
InstallResult install_archive(const fs::path& archive, const fs::path& folder, const std::string& source = "local");

// Points the installed layer at the EFMI folder: m2_mods = <efmi>\Mods in its efmi_vk.cfg, every
// other line as it is. False when there is no configuration file or it cannot be written.
bool set_layer_mods(const fs::path& folder, const fs::path& efmi_folder);

// {installed, version, dll}: version is empty for a layer this launcher did not install.
json layer_state(const fs::path& folder);

} // namespace efvl
