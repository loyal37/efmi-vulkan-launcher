// Starting a program with a DLL in it before its first instruction runs.
//
// The process is created suspended, a remote thread runs LoadLibraryW for each DLL, and only then
// is the process resumed: the plainest injection there is, nothing hidden. The layer has to be in
// the game before the game loads the Vulkan loader, which is why the process must not run first.
#pragma once

#include "util.h"

#include <utility>
#include <vector>

namespace efvl {

struct LaunchRequest {
    fs::path exe;
    fs::path cwd;                 // empty: the program's folder
    std::wstring args;            // appended to the quoted program path as written
    std::vector<fs::path> dlls;   // loaded in this order
    // Variables set for the child only (an empty value removes one). The checks use it; the game gets none.
    std::vector<std::pair<std::wstring, std::wstring>> env;
    bool wait = false;            // wait for the program to end and report its exit code
};

struct LaunchResult {
    // ok, exe_not_found, dll_not_found, elevation_required, create_failed, wrong_architecture,
    // inject_failed, resume_failed
    std::string code = "ok";
    DWORD error = 0;              // GetLastError of the failing call
    std::string detail;           // which call or which DLL
    DWORD pid = 0;
    bool exited = false;
    DWORD exit_code = 0;
    json to_json() const;
};

LaunchResult launch(const LaunchRequest& request);

// Whether a process with this file name (no folder) is running.
bool process_running(const std::wstring& exe_name);

std::wstring quote_argument(const std::wstring& argument);

} // namespace efvl
