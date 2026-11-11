#include "update.h"

#include "actions.h"
#include "install.h"
#include "settings.h"

#include <winhttp.h>

#include <regex>

namespace efvl {

namespace {

struct Handle {
    HINTERNET h = nullptr;
    ~Handle()
    {
        if (h)
            WinHttpCloseHandle(h);
    }
    operator HINTERNET() const { return h; }
};

struct Release {
    std::string code = "ok", detail, version, asset, url;
};

Release latest_release(const std::string& api)
{
    Release r;
    std::string body;
    DWORD status = 0;
    if (!http_get(api.empty() ? kReleaseApi : api, body, status, 4u << 20)) {
        r.code = "network_failed";
        r.detail = status ? "HTTP " + std::to_string(status) : "no answer";
        return r;
    }
    const json j = json::parse(body, nullptr, false);
    static const std::regex tag(R"(^v?(\d\.\d\.\d)$)"), name(R"(^EFMI-Vulkan-(\d\.\d\.\d)\.zip$)");
    std::smatch m;
    const std::string tag_name = j.is_object() ? j.value("tag_name", std::string()) : std::string();
    if (std::regex_match(tag_name, m, tag) && j.contains("assets") && j["assets"].is_array()) {
        r.version = m[1].str();
        for (const json& asset : j["assets"]) {
            if (!asset.is_object())
                continue;
            const std::string asset_name = asset.value("name", std::string());
            const std::string url = asset.value("browser_download_url", std::string());
            if (std::regex_match(asset_name, m, name) && m[1].str() == r.version &&
                (url.rfind("https://", 0) == 0 || url.rfind("http://", 0) == 0)) {
                r.asset = asset_name;
                r.url = url;
            }
        }
    }
    if (r.url.empty()) {
        r.code = "release_invalid";
        r.detail = tag_name;
    }
    return r;
}

json describe(const Release& r, const json& layer)
{
    const std::string installed = layer.value("version", std::string());
    const bool available = !layer.value("installed", false) || installed.empty() || compare_versions(r.version, installed) > 0;
    return {{"ok", true}, {"code", "ok"}, {"latest", r.version}, {"installed", installed}, {"available", available}};
}

json failure(const std::string& code, const std::string& detail)
{
    json j = {{"ok", false}, {"code", code}};
    if (!detail.empty())
        j["detail"] = detail;
    return j;
}

} // namespace

bool http_get(const std::string& url, std::string& body, DWORD& status, size_t limit)
{
    status = 0;
    body.clear();
    const std::wstring wide = widen(url);
    URL_COMPONENTS parts = {};
    parts.dwStructSize = sizeof(parts);
    wchar_t host[256] = {}, path[4096] = {};
    parts.lpszHostName = host;
    parts.dwHostNameLength = 255;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = 4095;
    if (!WinHttpCrackUrl(wide.c_str(), 0, 0, &parts) ||
        (parts.nScheme != INTERNET_SCHEME_HTTP && parts.nScheme != INTERNET_SCHEME_HTTPS))
        return false;
    Handle session{WinHttpOpen(L"EFMI-Vulkan-Launcher", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                               WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!session)
        return false;
    WinHttpSetTimeouts(session, 8000, 8000, 15000, 30000);
    Handle connection{WinHttpConnect(session, host, parts.nPort, 0)};
    if (!connection)
        return false;
    const wchar_t* accept[] = {L"*/*", nullptr};
    Handle request{WinHttpOpenRequest(connection, L"GET", path, nullptr, WINHTTP_NO_REFERER, accept,
                                      parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)};
    if (!request)
        return false;
    // No cookies and no credentials: the request is the address and the program's name.
    DWORD off = WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION;
    WinHttpSetOption(request, WINHTTP_OPTION_DISABLE_FEATURE, &off, sizeof(off));
    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request, nullptr))
        return false;
    DWORD size = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status,
                        &size, WINHTTP_NO_HEADER_INDEX);
    if (status != 200)
        return false;
    std::string chunk(1 << 16, '\0');
    for (;;) {
        DWORD got = 0;
        if (!WinHttpReadData(request, &chunk[0], static_cast<DWORD>(chunk.size()), &got))
            return false;
        if (!got)
            return true;
        if (body.size() + got > limit)
            return false;
        body.append(chunk.data(), got);
    }
}

json action_check_update(const fs::path& data, const std::string& api)
{
    const Release r = latest_release(api);
    if (r.code != "ok")
        return failure(r.code, r.detail);
    return describe(r, layer_state(layer_dir(load_settings(data))));
}

json action_update(const fs::path& data, const std::string& api)
{
    const Release r = latest_release(api);
    if (r.code != "ok")
        return failure(r.code, r.detail);
    json answer = describe(r, layer_state(layer_dir(load_settings(data))));
    if (!answer["available"].get<bool>()) {
        answer["code"] = "up_to_date";
        return answer;
    }
    std::string bytes;
    DWORD status = 0;
    if (!http_get(r.url, bytes, status, 256u << 20))
        return failure("network_failed", status ? "HTTP " + std::to_string(status) : "no answer");
    // The archive goes through the same door as a local one: checked whole before anything is replaced.
    const fs::path archive = data / L"downloads" / path_of(r.asset);
    if (!write_file(archive, bytes))
        return failure("write_failed", "downloads");
    json done = action_install(data, archive, "network");
    std::error_code ec;
    fs::remove(archive, ec);
    done["latest"] = r.version;
    return done;
}

} // namespace efvl
