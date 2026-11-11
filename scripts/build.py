"""Builds the launcher into dist/: efvl_core.exe, the window program and the page.

    python scripts/build.py [core] [shell]

MSVC is found through vswhere (or VCVARS names vcvars64.bat). Dependencies are under F:/vulkan
(EFVL_DEPS names another folder): the WebView2 SDK, miniz, nlohmann/json.
"""
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEPS = os.environ.get("EFVL_DEPS", "F:/vulkan")
WEBVIEW2 = os.path.join(DEPS, "webview2-1.0.2903.40", "build", "native")
MINIZ = os.path.join(DEPS, "miniz-3.0.2")
JSON = os.path.join(DEPS, "nlohmann-json-3.11.3")
DIST = os.path.join(ROOT, "dist")
OBJ = os.path.join(ROOT, "build", "obj")
SHELL_EXE = "EFMI Vulkan Launcher.exe"
CORE_EXE = "efvl_core.exe"
CORE_SOURCES = ["util.cpp", "launch.cpp", "settings.cpp", "install.cpp", "actions.cpp", "update.cpp"]
COMMON = ["/nologo", "/std:c++17", "/EHsc", "/W4", "/O2", "/MT", "/utf-8", "/DUNICODE", "/D_UNICODE",
          "/D_CRT_SECURE_NO_WARNINGS", "/Zi", "/wd4132", "/wd4127"]


class BuildError(Exception):
    pass


def find_vcvars() -> str:
    named = os.environ.get("VCVARS")
    if named and os.path.isfile(named):
        return named
    vswhere = os.path.join(os.environ.get("ProgramFiles(x86)", ""), "Microsoft Visual Studio", "Installer", "vswhere.exe")
    if os.path.isfile(vswhere):
        out = subprocess.run([vswhere, "-latest", "-products", "*", "-property", "installationPath"],
                             capture_output=True, text=True).stdout.strip()
        candidate = os.path.join(out, "VC", "Auxiliary", "Build", "vcvars64.bat")
        if os.path.isfile(candidate):
            return candidate
    raise BuildError("MSVC not found: set VCVARS to vcvars64.bat")


def run_cl(vcvars: str, args: list) -> None:
    command = 'chcp 65001 >nul && call "%s" >nul 2>nul && cl %s' % (vcvars, subprocess.list2cmdline(args))
    done = subprocess.run(command, shell=True, cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")
    text = (done.stdout + done.stderr).strip()
    noise = {os.path.basename(a) for a in args if a.endswith((".cpp", ".c"))} | {"Generating Code...", "Compiling..."}
    shown = [line for line in text.splitlines() if line.strip() not in noise]
    if shown:
        print("\n".join(shown))
    if done.returncode:
        raise BuildError("cl failed")


def objects(name: str) -> str:
    folder = os.path.join(OBJ, name)
    os.makedirs(folder, exist_ok=True)
    return folder + os.sep


def core_inputs() -> list:
    return [os.path.join("core", "src", name) for name in CORE_SOURCES] + [os.path.join(MINIZ, "miniz.c")]


def build_core(vcvars: str) -> str:
    out = os.path.join(DIST, CORE_EXE)
    obj = objects("core")
    run_cl(vcvars, COMMON + ["/I" + JSON, "/I" + MINIZ, "/Fo" + obj, "/Fd" + obj + "efvl_core_cl.pdb",
                             os.path.join("core", "src", "main.cpp")] + core_inputs() +
           ["/Fe:" + out, "/link", "/NOLOGO", "/DEBUG", "/PDB:" + obj + "efvl_core.pdb", "/SUBSYSTEM:CONSOLE",
            "/MANIFESTUAC:level='asInvoker' uiAccess='false'",
            "winhttp.lib", "bcrypt.lib", "shell32.lib", "ole32.lib", "advapi32.lib", "psapi.lib", "uuid.lib"])
    return out


def build_shell(vcvars: str) -> str:
    out = os.path.join(DIST, SHELL_EXE)
    obj = objects("shell")
    sources = [os.path.join("shell", "src", name) for name in sorted(os.listdir(os.path.join(ROOT, "shell", "src")))
               if name.endswith(".cpp")]
    run_cl(vcvars, COMMON + ["/I" + JSON, "/I" + MINIZ, "/I" + os.path.join(WEBVIEW2, "include"), "/Icore/src",
                             "/Fo" + obj, "/Fd" + obj + "efvl_shell_cl.pdb"] + sources + core_inputs() +
           ["/Fe:" + out, "/link", "/NOLOGO", "/DEBUG", "/PDB:" + obj + "efvl_shell.pdb", "/SUBSYSTEM:WINDOWS",
            "/MANIFESTUAC:level='asInvoker' uiAccess='false'",
            os.path.join(WEBVIEW2, "x64", "WebView2LoaderStatic.lib"),
            "winhttp.lib", "bcrypt.lib", "shell32.lib", "ole32.lib", "oleaut32.lib", "advapi32.lib", "psapi.lib", "uuid.lib",
            "user32.lib", "gdi32.lib", "dwmapi.lib", "shlwapi.lib", "version.lib", "comdlg32.lib"])
    ui = os.path.join(DIST, "ui")
    if os.path.isdir(ui):
        shutil.rmtree(ui)
    shutil.copytree(os.path.join(ROOT, "ui"), ui)
    return out


def build(targets=()) -> dict:
    targets = list(targets) or ["core", "shell"]
    os.makedirs(DIST, exist_ok=True)
    vcvars = find_vcvars()
    result = {}
    if "core" in targets:
        result["core"] = build_core(vcvars)
    if "shell" in targets:
        result["shell"] = build_shell(vcvars)
    return result


if __name__ == "__main__":
    try:
        for name, path in build(sys.argv[1:]).items():
            print("%s: %s" % (name, path))
    except BuildError as e:
        print("build failed: %s" % e)
        sys.exit(1)
