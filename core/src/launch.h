// Starting a program with a DLL in it before its first instruction runs.
//
// The process is created suspended, a remote thread runs LoadLibraryW for each DLL, and only then
// is the process resumed: the plainest injection there is, nothing hidden. The layer has to be in
// the game before the game loads the Vulkan loader, which is why the process must not run first.
//
// Outside Steam the game expects to be started by the official launcher. As the XXMI Launcher
// does (user, 2026-11-11), that launcher's Games.exe is created suspended and without a window,
// the game is created as its child, and Games.exe, which never ran, is ended once the game shows
// a window.
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
    // After the resume the program is watched this long: one that ends within it did not start
    // (a game that refuses the way it was started ends at once). Not used with `wait`.
    DWORD settle_ms = 0;
    // The program to be the parent of `exe` (the official launcher's Games.exe); empty: this process.
    fs::path parent_exe;
    // With a parent: how long to wait for the program's window before the parent is ended anyway.
    DWORD window_ms = 120000;
};

struct LaunchResult {
    // ok, exe_not_found, dll_not_found, elevation_required, create_failed, wrong_architecture,
    // inject_failed, resume_failed, exited_early, parent_not_found, parent_failed
    std::string code = "ok";
    DWORD error = 0;              // GetLastError of the failing call
    std::string detail;           // which call or which DLL
    DWORD pid = 0;
    DWORD parent_pid = 0;
    bool exited = false;
    DWORD exit_code = 0;
    json to_json() const;
};

LaunchResult launch(const LaunchRequest& request);

// Whether a process with this file name (no folder) is running.
bool process_running(const std::wstring& exe_name);

std::wstring quote_argument(const std::wstring& argument);

} // namespace efvl
