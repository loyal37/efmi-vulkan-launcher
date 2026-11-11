"""Checks the injection's edges without the game: what a start tells when something is wrong.

    python tests/check_inject.py            everything that needs no administrator rights
    python tests/check_inject.py --elevated the elevated path as well: ONE UAC prompt to accept

The "game" is a copy of the EFMI Vulkan project's sandbox program. For the elevated path a second
copy gets a manifest that requires administrator rights (as Endfield.exe has), so that the core
has to start itself again elevated and inject from there.
"""
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import build  # noqa: E402

CORE = ROOT / "dist" / "efvl_core.exe"
SANDBOX = Path(os.environ.get("EFVL_SANDBOX", "F:/efmi vulkan/dist/efmi_vk_sandbox.exe"))
RELEASES = Path(os.environ.get("EFVL_RELEASES", "F:/efmi vulkan/.worktrees/fix/build/release"))
INSTALLED_CORE = Path(os.environ.get("EFVL_EFMI_CORE", "D:/XXMI-Launcher-Portable-v2.1.6/EFMI/Core/EFMI"))
ARCHIVE = RELEASES / "EFMI-Vulkan-2.1.1.zip"

passed = failed = 0
sys.stdout.reconfigure(errors="replace")


def check(ok, what, detail=None):
    global passed, failed
    if ok:
        passed += 1
    else:
        failed += 1
        print("  FAIL: %s%s" % (what, "" if detail is None else " (%s)" % (detail,)))
    return ok


def core(*args, timeout=180):
    done = subprocess.run([str(CORE), *map(str, args)], capture_output=True, text=True, encoding="utf-8", timeout=timeout)
    try:
        return json.loads(done.stdout.strip().splitlines()[-1])
    except (ValueError, IndexError):
        return {"code": "no JSON", "stdout": done.stdout, "stderr": done.stderr}


def running(image_folder: Path) -> int:
    """How many processes run from a program in this folder."""
    out = subprocess.run(["powershell", "-NoProfile", "-Command",
                          "@(Get-Process | Where-Object { $_.Path -like '%s*' }).Count" % str(image_folder)],
                         capture_output=True, text=True).stdout.strip()
    return int(out or 0)


for needed in (CORE, SANDBOX, INSTALLED_CORE, ARCHIVE):
    if not needed.exists():
        print("missing: %s" % needed)
        sys.exit(1)

work = ROOT / "build" / "tests" / ("inject-%d" % int(time.time()))
# Every folder on the way is named outside ASCII: the game's, the EFMI folder, the layer's.
game = work / "\u6e38\u620f game"
efmi = work / "EFMI \u6a21\u7ec4"
layer = work / "\u5c42" / "EFMI-Vulkan"
data = work / "data"
game.mkdir(parents=True)
(efmi / "Mods").mkdir(parents=True)
shutil.copytree(INSTALLED_CORE, efmi / "Core" / "EFMI")
exe = game / "Endfield.exe"
shutil.copy2(SANDBOX, exe)
core("set", "--data", data, "--game", game, "--efmi", efmi, "--layer", layer)
answer = core("install", "--data", data, "--archive", ARCHIVE)
check(answer.get("ok"), "the layer installs into a folder named outside ASCII", answer)
dll = layer / "efmi_vk_inject.dll"
quiet = ["--env", "VK_LOADER_LAYERS_DISABLE=~implicit~"]

# A whole start with every path outside ASCII.
log = work / "layer.log"
answer = core("start", "--data", data, "--wait", "--no-elevate", "--args", "--seconds 4", "--env", "EFMI_VK_LOG=%s" % log, *quiet)
text = log.read_text(encoding="utf-8", errors="replace") if log.is_file() else ""
check(answer.get("code") == "ok" and answer.get("exit_code") == 0, "a start with paths outside ASCII succeeds", answer)
check("loaded into Endfield.exe" in text and "exports redirected" in text, "the layer loaded from a folder named outside ASCII")
used = re.search(r"character packages: \d+ from the folder generation of (.*)", text)
check(used is not None and Path(used.group(1).strip()) == efmi / "Mods",
      "the layer read the EFMI folder named outside ASCII from efmi_vk.cfg", used.group(1) if used else text[-400:])

# A program that ends at once after the start is told as such, with its exit code.
answer = core("start", "--data", data, "--no-elevate", "--args", "--no-such-argument", *quiet)
check(answer.get("ok") is False and answer.get("code") == "exited_early" and answer.get("exit_code") == 2,
      "a program that ends right after the start is told, with its exit code", answer)
started = time.monotonic()
answer = core("start", "--data", data, "--no-elevate", "--settle", "3000", "--args", "--seconds 20", *quiet)
check(answer.get("code") == "ok" and 2.5 < time.monotonic() - started, "a program that keeps running is a good start", answer)
check(running(game) == 1, "and it is running")
answer = core("start", "--data", data, "--no-elevate")
check(answer.get("code") == "game_running", "a second start while it runs is refused", answer)
subprocess.run(["powershell", "-NoProfile", "-Command",
                "Get-Process Endfield -ErrorAction SilentlyContinue | Where-Object { $_.Path -like '%s*' } | Stop-Process -Force" % str(work)],
               capture_output=True)
time.sleep(1)

# The launcher's own log records each start.
record = (data / "launcher.log").read_text(encoding="utf-8") if (data / "launcher.log").is_file() else ""
check("exited_early" in record and "Endfield.exe" in record and "efmi_vk_inject.dll" in record,
      "the launcher's log records the starts and how they ended", record[-300:])

# Failures before the program runs leave no process behind.
bad = work / "bad.dll"
bad.write_text("not a DLL", encoding="ascii")
answer = core("launch", "--exe", exe, "--dll", bad, "--no-elevate", "--args", "--seconds 20")
check(answer.get("code") == "inject_failed" and "bad.dll" in answer.get("detail", ""), "a file that is no DLL is told as a failed load", answer)
check(running(game) == 0, "and the program it was meant for never ran")
answer = core("launch", "--exe", exe, "--dll", dll, "--dll", bad, "--no-elevate", "--args", "--seconds 20")
check(answer.get("code") == "inject_failed" and running(game) == 0, "a second DLL that fails ends the program too", answer)
narrow = Path(os.environ["SystemRoot"]) / "SysWOW64" / "where.exe"
answer = core("launch", "--exe", narrow, "--dll", dll, "--no-elevate")
check(answer.get("code") == "wrong_architecture", "a 32-bit program is refused", answer)

# A program that requires administrator rights: told when elevation is not allowed.
admin_folder = work / "admin"
admin_folder.mkdir()
admin = admin_folder / "Endfield.exe"
shutil.copy2(SANDBOX, admin)
manifest = work / "admin.manifest"
manifest.write_text(
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
    '<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0"><trustInfo xmlns="urn:schemas-microsoft-com:asm.v3">'
    '<security><requestedPrivileges><requestedExecutionLevel level="requireAdministrator" uiAccess="false"/>'
    '</requestedPrivileges></security></trustInfo></assembly>\n', encoding="utf-8")
made = subprocess.run('call "%s" >nul 2>nul && mt.exe -nologo -manifest "%s" "-outputresource:%s;#1"' % (build.find_vcvars(), manifest, admin),
                      shell=True, capture_output=True, text=True)
check(made.returncode == 0, "the stand-in that requires administrator rights was made", made.stdout + made.stderr)
answer = core("launch", "--exe", admin, "--dll", dll, "--no-elevate", "--args", "--seconds 3")
check(answer.get("code") == "elevation_required" and answer.get("error") == 740, "a program that requires administrator rights says so", answer)

if "--elevated" in sys.argv:
    # The whole elevated path: this (not elevated) core starts itself elevated, which injects and answers.
    core("set", "--data", data, "--game", admin_folder)
    log = work / "layer-admin.log"
    result = work / "admin-result.json"
    answer = core("start", "--data", data, "--wait", "--result", result, "--args", "--seconds 4",
                  "--env", "EFMI_VK_LOG=%s" % log, *quiet, timeout=300)
    text = log.read_text(encoding="utf-8", errors="replace") if log.is_file() else ""
    check(answer.get("code") == "ok" and answer.get("exit_code") == 0, "the elevated start succeeds and its answer comes back", answer)
    check(json.loads(result.read_text(encoding="utf-8")).get("code") == "ok" if result.is_file() else False, "the result file holds the answer")
    check("loaded into Endfield.exe" in text and "exports redirected" in text and "already loaded" not in text,
          "the layer loaded into the elevated program before the Vulkan loader", text[:200])
else:
    print("  (the elevated path was not run: --elevated, one UAC prompt)")

total = passed + failed
print("artifacts: %s" % work)
print("inject: %d/%d checks passed" % (passed, total))
sys.exit(0 if failed == 0 and total else 1)
