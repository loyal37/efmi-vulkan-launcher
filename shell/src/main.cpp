// The launcher's window: a Win32 host of WebView2 that shows ui/ and answers the page's requests
// with the core's library. Not elevated; a start runs efvl_core.exe, which elevates itself when
// the game needs it.
//
//   "EFMI Vulkan Launcher.exe" [--data DIR] [--dev] [--page NAME] [--no-activate]
//                              [--screenshot FILE] [--eval FILE --eval-out FILE]
//
// --page opens the page at a named view (the checks and screenshots use it). --no-activate shows
// the window on the secondary screen without taking the focus. --screenshot writes the rendered
// page as a PNG and ends. --eval runs a script of a check in the page once it has loaded; the
// script ends the program by posting {cmd: "test_done", result}, which is written to --eval-out.
// EFVL_RELEASE_API names another address for the release lookup (the checks' stand-in server).
#include "actions.h"
#include "install.h"
#include "launch.h"
#include "settings.h"
#include "update.h"

#include <objbase.h>
#include <ole2.h>

#include <WebView2.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <wrl.h>

#include <string>
#include <thread>

using namespace efvl;
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace {

const wchar_t kHost[] = L"efvl.example";
const UINT kReply = WM_APP + 1;   // lParam: a std::wstring* with the JSON for the page
const UINT kCapture = WM_APP + 2;
const UINT kEval = WM_APP + 3;
const int kWidth = 1000, kHeight = 600; // the page's size at 100 %
const COLORREF kBackground = RGB(0x0f, 0x11, 0x15);

struct App {
    HWND window = nullptr;
    fs::path data, screenshot, eval, eval_out;
    std::string api;
    std::wstring page;
    bool dev = false, no_activate = false;
    json strings;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> view;
} g;

// A text of the strings file for the native dialogs; English when the file is missing.
std::wstring text(const char* key, const wchar_t* fallback)
{
    if (g.strings.is_object() && g.strings.contains(key) && g.strings[key].is_string())
        return widen(g.strings[key].get<std::string>());
    return fallback;
}

void reply(const json& message)
{
    PostMessageW(g.window, kReply, 0, reinterpret_cast<LPARAM>(new std::wstring(widen(dump(message)))));
}

// A folder or a .zip file chosen by the user; empty when the dialog was closed.
fs::path choose(bool folder, const fs::path& start)
{
    fs::path chosen;
    ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))))
        return chosen;
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_FORCEFILESYSTEM | (folder ? FOS_PICKFOLDERS : FOS_FILEMUSTEXIST));
    if (!folder) {
        const COMDLG_FILTERSPEC zip[] = {{L"ZIP", L"*.zip"}};
        dialog->SetFileTypes(1, zip);
    }
    std::error_code ec;
    if (!start.empty() && fs::is_directory(start, ec)) {
        ComPtr<IShellItem> at;
        if (SUCCEEDED(SHCreateItemFromParsingName(start.c_str(), nullptr, IID_PPV_ARGS(&at))))
            dialog->SetFolder(at.Get());
    }
    if (SUCCEEDED(dialog->Show(g.window))) {
        ComPtr<IShellItem> item;
        PWSTR raw = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item)) && SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw)))
            chosen = raw;
        CoTaskMemFree(raw);
    }
    return chosen;
}

// efvl_core.exe start, hidden; its answer comes through a result file because an elevated copy
// cannot hand it back any other way.
json run_core_start()
{
    const fs::path core = program_dir() / L"efvl_core.exe";
    const fs::path result = g.data / L"start-result.json";
    DeleteFileW(result.c_str());
    std::wstring command = quote_argument(core.wstring()) + L" start --data " + quote_argument(g.data.wstring()) +
                           L" --result " + quote_argument(result.wstring());
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(core.c_str(), &command[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup,
                        &process))
        return {{"ok", false}, {"code", "core_missing"}};
    WaitForSingleObject(process.hProcess, INFINITE);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    json answer = read_json(result);
    DeleteFileW(result.c_str());
    return answer.is_object() ? answer : json{{"ok", false}, {"code", "core_failed"}};
}

json state()
{
    json s = launcher_status(g.data);
    s["ok"] = true;
    return s;
}

// One request of the page: {id, cmd, ...}. The answer carries the id back.
void handle(const json& request)
{
    const std::string cmd = request.value("cmd", std::string());
    const json id = request.contains("id") ? request["id"] : json();
    json answer;
    if (cmd == "state") {
        action_prefill(g.data);
        answer = state();
    } else if (cmd == "browse") {
        // key: game_path, efmi_path or layer_path. The folder chosen is saved at once.
        const std::string key = request.value("key", std::string());
        const json before = launcher_status(g.data);
        const fs::path chosen = choose(true, path_of(before.value(key, std::string())));
        if (!chosen.empty())
            action_set_paths(g.data, json{{key, text_of(chosen)}});
        answer = state();
    } else if (cmd == "install_file") {
        const fs::path archive = choose(false, fs::path());
        if (archive.empty()) {
            answer = state();
            answer["cancelled"] = true;
        } else {
            const json done = action_install(g.data, archive);
            answer = state();
            answer["ok"] = done.value("ok", false);
            answer["code"] = done.value("code", std::string());
        }
    } else if (cmd == "check_update" || cmd == "update") {
        // The network is asked off the window's thread.
        std::thread([id, cmd] {
            const json done = cmd == "update" ? action_update(g.data, g.api) : action_check_update(g.data, g.api);
            json answer = state();
            for (const char* key : {"ok", "code", "latest", "available"})
                if (done.contains(key))
                    answer[key] = done[key];
            answer["id"] = id;
            reply(answer);
        }).detach();
        return;
    } else if (cmd == "test_done" && !g.eval_out.empty()) {
        write_file(g.eval_out, dump(request.contains("result") ? request["result"] : json()) + "\n");
        PostMessageW(g.window, WM_CLOSE, 0, 0);
        return;
    } else if (cmd == "start") {
        std::thread([id] {
            json done = run_core_start();
            done["id"] = id;
            reply(done);
        }).detach();
        return;
    } else {
        answer = {{"ok", false}, {"code", "unknown_command"}};
    }
    answer["id"] = id;
    reply(answer);
}

void resize()
{
    if (!g.controller)
        return;
    RECT bounds;
    GetClientRect(g.window, &bounds);
    g.controller->put_Bounds(bounds);
}

void capture()
{
    ComPtr<IStream> stream;
    if (FAILED(SHCreateStreamOnFileEx(g.screenshot.c_str(), STGM_READWRITE | STGM_CREATE, FILE_ATTRIBUTE_NORMAL, TRUE,
                                      nullptr, &stream))) {
        PostQuitMessage(2);
        return;
    }
    g.view->CapturePreview(COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG, stream.Get(),
                           Callback<ICoreWebView2CapturePreviewCompletedHandler>([stream](HRESULT hr) -> HRESULT {
                               PostQuitMessage(SUCCEEDED(hr) ? 0 : 2);
                               return S_OK;
                           }).Get());
}

void on_view(ICoreWebView2Controller* controller)
{
    g.controller = controller;
    g.controller->get_CoreWebView2(&g.view);
    ComPtr<ICoreWebView2Controller2> colour;
    if (SUCCEEDED(g.controller.As(&colour)))
        colour->put_DefaultBackgroundColor({255, 0x0f, 0x11, 0x15});
    ComPtr<ICoreWebView2Settings> settings;
    g.view->get_Settings(&settings);
    settings->put_AreDefaultContextMenusEnabled(g.dev);
    settings->put_AreDevToolsEnabled(g.dev);
    settings->put_IsStatusBarEnabled(FALSE);
    settings->put_IsZoomControlEnabled(FALSE);
    ComPtr<ICoreWebView2Settings3> keys;
    if (SUCCEEDED(settings.As(&keys)))
        keys->put_AreBrowserAcceleratorKeysEnabled(g.dev);

    ComPtr<ICoreWebView2_3> mapping;
    if (SUCCEEDED(g.view.As(&mapping)))
        mapping->SetVirtualHostNameToFolderMapping(kHost, (program_dir() / L"ui").c_str(),
                                                   COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);
    g.view->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                PWSTR raw = nullptr;
                if (SUCCEEDED(args->get_WebMessageAsJson(&raw)) && raw) {
                    const json request = json::parse(narrow(raw), nullptr, false);
                    if (request.is_object())
                        handle(request);
                }
                CoTaskMemFree(raw);
                return S_OK;
            }).Get(),
        nullptr);
    // The page never leaves the launcher's own files.
    g.view->add_NavigationStarting(
        Callback<ICoreWebView2NavigationStartingEventHandler>(
            [](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                PWSTR uri = nullptr;
                args->get_Uri(&uri);
                const std::wstring prefix = std::wstring(L"https://") + kHost + L"/";
                if (!uri || wcsncmp(uri, prefix.c_str(), prefix.size()) != 0)
                    args->put_Cancel(TRUE);
                CoTaskMemFree(uri);
                return S_OK;
            }).Get(),
        nullptr);
    g.view->add_NewWindowRequested(
        Callback<ICoreWebView2NewWindowRequestedEventHandler>(
            [](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                args->put_Handled(TRUE);
                return S_OK;
            }).Get(),
        nullptr);
    if (!g.eval.empty())
        g.view->add_NavigationCompleted(
            Callback<ICoreWebView2NavigationCompletedEventHandler>(
                [](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                    SetTimer(g.window, kEval, 800, nullptr);
                    return S_OK;
                }).Get(),
            nullptr);
    if (!g.screenshot.empty())
        g.view->add_NavigationCompleted(
            Callback<ICoreWebView2NavigationCompletedEventHandler>(
                [](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                    SetTimer(g.window, kCapture, 1500, nullptr);
                    return S_OK;
                }).Get(),
            nullptr);
    resize();
    std::wstring url = std::wstring(L"https://") + kHost + L"/index.html";
    if (!g.page.empty())
        url += L"#" + g.page;
    g.view->Navigate(url.c_str());
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_SIZE:
        resize();
        return 0;
    case WM_DPICHANGED: {
        const RECT* suggested = reinterpret_cast<const RECT*>(lparam);
        SetWindowPos(window, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                     suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_MOUSEACTIVATE:
        if (g.no_activate)
            return MA_NOACTIVATE;
        break;
    case WM_TIMER:
        if (wparam == kCapture) {
            KillTimer(window, kCapture);
            capture();
        } else if (wparam == kEval) {
            KillTimer(window, kEval);
            std::string script;
            if (g.view && read_file(g.eval, script))
                g.view->ExecuteScript(widen(script).c_str(), nullptr);
        }
        return 0;
    case kReply: {
        std::wstring* message_text = reinterpret_cast<std::wstring*>(lparam);
        if (g.view)
            g.view->PostWebMessageAsJson(message_text->c_str());
        delete message_text;
        return 0;
    }
    case WM_ERASEBKGND: {
        RECT area;
        GetClientRect(window, &area);
        HBRUSH brush = CreateSolidBrush(kBackground);
        FillRect(reinterpret_cast<HDC>(wparam), &area, brush);
        DeleteObject(brush);
        return 1;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

struct Screens {
    RECT primary = {}, secondary = {};
    bool have_secondary = false;
};

BOOL CALLBACK screen_cb(HMONITOR monitor, HDC, LPRECT, LPARAM param)
{
    Screens* screens = reinterpret_cast<Screens*>(param);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info))
        return TRUE;
    if (info.dwFlags & MONITORINFOF_PRIMARY)
        screens->primary = info.rcWork;
    else if (!screens->have_secondary) {
        screens->secondary = info.rcWork;
        screens->have_secondary = true;
    }
    return TRUE;
}

bool create_window(HINSTANCE instance)
{
    WNDCLASSEXW cls = {};
    cls.cbSize = sizeof(cls);
    cls.lpfnWndProc = window_proc;
    cls.hInstance = instance;
    cls.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    cls.hIcon = LoadIconW(nullptr, reinterpret_cast<LPCWSTR>(IDI_APPLICATION));
    cls.lpszClassName = L"EfmiVulkanLauncher";
    if (!RegisterClassExW(&cls))
        return false;

    Screens screens;
    EnumDisplayMonitors(nullptr, nullptr, screen_cb, reinterpret_cast<LPARAM>(&screens));
    // A test's window goes to the secondary screen and never takes the focus.
    const RECT area = g.no_activate && screens.have_secondary ? screens.secondary : screens.primary;
    const POINT centre = {(area.left + area.right) / 2, (area.top + area.bottom) / 2};
    UINT dpi = 96;
    if (HMONITOR monitor = MonitorFromPoint(centre, MONITOR_DEFAULTTOPRIMARY)) {
        using GetDpi = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
        HMODULE shcore = LoadLibraryW(L"shcore.dll");
        UINT y = 96;
        if (auto get = shcore ? reinterpret_cast<GetDpi>(GetProcAddress(shcore, "GetDpiForMonitor")) : nullptr)
            get(monitor, 0, &dpi, &y);
    }
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    const DWORD ex_style = g.no_activate ? WS_EX_NOACTIVATE : 0;
    RECT frame = {0, 0, MulDiv(kWidth, dpi, 96), MulDiv(kHeight, dpi, 96)};
    AdjustWindowRectExForDpi(&frame, style, FALSE, ex_style, dpi);
    const int width = frame.right - frame.left, height = frame.bottom - frame.top;
    g.window = CreateWindowExW(ex_style, cls.lpszClassName, L"EFMI Vulkan", style, centre.x - width / 2,
                               centre.y - height / 2, width, height, nullptr, nullptr, instance, nullptr);
    if (!g.window)
        return false;
    // The title bar in the page's colour (Windows 11; ignored before).
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(g.window, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
    DwmSetWindowAttribute(g.window, 35 /* DWMWA_CAPTION_COLOR */, &kBackground, sizeof(kBackground));
    ShowWindow(g.window, g.no_activate ? SW_SHOWNOACTIVATE : SW_SHOW);
    return true;
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    fs::path named;
    wchar_t env[32768];
    const DWORD n = GetEnvironmentVariableW(L"EFVL_DATA_DIR", env, 32768);
    if (n > 0 && n < 32768)
        named = std::wstring(env, n);
    for (int i = 1; i < argc; i++) {
        const std::wstring key = argv[i];
        if (key == L"--dev") g.dev = true;
        else if (key == L"--no-activate") g.no_activate = true;
        else if (key == L"--data" && i + 1 < argc) named = argv[++i];
        else if (key == L"--page" && i + 1 < argc) g.page = argv[++i];
        else if (key == L"--screenshot" && i + 1 < argc) g.screenshot = argv[++i];
        else if (key == L"--eval" && i + 1 < argc) g.eval = argv[++i];
        else if (key == L"--eval-out" && i + 1 < argc) g.eval_out = argv[++i];
    }
    if (!g.screenshot.empty() || !g.eval.empty())
        g.no_activate = true;
    const DWORD api = GetEnvironmentVariableW(L"EFVL_RELEASE_API", env, 32768);
    if (api > 0 && api < 32768)
        g.api = narrow(std::wstring(env, api));
    g.data = data_dir(named);
    g.strings = read_json(program_dir() / L"ui" / L"strings.zh-CN.json");

    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) || !create_window(instance))
        return 1;

    const fs::path user_data = g.data / L"webview";
    std::error_code ec;
    fs::create_directories(user_data, ec);
    const HRESULT made = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, user_data.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                if (FAILED(result) || !environment) {
                    PostMessageW(g.window, WM_CLOSE, 0, 0);
                    return S_OK;
                }
                environment->CreateCoreWebView2Controller(
                    g.window, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                                  [](HRESULT made_controller, ICoreWebView2Controller* controller) -> HRESULT {
                                      if (SUCCEEDED(made_controller) && controller)
                                          on_view(controller);
                                      else
                                          PostMessageW(g.window, WM_CLOSE, 0, 0);
                                      return S_OK;
                                  }).Get());
                return S_OK;
            }).Get());
    if (FAILED(made)) {
        // No WebView2 runtime: said plainly; the game can still be started with efvl_core.exe start.
        ShowWindow(g.window, SW_HIDE);
        MessageBoxW(nullptr,
                    text("webview_missing",
                         L"The Microsoft Edge WebView2 Runtime is not installed.\n\nInstall it, or start the game with:\n"
                         L"efvl_core.exe start")
                        .c_str(),
                    L"EFMI Vulkan", MB_OK | MB_ICONWARNING);
        return 3;
    }

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    g.view.Reset();
    g.controller.Reset();
    CoUninitialize();
    return static_cast<int>(message.wParam);
}
