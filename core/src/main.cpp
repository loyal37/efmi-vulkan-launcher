// efvl_core: the launcher's core as a command line. Every command prints one line of JSON and
// ends with 0 when "ok" is true.
//
//   efvl_core status   [--data DIR]
//   efvl_core prefill  [--data DIR] [--xxmi-config FILE]
//   efvl_core set      [--data DIR] [--game DIR] [--efmi DIR] [--layer DIR] [--official DIR]
//   efvl_core install  [--data DIR] --archive FILE
//   efvl_core check-update [--data DIR] [--api URL]
//   efvl_core update   [--data DIR] [--api URL]
//   efvl_core start    [--data DIR] [--wait] [--settle MS] [--result FILE] [--no-elevate]
//   efvl_core launch   --exe FILE --dll FILE [--dll FILE] [--cwd DIR] [--args TEXT] [--env NAME=VALUE]
//                      [--wait] [--result FILE] [--no-elevate]
//   efvl_core version
//
// start and launch create the program suspended, load the DLL and resume. A program that needs
// administrator rights makes the core start itself again elevated (one UAC prompt); the elevated
// copy writes its answer to the result file, which this one reads and prints.
#include "actions.h"
#include "install.h"
#include "launch.h"
#include "settings.h"
#include "update.h"

#include <shellapi.h>

#include <cstdio>
#include <string>
#include <vector>

using namespace efvl;

namespace {

struct Args {
    std::wstring command;
    fs::path data, xxmi_config, archive, result, exe, cwd;
    std::vector<fs::path> dlls;
    std::wstring args;
    std::string api;
    DWORD settle = 5000;
    std::vector<std::pair<std::wstring, std::wstring>> env;
    json paths = json::object();
    bool wait = false, no_elevate = false, elevated = false, bad = false;
};

Args parse(int argc, wchar_t** argv)
{
    Args a;
    if (argc < 2) {
        a.bad = true;
        return a;
    }
    a.command = argv[1];
    for (int i = 2; i < argc; i++) {
        const std::wstring key = argv[i];
        auto value = [&]() -> std::wstring {
            if (i + 1 < argc)
                return argv[++i];
            a.bad = true;
            return std::wstring();
        };
        if (key == L"--data") a.data = value();
        else if (key == L"--xxmi-config") a.xxmi_config = value();
        else if (key == L"--archive") a.archive = value();
        else if (key == L"--result") a.result = value();
        else if (key == L"--exe") a.exe = value();
        else if (key == L"--cwd") a.cwd = value();
        else if (key == L"--dll") a.dlls.push_back(value());
        else if (key == L"--api") a.api = narrow(value());
        else if (key == L"--args") a.args = value();
        else if (key == L"--game") a.paths["game_path"] = narrow(value());
        else if (key == L"--efmi") a.paths["efmi_path"] = narrow(value());
        else if (key == L"--layer") a.paths["layer_path"] = narrow(value());
        else if (key == L"--official") a.paths["launcher_path"] = narrow(value());
        else if (key == L"--env") {
            const std::wstring pair = value();
            const size_t eq = pair.find(L'=');
            if (eq == std::wstring::npos || !eq)
                a.bad = true;
            else
                a.env.emplace_back(pair.substr(0, eq), pair.substr(eq + 1));
        }
        else if (key == L"--settle") a.settle = static_cast<DWORD>(_wtoi(value().c_str()));
        else if (key == L"--wait") a.wait = true;
        else if (key == L"--no-elevate") a.no_elevate = true;
        else if (key == L"--elevated") a.elevated = true;
        else a.bad = true;
    }
    return a;
}

// The same command again with administrator rights; its answer comes back through the result file.
json run_elevated(int argc, wchar_t** argv, const Args& a)
{
    fs::path result = a.result;
    if (result.empty()) {
        wchar_t temp[MAX_PATH + 1] = {};
        GetTempPathW(MAX_PATH, temp);
        result = fs::path(temp) / (L"efvl-result-" + std::to_wstring(GetCurrentProcessId()) + L".json");
    }
    DeleteFileW(result.c_str());
    std::wstring params;
    for (int i = 1; i < argc; i++)
        params += quote_argument(argv[i]) + L" ";
    params += L"--elevated";
    if (a.result.empty())
        params += L" --result " + quote_argument(result.wstring());

    std::vector<wchar_t> self(32768);
    GetModuleFileNameW(nullptr, self.data(), static_cast<DWORD>(self.size()));
    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NO_CONSOLE;
    info.lpVerb = L"runas";
    info.lpFile = self.data();
    info.lpParameters = params.c_str();
    info.nShow = SW_HIDE;
    if (!ShellExecuteExW(&info)) {
        const DWORD error = GetLastError();
        return {{"ok", false}, {"code", error == ERROR_CANCELLED ? "elevation_declined" : "elevation_failed"}, {"error", error}};
    }
    WaitForSingleObject(info.hProcess, INFINITE);
    CloseHandle(info.hProcess);
    json answer = read_json(result);
    if (a.result.empty())
        DeleteFileW(result.c_str());
    if (!answer.is_object())
        return {{"ok", false}, {"code", "elevation_failed"}};
    return answer;
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
    const Args a = parse(argc, argv);
    json answer;
    if (a.bad) {
        answer = {{"ok", false}, {"code", "usage"}};
    } else {
        const fs::path data = data_dir(a.data);
        if (a.command == L"version")
            answer = {{"ok", true}, {"code", "ok"}, {"version", EFVL_VERSION}};
        else if (a.command == L"status")
            answer = launcher_status(data), answer["ok"] = true, answer["code"] = "ok";
        else if (a.command == L"prefill")
            answer = action_prefill(data, a.xxmi_config);
        else if (a.command == L"set")
            answer = action_set_paths(data, a.paths);
        else if (a.command == L"install")
            answer = action_install(data, a.archive);
        else if (a.command == L"check-update")
            answer = action_check_update(data, a.api);
        else if (a.command == L"update")
            answer = action_update(data, a.api);
        else if (a.command == L"start")
            answer = action_start(data, a.wait, a.args, a.env, a.settle);
        else if (a.command == L"launch") {
            LaunchRequest request;
            request.exe = a.exe;
            request.cwd = a.cwd;
            request.args = a.args;
            request.dlls = a.dlls;
            request.env = a.env;
            request.wait = a.wait;
            answer = launch(request).to_json();
        } else
            answer = {{"ok", false}, {"code", "usage"}};

        if (answer.value("code", std::string()) == "elevation_required" && !a.no_elevate && !a.elevated)
            answer = run_elevated(argc, argv, a);
    }
    const std::string line = dump(answer);
    if (!a.result.empty())
        write_file(a.result, line + "\n");
    printf("%s\n", line.c_str());
    return answer.value("ok", false) ? 0 : 1;
}
