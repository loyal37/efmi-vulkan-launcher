"""Checks installing and updating EFMI Vulkan from the network, against a stand-in server on this
machine: never the real network, never the real EFMI folder.

    python tests/check_update.py
"""
import getpass
import http.server
import json
import os
import re
import shutil
import subprocess
import sys
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / "dist" / "efvl_core.exe"
RELEASES = Path(os.environ.get("EFVL_RELEASES", "F:/efmi vulkan/.worktrees/fix/build/release"))

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
        return json.loads(done.stdout.strip().splitlines()[-1])
    except (ValueError, IndexError):
        return {"code": "no JSON", "stdout": done.stdout, "stderr": done.stderr}


class Server:
    """Answers as GitHub's latest-release endpoint and serves the asset; records what it was asked."""

    def __init__(self):
        self.version = ""
        self.asset = b""
        self.status = 200
        self.requests = []
        outer = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass

            def do_GET(self):
                outer.requests.append((self.path, dict(self.headers)))
                name = "EFMI-Vulkan-%s.zip" % outer.version
                if outer.status != 200:
                    self.send_response(outer.status)
                    self.end_headers()
                elif self.path == "/repos/loyal37/efmi-vulkan/releases/latest":
                    body = json.dumps({"tag_name": "v" + outer.version, "assets": [
                        {"name": "notes.txt", "browser_download_url": "http://127.0.0.1:%d/notes.txt" % outer.port},
                        {"name": name, "size": len(outer.asset),
                         "browser_download_url": "http://127.0.0.1:%d/download/%s" % (outer.port, name)}]}).encode()
                    self.send_response(200)
                    self.send_header("Content-Type", "application/json")
                    self.send_header("Content-Length", str(len(body)))
                    self.end_headers()
                    self.wfile.write(body)
                elif self.path == "/download/" + name:
                    self.send_response(200)
                    self.send_header("Content-Length", str(len(outer.asset)))
                    self.end_headers()
                    self.wfile.write(outer.asset)
                else:
                    self.send_response(404)
                    self.end_headers()

        self.httpd = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.port = self.httpd.server_address[1]
        self.api = "http://127.0.0.1:%d/repos/loyal37/efmi-vulkan/releases/latest" % self.port
        threading.Thread(target=self.httpd.serve_forever, daemon=True).start()

    def stop(self):
        self.httpd.shutdown()
        self.httpd.server_close()


def archive_of(version: str) -> Path:
    return RELEASES / ("EFMI-Vulkan-%s.zip" % version)


if not CORE.exists() or not archive_of("2.1.1").exists() or not archive_of("2.1.0").exists():
    print("missing: the core or the release archives 2.1.0 and 2.1.1")
    sys.exit(1)

work = ROOT / "build" / "tests" / ("update-%d" % int(time.time()))
data = work / "data"
layer = work / "EFMI-Vulkan"
efmi = work / "EFMI"
(efmi / "Mods").mkdir(parents=True)
(efmi / "Core" / "EFMI").mkdir(parents=True)
core("set", "--data", data, "--layer", layer, "--efmi", efmi)
server = Server()
dll = layer / "efmi_vk_inject.dll"

# Nothing installed: the latest release is offered and installed.
server.version, server.asset = "2.1.0", archive_of("2.1.0").read_bytes()
answer = core("check-update", "--data", data, "--api", server.api)
check(answer.get("ok") and answer.get("available") is True and answer.get("latest") == "2.1.0" and answer.get("installed") == "",
      "the latest release is offered when nothing is installed", answer)
check(not layer.exists(), "looking for an update installs nothing")
answer = core("update", "--data", data, "--api", server.api)
check(answer.get("ok") and answer.get("code") == "ok" and answer.get("version") == "2.1.0", "the release installs from the network", answer)
state = core("status", "--data", data)
check(state["layer"]["installed"] and state["layer"]["version"] == "2.1.0", "the installed version is the downloaded one", state)
record = json.loads((layer / "efvl-install.json").read_text(encoding="utf-8")) if dll.exists() else {}
check(record.get("source") == "network", "the install record names the network as its source", record)
check("m2_mods = " + (efmi / "Mods").as_posix() in (layer / "efmi_vk.cfg").read_text(encoding="utf-8").splitlines() if dll.exists() else False,
      "the downloaded layer is pointed at the EFMI folder")
check(not list((data / "downloads").glob("*")) if (data / "downloads").exists() else True, "the downloaded archive is not kept")

# What the requests carried: a generic program name, nothing of the user or the machine.
user = getpass.getuser().lower()
sent = json.dumps(server.requests).lower()
agents = {headers.get("User-Agent", "") for _, headers in server.requests}
check(len(server.requests) >= 3 and agents == {"EFMI-Vulkan-Launcher"}, "every request names a generic program", agents)
check(user not in sent and "efmi vulkan launcher" not in sent and str(work).lower().replace("\\", "/") not in sent.replace("\\\\", "/"),
      "no request carries the user's name or a path")
check(all(set(headers) <= {"Host", "User-Agent", "Accept", "Connection", "Cache-Control", "Pragma", "Accept-Encoding"}
          for _, headers in server.requests), "no header beyond the plain ones", [sorted(h) for _, h in server.requests][:1])

# The same version again, an older one, a newer one.
answer = core("check-update", "--data", data, "--api", server.api)
check(answer.get("ok") and answer.get("available") is False and answer.get("installed") == "2.1.0", "the same version is no update", answer)
answer = core("update", "--data", data, "--api", server.api)
check(answer.get("ok") and answer.get("code") == "up_to_date", "an update with nothing newer changes nothing", answer)
server.version, server.asset = "2.1.1", archive_of("2.1.1").read_bytes()
(layer / "settings").mkdir(parents=True, exist_ok=True)
(layer / "settings" / "keep.ini").write_text("kept", encoding="ascii")
answer = core("check-update", "--data", data, "--api", server.api)
check(answer.get("available") is True and answer.get("latest") == "2.1.1", "a newer release is offered", answer)
answer = core("update", "--data", data, "--api", server.api)
state = core("status", "--data", data)
check(answer.get("ok") and state["layer"]["version"] == "2.1.1", "the newer release installs", answer)
check((layer / "settings" / "keep.ini").read_text(encoding="ascii") == "kept", "the update keeps the settings folder")
server.version, server.asset = "2.0.9", b"never downloaded"
before = len(server.requests)
answer = core("update", "--data", data, "--api", server.api)
check(answer.get("ok") and answer.get("code") == "up_to_date" and answer.get("latest") == "2.0.9", "an older release online is no update", answer)
check(len(server.requests) == before + 1, "nothing is downloaded when there is no update")

# A damaged download is refused before anything installed is touched.
installed = dll.read_bytes()
server.version, server.asset = "2.1.2", archive_of("2.1.1").read_bytes()[:300000]
answer = core("update", "--data", data, "--api", server.api)
check(answer.get("ok") is False and answer.get("code") == "archive_invalid", "a damaged download is refused", answer)
state = core("status", "--data", data)
check(dll.read_bytes() == installed and state["layer"]["version"] == "2.1.1", "a refused download leaves the installed layer as it was")

# The server answering with an error, and no server at all.
server.status = 403
answer = core("check-update", "--data", data, "--api", server.api)
check(answer.get("ok") is False and answer.get("code") == "network_failed", "an error answer is told as a network failure", answer)
server.status = 200
server.stop()
started = time.monotonic()
answer = core("update", "--data", data, "--api", server.api)
check(answer.get("ok") is False and answer.get("code") == "network_failed", "no server is told as a network failure", answer)
check(time.monotonic() - started < 30, "and told soon")
answer = core("install", "--data", data, "--archive", archive_of("2.1.0"))
check(answer.get("ok") and answer.get("version") == "2.1.0", "a local archive still installs without the network", answer)

total = passed + failed
print("artifacts: %s" % work)
print("update: %d/%d checks passed" % (passed, total))
sys.exit(0 if failed == 0 and total else 1)
