// Shared helpers of the launcher's core: text conversion, files, hashes. Text inside the core is UTF-8;
// it becomes UTF-16 only at a Windows call.
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <filesystem>
#include <string>

#include <nlohmann/json.hpp>

#define EFVL_VERSION "0.1.0"

namespace efvl {

using json = nlohmann::json;
namespace fs = std::filesystem;

std::wstring widen(const std::string& text);
std::string narrow(const std::wstring& text);
inline fs::path path_of(const std::string& text) { return fs::path(widen(text)); }
inline std::string text_of(const fs::path& path) { return narrow(path.wstring()); }

bool read_file(const fs::path& path, std::string& out);
// Written beside the target and renamed over it, so a reader never sees half a file.
bool write_file(const fs::path& path, const std::string& data);
// Lower-case hex; empty when the file cannot be read.
std::string sha256_file(const fs::path& path);
std::string sha256(const void* data, size_t size);

fs::path program_dir();
// The JSON of a file, or a discarded value when it is missing or not JSON.
json read_json(const fs::path& path);
std::string dump(const json& value);

} // namespace efvl
