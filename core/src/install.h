// Installing the EFMI Vulkan layer from its release archive (EFMI-Vulkan-X.Y.Z.zip, one folder
// EFMI-Vulkan) into <EFMI folder>\EFMI-Vulkan, where the layer finds Mods and Core by itself.
#pragma once

#include "util.h"

namespace efvl {

inline constexpr wchar_t kLayerFolder[] = L"EFMI-Vulkan";
inline constexpr wchar_t kLayerDll[] = L"efmi_vk_inject.dll";
inline constexpr wchar_t kInstallRecord[] = L"efvl-install.json";

struct InstallResult {
    // ok, archive_not_found, archive_invalid, checksum_mismatch, efmi_invalid, in_use, write_failed
    std::string code = "ok";
    std::string detail;
    std::string version;
    json to_json() const;
};

// The version an archive holds, from its name; empty when the name carries none.
std::string archive_version(const fs::path& archive);

// Replaces the package's files and leaves everything else in the folder (settings, the log). An
// efmi_vk.cfg that differs from the new one is kept as efmi_vk.cfg.previous.
InstallResult install_archive(const fs::path& archive, const fs::path& efmi_folder);

// {installed, version, dll}: version is empty for a layer this launcher did not install.
json layer_state(const fs::path& efmi_folder);

} // namespace efvl
