#include "launch.h"

#include <psapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <map>

namespace efvl {

json LaunchResult::to_json() const
{
    json j = {{"code", code}, {"ok", code == "ok"}};
    if (error)
        j["error"] = error;
    if (!detail.empty())
        j["detail"] = detail;
    if (pid)
        j["pid"] = pid;
    if (exited)
        j["exit_code"] = exit_code;
    return j;
}

// The quoting CommandLineToArgvW undoes.
std::wstring quote_argument(const std::wstring& argument)
{
    if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos)
        return argument;
    std::wstring out = L"\"";
    for (size_t i = 0;; i++) {
        size_t slashes = 0;
        while (i < argument.size() && argument[i] == L'\\') {
            i++;
            slashes++;
        }
        if (i == argument.size()) {
            out.append(slashes * 2, L'\\');
            break;
        }
        if (argument[i] == L'"')
            out.append(slashes * 2 + 1, L'\\');
        else
            out.append(slashes, L'\\');
        out.push_back(argument[i]);
    }
    out.push_back(L'"');
    return out;
}

bool process_running(const std::wstring& exe_name)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return false;
    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    bool found = false;
    for (BOOL more = Process32FirstW(snap, &entry); more && !found; more = Process32NextW(snap, &entry))
        found = _wcsicmp(entry.szExeFile, exe_name.c_str()) == 0;
    CloseHandle(snap);
    return found;
}

namespace {

struct NameLess {
    bool operator()(const std::wstring& a, const std::wstring& b) const { return _wcsicmp(a.c_str(), b.c_str()) < 0; }
};

// This process's environment with the request's variables applied, as CreateProcessW takes it.
std::vector<wchar_t> environment_block(const std::vector<std::pair<std::wstring, std::wstring>>& changes)
{
    std::map<std::wstring, std::wstring, NameLess> vars;
    if (wchar_t* block = GetEnvironmentStringsW()) {
        for (const wchar_t* p = block; *p; p += wcslen(p) + 1) {
            std::wstring entry = p;
            size_t eq = entry.find(L'=', 1); // names like "=C:" start with the sign
            if (eq != std::wstring::npos)
                vars[entry.substr(0, eq)] = entry.substr(eq + 1);
        }
        FreeEnvironmentStringsW(block);
    }
    for (const auto& change : changes) {
        if (change.second.empty())
            vars.erase(change.first);
        else
            vars[change.first] = change.second;
    }
    std::vector<wchar_t> out;
    for (const auto& var : vars) {
        out.insert(out.end(), var.first.begin(), var.first.end());
        out.push_back(L'=');
        out.insert(out.end(), var.second.begin(), var.second.end());
        out.push_back(L'\0');
    }
    out.push_back(L'\0');
    return out;
}

bool module_loaded(HANDLE process, const fs::path& dll)
{
    std::vector<HMODULE> mods(4096);
    DWORD needed = 0;
    if (!EnumProcessModulesEx(process, mods.data(), static_cast<DWORD>(mods.size() * sizeof(HMODULE)), &needed,
                              LIST_MODULES_64BIT))
        return false;
    size_t count = std::min(mods.size(), static_cast<size_t>(needed / sizeof(HMODULE)));
    std::error_code ec;
    for (size_t i = 0; i < count; i++) {
        wchar_t name[32768 / 8];
        DWORD n = GetModuleFileNameExW(process, mods[i], name, static_cast<DWORD>(sizeof(name) / sizeof(name[0])));
        if (n && fs::equivalent(fs::path(std::wstring(name, n)), dll, ec))
            return true;
    }
    return false;
}

// LoadLibraryW(dll) on a thread of its own in the process. kernel32.dll is mapped at one address
// in every process of a session, so the function's address here is its address there.
bool load_library(HANDLE process, const fs::path& dll, LaunchResult& result)
{
    const std::wstring text = dll.wstring();
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    result.detail = text_of(dll);
    void* remote = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) {
        result.error = GetLastError();
        result.detail += ": VirtualAllocEx";
        return false;
    }
    bool ok = false;
    HANDLE thread = nullptr;
    if (!WriteProcessMemory(process, remote, text.c_str(), bytes, nullptr)) {
        result.error = GetLastError();
        result.detail += ": WriteProcessMemory";
    } else {
        HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
        auto start = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(kernel, "LoadLibraryW"));
        thread = CreateRemoteThread(process, nullptr, 0, start, remote, 0, nullptr);
        if (!thread) {
            result.error = GetLastError();
            result.detail += ": CreateRemoteThread";
        } else if (WaitForSingleObject(thread, 60000) != WAIT_OBJECT_0) {
            result.error = WAIT_TIMEOUT;
            result.detail += ": the loading thread did not end";
        } else {
            // The thread's exit code is the low half of the module handle: zero when LoadLibraryW failed.
            DWORD code = 0;
            GetExitCodeThread(thread, &code);
            ok = code != 0 || module_loaded(process, dll);
            if (!ok)
                result.detail += ": LoadLibraryW failed in the process";
        }
    }
    if (thread)
        CloseHandle(thread);
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    if (ok)
        result.detail.clear();
    return ok;
}

} // namespace

LaunchResult launch(const LaunchRequest& request)
{
    LaunchResult result;
    std::error_code ec;
    if (!fs::is_regular_file(request.exe, ec)) {
        result.code = "exe_not_found";
        result.detail = text_of(request.exe);
        return result;
    }
    for (const fs::path& dll : request.dlls) {
        if (!fs::is_regular_file(dll, ec)) {
            result.code = "dll_not_found";
            result.detail = text_of(dll);
            return result;
        }
    }

    std::wstring command = quote_argument(request.exe.wstring());
    if (!request.args.empty())
        command += L" " + request.args;
    const fs::path cwd = request.cwd.empty() ? request.exe.parent_path() : request.cwd;
    std::vector<wchar_t> env;
    if (!request.env.empty())
        env = environment_block(request.env);

    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(request.exe.c_str(), &command[0], nullptr, nullptr, FALSE,
                        // No console shared with the core: its one line of JSON stays alone on its output.
                        CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | DETACHED_PROCESS, env.empty() ? nullptr : env.data(), cwd.c_str(),
                        &startup, &process)) {
        result.error = GetLastError();
        result.code = result.error == ERROR_ELEVATION_REQUIRED ? "elevation_required" : "create_failed";
        result.detail = "CreateProcessW";
        return result;
    }
    result.pid = process.dwProcessId;

    // From here the process exists and has not run: any failure ends it again.
    auto abandon = [&](const char* code) {
        result.code = code;
        TerminateProcess(process.hProcess, 1);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return result;
    };

    BOOL wow64 = FALSE;
    if (!IsWow64Process(process.hProcess, &wow64) || wow64) {
        result.detail = "the program is not a 64-bit program";
        return abandon("wrong_architecture");
    }
    for (const fs::path& dll : request.dlls) {
        if (!load_library(process.hProcess, dll, result))
            return abandon("inject_failed");
    }
    if (ResumeThread(process.hThread) == static_cast<DWORD>(-1)) {
        result.error = GetLastError();
        result.detail = "ResumeThread";
        return abandon("resume_failed");
    }
    if (request.wait) {
        WaitForSingleObject(process.hProcess, INFINITE);
        result.exited = GetExitCodeProcess(process.hProcess, &result.exit_code) != 0;
    } else if (request.settle_ms && WaitForSingleObject(process.hProcess, request.settle_ms) == WAIT_OBJECT_0) {
        result.exited = GetExitCodeProcess(process.hProcess, &result.exit_code) != 0;
        result.code = "exited_early";
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return result;
}

} // namespace efvl
