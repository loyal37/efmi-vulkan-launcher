"""Checks that the interface's Chinese text lives in ui/strings.zh-CN.json and nowhere else.

    python scripts/check_text.py

Every file git knows or would add is read as UTF-8; a Chinese character outside the strings file
fails the check. A string that must hold one elsewhere (a test's folder name) is spelled with \u.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ALLOWED = {"ui/strings.zh-CN.json"}
BINARY = {".png", ".ico", ".zip", ".exe", ".dll"}
CJK = re.compile("[⺀-鿿豈-﫿＀-￯]")

names = subprocess.run(["git", "ls-files", "--cached", "--others", "--exclude-standard"], cwd=ROOT,
                       capture_output=True, text=True, encoding="utf-8").stdout.splitlines()
failures = 0
for name in names:
    path = ROOT / name
    if name in ALLOWED or path.suffix.lower() in BINARY or not path.is_file():
        continue
    try:
        text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        print("FAIL %s: not UTF-8" % name)
        failures += 1
        continue
    for number, line in enumerate(text.splitlines(), 1):
        if CJK.search(line):
            print("FAIL %s:%d: Chinese text outside the strings file" % (name, number))
            failures += 1
print("text: %d files, %s" % (len(names), "FAILED" if failures else "passed"))
sys.exit(1 if failures else 0)
