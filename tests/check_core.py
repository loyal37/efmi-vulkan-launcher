"""Checks the core end to end without the game: settings, prefill, install, and the start with injection.

    python tests/check_core.py

Everything happens in a folder of its own under build/tests. The "game" is a copy of the EFMI Vulkan
project's sandbox program (a Vulkan test program) named Endfield.exe; the EFMI folder is a stand-in
with a copy of the installed Core and no Mods; the layer comes from a release archive of that
project. Nothing outside this project is written, and the real game is never started.
"""
import json
import os
import re
import shutil
import subprocess
import sys
import time
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / "dist" / "efvl_core.exe"
SANDBOX = Path(os.environ.get("EFVL_SANDBOX", "F:/efmi vulkan/dist/efmi_vk_sandbox.exe"))
RELEASES = Path(os.environ.get("EFVL_RELEASES", "F:/efmi vulkan/.worktrees/fix/build/release"))
INSTALLED_CORE = Path(os.environ.get("EFVL_EFMI_CORE", "D:/XXMI-Launcher-Portable-v2.1.6/EFMI/Core/EFMI"))

passed = failed = 0


def check(ok, what, detail=None):
    global passed, failed
    if ok:
        passed += 1
    else:
        failed += 1
        print("  FAIL: %s%s" % (what, "" if detail is None else " (%s)" % (detail,)))
    return ok


def core(*args, timeout=120):
    done = subprocess.run([str(CORE), *map(str, args)], capture_output=True, text=True, encoding="utf-8", timeout=timeout)
    try:
        return json.loads(done.stdout.strip().splitlines()[-1]), done.returncode
    except (ValueError, IndexError):
        return {"code": "no JSON", "stdout": done.stdout, "stderr": done.stderr}, done.returncode


def newest_archive() -> Path:
    def key(p):
        return tuple(int(x) for x in re.search(r"(\d)\.(\d)\.(\d)\.zip$", p.name).groups())
    return max(RELEASES.glob("EFMI-Vulkan-*.zip"), key=key)


for needed in (CORE, SANDBOX, INSTALLED_CORE):
    if not needed.exists():
        print("missing: %s" % needed)
        sys.exit(1)

work = ROOT / "build" / "tests" / ("core-%d" % int(time.time()))
# A folder name outside ASCII on the way: paths are UTF-8 inside the core and UTF-16 at Windows.
game = work / "game \u6e38\u620f"
efmi = work / "EFMI"
layer = work / "layer" / "EFMI-Vulkan"
data = work / "data"
game.mkdir(parents=True)
(efmi / "Mods").mkdir(parents=True)
shutil.copytree(INSTALLED_CORE, efmi / "Core" / "EFMI")
shutil.copy2(SANDBOX, game / "Endfield.exe")
archive = newest_archive()
version = re.search(r"(\d\.\d\.\d)\.zip$", archive.name).group(1)

# Settings: empty at first, prefilled from a stand-in XXMI configuration, read back.
state, _ = core("status", "--data", data)
check(state.get("game_path") == "" and state.get("ready") is False, "a new data folder has no paths", state)
xxmi = work / "xxmi" / "XXMI Launcher Config.json"
xxmi.parent.mkdir()
xxmi.write_text(json.dumps({"Importers": {"EFMI": {"Importer": {
    "importer_folder": efmi.as_posix(), "game_folder": game.as_posix()}}}}), encoding="utf-8")
xxmi_before = xxmi.read_bytes()
answer, code = core("prefill", "--data", data, "--xxmi-config", xxmi)
check(code == 0 and answer.get("filled") is True, "prefill takes both paths from the XXMI configuration", answer)
state, _ = core("status", "--data", data)
check(Path(state.get("game_path", "")) == game and Path(state.get("efmi_path", "")) == efmi,
      "the prefilled paths are read back", state)
check(state.get("game_ok") and state.get("efmi_ok"), "both prefilled paths are usable", state)
check(xxmi.read_bytes() == xxmi_before, "the XXMI configuration is unchanged")

# Paths set by hand are saved and read back; an unusable one is told.
core("set", "--data", data, "--game", work / "nowhere", "--layer", layer)
state, _ = core("status", "--data", data)
check(Path(state["game_path"]) == work / "nowhere" and state["game_ok"] is False, "a folder without the game is not usable", state)
check(Path(state["layer_path"]) == layer, "the layer's folder is a setting", state)
check(Path(state["efmi_path"]) == efmi, "a path not named keeps its value", state)
answer, code = core("start", "--data", data, "--no-elevate")
check(code == 1 and answer.get("code") == "game_not_set", "no start without the game", answer)
core("set", "--data", data, "--game", game)
saved = json.loads((data / "settings.json").read_text(encoding="utf-8"))
check(Path(saved["game_path"]) == game and Path(saved["efmi_path"]) == efmi, "settings.json holds the paths", saved)

# Install: refused archives change nothing; a good one is installed with its version.
answer, code = core("start", "--data", data, "--no-elevate")
check(answer.get("code") == "layer_not_installed", "no start before the layer is installed", answer)
bad = work / "bad" / "EFMI-Vulkan-9.9.9.zip"
bad.parent.mkdir()
bad.write_bytes(archive.read_bytes()[:200000])
answer, code = core("install", "--data", data, "--archive", bad)
check(code == 1 and answer.get("code") == "archive_invalid", "a cut archive is refused", answer)
with zipfile.ZipFile(bad, "w") as z:
    z.writestr("EFMI-Vulkan/efmi_vk_inject.dll", b"x")
    z.writestr("EFMI-Vulkan/../../escape.txt", b"x")
answer, code = core("install", "--data", data, "--archive", bad)
check(answer.get("code") == "archive_invalid" and not (work / "escape.txt").exists() and not layer.exists(),
      "an archive with a path outside its folder is refused whole", answer)
with zipfile.ZipFile(bad, "w") as z:
    z.writestr("EFMI-Vulkan/README.txt", b"x")
answer, code = core("install", "--data", data, "--archive", bad)
check(answer.get("code") == "archive_invalid", "an archive without the DLL is refused", answer)
copy = work / "sum" / archive.name
copy.parent.mkdir()
shutil.copy2(archive, copy)
(work / "sum" / (archive.name + ".sha256")).write_text("0" * 64 + "  " + archive.name + "\n", encoding="ascii")
answer, code = core("install", "--data", data, "--archive", copy)
check(answer.get("code") == "checksum_mismatch" and not layer.exists(), "a wrong .sha256 refuses the archive", answer)

answer, code = core("install", "--data", data, "--archive", archive)
check(code == 0 and answer.get("version") == version, "the release archive installs", answer)
state, _ = core("status", "--data", data)
check(state["layer"]["installed"] and state["layer"]["version"] == version and state["ready"],
      "the installed version is shown and the launcher is ready", state)
with zipfile.ZipFile(archive) as z:
    names = [n[len("EFMI-Vulkan/"):] for n in z.namelist() if not n.endswith("/")]
    package_cfg = z.read("EFMI-Vulkan/efmi_vk.cfg").decode("utf-8")
check(all((layer / n).is_file() for n in names), "every file of the package is in the layer's folder")
cfg = (layer / "efmi_vk.cfg").read_text(encoding="utf-8")
check("m2_mods = " + (efmi / "Mods").as_posix() in cfg.splitlines(), "efmi_vk.cfg names the EFMI Mods folder", cfg)
other = lambda text: [ln for ln in text.splitlines() if not ln.startswith("m2_mods")]
check(other(cfg) == other(package_cfg), "every other line of efmi_vk.cfg is the package's")
check(not any(p.name == "EFMI-Vulkan" for p in efmi.iterdir()), "nothing is written into the EFMI folder")

# An update keeps what is not the package's, and the EFMI path is followed.
(layer / "settings").mkdir()
(layer / "settings" / "keep.ini").write_text("kept", encoding="ascii")
(layer / "efmi_vk_inject.dll").write_bytes(b"old")
answer, code = core("install", "--data", data, "--archive", archive)
check(code == 0 and (layer / "efmi_vk_inject.dll").stat().st_size > 1000, "an update replaces the DLL", answer)
check((layer / "settings" / "keep.ini").read_text(encoding="ascii") == "kept", "an update keeps the settings folder")
efmi2 = work / "EFMI2"
shutil.copytree(efmi, efmi2)
core("set", "--data", data, "--efmi", efmi2)
check("m2_mods = " + (efmi2 / "Mods").as_posix() in (layer / "efmi_vk.cfg").read_text(encoding="utf-8").splitlines(),
      "a changed EFMI path is written to efmi_vk.cfg")
core("set", "--data", data, "--efmi", efmi)

# The start: suspended, the DLL loaded, resumed. Judged from the layer's own log.
answer, code = core("launch", "--exe", game / "Endfield.exe", "--dll", work / "none.dll", "--no-elevate")
check(answer.get("code") == "dll_not_found", "a missing DLL is told before anything starts", answer)
log = work / "layer.log"
expected = work / "expected.json"
answer, code = core("start", "--data", data, "--wait", "--no-elevate",
                    "--args", '--seconds 6 --expected "%s"' % expected,
                    "--env", "EFMI_VK_LOG=%s" % log, "--env", "VK_LOADER_LAYERS_DISABLE=~implicit~", timeout=180)
check(code == 0 and answer.get("code") == "ok" and answer.get("pid"), "the start succeeds", answer)
check(answer.get("exit_code") == 0, "the program ran to its end", answer)
check(expected.is_file(), "the program rendered (it wrote its expected draws)")
text = log.read_text(encoding="utf-8", errors="replace") if log.is_file() else ""
check(re.search(r"efmi_vk_layer %s \(release, no probe\) loaded into Endfield\.exe" % re.escape(version), text) is not None,
      "the layer logged that it was loaded into the program", text[:300])
check("entry=inject" in text, "the layer took its inject entry")
check("the Vulkan loader is not loaded yet, waiting for it" in text, "the layer was there before the Vulkan loader")
redirected = re.search(r"bootstrap: .*vulkan-1\.dll at \S+, (\d+) of (\d+) exports redirected(.*)", text)
check(redirected is not None and int(redirected.group(1)) > 0 and "already loaded" not in redirected.group(3),
      "the loader's exports were redirected before anything resolved them", redirected.group(0) if redirected else None)
found = re.search(r"Mods folder found from the layer's folder: (.*)", text)
named = (efmi / "Mods").as_posix() in text or str(efmi / "Mods") in text
check(named or (found is not None and Path(found.group(1).strip()) == efmi / "Mods"), "the layer used the EFMI folder of the settings")
check("crash:" not in text, "no fault in the layer")

total = passed + failed
print("artifacts: %s" % work)
print("core: %d/%d checks passed" % (passed, total))
sys.exit(0 if failed == 0 and total else 1)
