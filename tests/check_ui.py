"""Checks the window: the real program with its page, the page's buttons clicked by a script.

    python tests/check_ui.py

The window opens without taking the focus (on the secondary screen when there is one). The release
lookup goes to a stand-in server on this machine; the "game" is a copy of the sandbox program. The
native folder and file dialogs are not part of this check.
"""
import http.server
import json
import os
import shutil
import subprocess
import sys
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DIST = ROOT / "dist"
SHELL = DIST / "EFMI Vulkan Launcher.exe"
CORE = DIST / "efvl_core.exe"
SANDBOX = Path(os.environ.get("EFVL_SANDBOX", "F:/efmi vulkan/dist/efmi_vk_sandbox.exe"))
RELEASES = Path(os.environ.get("EFVL_RELEASES", "F:/efmi vulkan/.worktrees/fix/build/release"))
INSTALLED_CORE = Path(os.environ.get("EFVL_EFMI_CORE", "D:/XXMI-Launcher-Portable-v2.1.6/EFMI/Core/EFMI"))
VERSION = "2.1.1"

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


SCRIPT = r"""
(async () => {
  const $ = (id) => document.getElementById(id);
  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
  const until = async (test, ms) => { const end = Date.now() + ms; while (Date.now() < end) { if (test()) return true; await sleep(50); } return false; };
  const toast = async () => { const shown = await until(() => $("toast").classList.contains("show"), 30000); const text = shown ? $("toast").textContent : null; $("toast").classList.remove("show"); return text; };
  const r = {};
  try {
    await until(() => $("start-text").textContent !== "", 10000);
    await sleep(500);
    r.first_start = $("start-text").textContent;
    r.first_version = $("version").textContent;
    $("start").click();
    r.setup_opens = document.body.classList.contains("open");
    r.labels = [...document.querySelectorAll(".drawer label")].map((e) => e.textContent);
    r.values = [...document.querySelectorAll(".row .value")].map((e) => e.title);
    r.first_state = $("layer-state").textContent;
    r.first_install = $("install").textContent;
    $("install").click();
    r.install_toast = await toast();
    r.state = $("layer-state").textContent;
    r.install = $("install").textContent;
    r.version = $("version").textContent;
    r.start = $("start-text").textContent;
    $("install").click();
    r.again_toast = await toast();
    $("close").click();
    r.closed = !document.body.classList.contains("open");
    $("start").click();
    r.busy = $("start").classList.contains("busy");
    r.starting = $("start-text").textContent;
    await sleep(4000);
    r.start_toast = $("toast").classList.contains("show") ? $("toast").textContent : null;
    r.still_busy = $("start").classList.contains("busy");
    r.text = document.body.innerText;
  } catch (e) { r.error = String(e); }
  window.chrome.webview.postMessage({ cmd: "test_done", result: r });
})();
"""

for needed in (SHELL, CORE, SANDBOX, INSTALLED_CORE, RELEASES / ("EFMI-Vulkan-%s.zip" % VERSION)):
    if not needed.exists():
        print("missing: %s" % needed)
        sys.exit(1)

work = ROOT / "build" / "tests" / ("ui-%d" % int(time.time()))
data, game, efmi, layer = work / "data", work / "game", work / "EFMI", work / "EFMI-Vulkan"
game.mkdir(parents=True)
(efmi / "Mods").mkdir(parents=True)
shutil.copytree(INSTALLED_CORE, efmi / "Core" / "EFMI")
shutil.copy2(SANDBOX, game / "Endfield.exe")
subprocess.run([str(CORE), "set", "--data", str(data), "--game", str(game), "--efmi", str(efmi), "--layer", str(layer)],
               capture_output=True, check=True)
# The prefill would look for the real XXMI configuration: these paths are set already, and it is marked done.
settings = json.loads((data / "settings.json").read_text(encoding="utf-8"))
settings["prefilled"] = True
(data / "settings.json").write_text(json.dumps(settings), encoding="utf-8")

asset = (RELEASES / ("EFMI-Vulkan-%s.zip" % VERSION)).read_bytes()


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        name = "EFMI-Vulkan-%s.zip" % VERSION
        if self.path.endswith("/releases/latest"):
            body = json.dumps({"tag_name": "v" + VERSION, "assets": [
                {"name": name, "browser_download_url": "http://127.0.0.1:%d/%s" % (self.server.server_address[1], name)}]}).encode()
        elif self.path == "/" + name:
            body = asset
        else:
            self.send_response(404)
            self.end_headers()
            return
        self.send_response(200)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


httpd = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
threading.Thread(target=httpd.serve_forever, daemon=True).start()

script = work / "script.js"
script.write_text(SCRIPT, encoding="utf-8")
out = work / "result.json"
env = dict(os.environ, EFVL_RELEASE_API="http://127.0.0.1:%d/releases/latest" % httpd.server_address[1])
try:
    done = subprocess.run([str(SHELL), "--data", str(data), "--eval", str(script), "--eval-out", str(out)], env=env, timeout=180)
finally:
    # The stand-in game runs for half a minute by itself; it is ended here, found by its path.
    subprocess.run(["powershell", "-NoProfile", "-Command",
                    "Get-Process Endfield -ErrorAction SilentlyContinue | Where-Object { $_.Path -like '%s*' } | Stop-Process -Force" % str(work)],
                   capture_output=True)
    httpd.shutdown()

strings = json.loads((ROOT / "ui" / "strings.zh-CN.json").read_text(encoding="utf-8"))
r = json.loads(out.read_text(encoding="utf-8")) if out.is_file() else {}
check(done.returncode == 0 and r and "error" not in r, "the window ran the script to its end", r.get("error", done.returncode))
check(r.get("first_start") == strings["setup"] and r.get("first_version") == "", "without the layer the main button leads to the settings", r)
check(r.get("setup_opens") is True, "and opens them")
check(r.get("labels") == [strings["game"], strings["efmi"], strings["layer"]], "the settings name three paths", r.get("labels"))
check([Path(v) for v in r.get("values", [])] == [game, efmi, layer], "and show the saved ones", r.get("values"))
check(r.get("first_state") == strings["not_installed"] and r.get("first_install") == strings["install"], "the layer is shown as not installed", r)
check(r.get("install_toast") == strings["done"], "the install button installs from the network", r.get("install_toast"))
check(r.get("state") == VERSION and r.get("version") == VERSION and r.get("install") == strings["update"], "the installed version is shown", r)
check((layer / "efmi_vk_inject.dll").is_file(), "the layer is in its folder")
check(r.get("start") == strings["start"], "the main button now starts the game", r.get("start"))
check(r.get("again_toast") == strings["up_to_date"], "a second update says there is nothing newer", r.get("again_toast"))
check(r.get("closed") is True, "the settings close")
check(r.get("busy") is True and r.get("starting") == strings["starting"], "a start shows that it is starting", r)
check(r.get("start_toast") is None and r.get("still_busy") is True, "the start reports no error", r.get("start_toast"))
log = layer / "efmi_vk_layer.log"
text = log.read_text(encoding="utf-8", errors="replace") if log.is_file() else ""
check("loaded into Endfield.exe" in text and "exports redirected" in text and "already loaded" not in text,
      "the start from the window loaded the layer into the program before the Vulkan loader", text[:200])
shown = r.get("text", "")
check("Vulkan" not in shown.replace("EFMI Vulkan", "").replace("EFMI-Vulkan", "").replace("VULKAN", "") and "DX11" not in shown and "DirectX" not in shown,
      "the page names no renderer", shown)

total = passed + failed
print("artifacts: %s" % work)
print("ui: %d/%d checks passed" % (passed, total))
sys.exit(0 if failed == 0 and total else 1)
