#include "util.h"

#include <bcrypt.h>

#include <cstdio>
#include <vector>

namespace efvl {

std::wstring widen(const std::string& text)
{
    if (text.empty())
        return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), &out[0], n);
    return out;
}

std::string narrow(const std::wstring& text)
{
    if (text.empty())
        return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), &out[0], n, nullptr, nullptr);
    return out;
}

bool read_file(const fs::path& path, std::string& out)
{
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return false;
    out.clear();
    char buf[1 << 16];
    DWORD got = 0;
    bool ok = true;
    for (;;) {
        if (!ReadFile(f, buf, sizeof(buf), &got, nullptr)) {
            ok = false;
            break;
        }
        if (!got)
            break;
        out.append(buf, got);
    }
    CloseHandle(f);
    return ok;
}

bool write_file(const fs::path& path, const std::string& data)
{
    std::error_code ec;
    if (path.has_parent_path())
        fs::create_directories(path.parent_path(), ec);
    fs::path temp = path;
    temp += L".tmp";
    HANDLE f = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return false;
    DWORD put = 0;
    bool ok = WriteFile(f, data.data(), static_cast<DWORD>(data.size()), &put, nullptr) && put == data.size();
    CloseHandle(f);
    if (ok)
        ok = MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
    if (!ok)
        DeleteFileW(temp.c_str());
    return ok;
}

std::string sha256(const void* data, size_t size)
{
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
        return std::string();
    unsigned char digest[32] = {};
    std::string out;
    if (BCryptHash(alg, nullptr, 0, static_cast<PUCHAR>(const_cast<void*>(data)), static_cast<ULONG>(size), digest,
                   sizeof(digest)) == 0) {
        char hex[65];
        for (int i = 0; i < 32; i++)
            snprintf(hex + i * 2, 3, "%02x", digest[i]);
        out.assign(hex, 64);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return out;
}

std::string sha256_file(const fs::path& path)
{
    std::string data;
    if (!read_file(path, data))
        return std::string();
    return sha256(data.data(), data.size());
}

fs::path program_dir()
{
    std::vector<wchar_t> buf(32768);
    DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    return fs::path(std::wstring(buf.data(), n)).parent_path();
}

json read_json(const fs::path& path)
{
    std::string text;
    if (!read_file(path, text))
        return json(json::value_t::discarded);
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF)
        text.erase(0, 3);
    return json::parse(text, nullptr, false);
}

std::string dump(const json& value)
{
    return value.dump(-1, ' ', false, json::error_handler_t::replace);
}

} // namespace efvl
