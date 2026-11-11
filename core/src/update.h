// EFMI Vulkan from the network: the latest release of the layer's repository, downloaded, checked
// and installed. Download only: a request carries a generic program name and nothing else.
#pragma once

#include "util.h"

namespace efvl {

inline constexpr char kReleaseApi[] = "https://api.github.com/repos/loyal37/efmi-vulkan/releases/latest";

// A GET of an http or https address, redirects followed. False when there is no answer, the
// status is not 200 or the body is larger than `limit`; `status` is 0 without an answer.
bool http_get(const std::string& url, std::string& body, DWORD& status, size_t limit);

// {ok, code, latest, installed, available}: available when the latest release is newer than what
// is installed, or nothing with a known version is installed. Code network_failed or
// release_invalid when the answer cannot be had or read. `api` empty: the layer's repository.
json action_check_update(const fs::path& data, const std::string& api = std::string());
// Installs the latest release when it is available; code up_to_date when it is not.
json action_update(const fs::path& data, const std::string& api = std::string());

} // namespace efvl
