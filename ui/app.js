// The launcher's page. It asks the shell (window.chrome.webview) and shows what comes back; every
// text on screen comes from strings.zh-CN.json. Opened in a plain browser it shows a stand-in state.
(async function () {
  const $ = (id) => document.getElementById(id);
  const strings = await (await fetch("strings.zh-CN.json")).json();
  const t = (key) => strings[key] || key;
  const host = window.chrome && window.chrome.webview;

  let state = null;
  let nextId = 1;
  const waiting = new Map();

  function ask(cmd, extra) {
    if (!host) return Promise.resolve(standIn(cmd));
    return new Promise((resolve) => {
      const id = nextId++;
      waiting.set(id, resolve);
      host.postMessage(Object.assign({ id, cmd }, extra || {}));
    });
  }
  if (host) {
    host.addEventListener("message", (event) => {
      const resolve = waiting.get(event.data.id);
      if (resolve) {
        waiting.delete(event.data.id);
        resolve(event.data);
      }
    });
  }

  function standIn(cmd) {
    if (cmd === "start") return { ok: true };
    if (cmd === "check_update") return { ok: true, available: false };
    return {
      ok: true, version: "0.1.0", ready: true, game_ok: true, efmi_ok: true,
      game_path: "D:\\Games\\EndField Game", efmi_path: "D:\\XXMI\\EFMI", layer_path: "D:\\Launcher\\EFMI-Vulkan",
      layer: { installed: true, version: "2.1.1" },
    };
  }

  let toastTimer = 0;
  let latest = ""; // a newer release found online, or ""
  const failed = (code) => strings.errors[code] || strings.errors.unknown;
  function toast(message) {
    const el = $("toast");
    el.textContent = message;
    el.classList.add("show");
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => el.classList.remove("show"), 3200);
  }

  function showPath(key, ok) {
    const row = document.querySelector('.row[data-key="' + key + '"]');
    const value = row.querySelector(".value");
    const path = state[key] || "";
    // The value is laid out right to left so that a long path keeps its end; the mark keeps the text itself in order.
    value.textContent = path ? "\u200e" + path : t("not_set");
    value.title = path;
    value.className = "value" + (path ? (ok ? "" : " bad") : " empty");
    row.querySelector(".field .ghost").textContent = path ? t("change") : t("choose");
  }

  function render() {
    const layer = state.layer || {};
    $("version").textContent = layer.installed && layer.version ? layer.version : "";
    $("start-text").textContent = state.ready ? t("start") : t("setup");
    $("start").classList.toggle("quiet", !state.ready);
    showPath("game_path", state.game_ok);
    showPath("efmi_path", state.efmi_ok);
    showPath("layer_path", true);
    const label = $("layer-state");
    label.textContent = layer.installed ? (layer.version || t("installed")) : t("not_installed");
    label.className = "state" + (layer.installed ? "" : " empty");
    $("install").textContent = !layer.installed ? t("install") : latest ? t("update_to") + " " + latest : t("update");
  }

  function take(answer) {
    if (answer && "ready" in answer) {
      state = answer;
      render();
    }
  }

  function open(yes) { document.body.classList.toggle("open", yes); }

  document.querySelectorAll("[data-text]").forEach((el) => { el.textContent = t(el.dataset.text); });
  $("gear").title = t("settings");
  $("close").title = t("close");
  $("gear").addEventListener("click", () => open(true));
  $("close").addEventListener("click", () => open(false));
  $("shade").addEventListener("click", () => open(false));
  document.addEventListener("keydown", (event) => { if (event.key === "Escape") open(false); });

  document.querySelectorAll(".row").forEach((row) => {
    row.querySelector(".field .ghost").addEventListener("click", async () => take(await ask("browse", { key: row.dataset.key })));
  });

  async function install(cmd) {
    const buttons = [$("install"), $("file")];
    buttons.forEach((b) => b.classList.add("busy"));
    const answer = await ask(cmd);
    buttons.forEach((b) => b.classList.remove("busy"));
    if (answer.ok && answer.code !== "up_to_date" && !answer.cancelled) latest = "";
    take(answer);
    if (answer.cancelled) return;
    toast(!answer.ok ? failed(answer.code) : answer.code === "up_to_date" ? t("up_to_date") : t("done"));
  }
  $("install").addEventListener("click", () => install("update"));
  $("file").addEventListener("click", () => install("install_file"));

  $("start").addEventListener("click", async () => {
    if (!state.ready) { open(true); return; }
    const button = $("start");
    button.classList.add("busy");
    $("start-text").textContent = t("starting");
    const answer = await ask("start");
    if (!answer.ok) toast(failed(answer.code));
    // A moment longer after a good start, so that a second click does not start the game twice.
    setTimeout(() => {
      button.classList.remove("busy");
      render();
    }, answer.ok ? 6000 : 0);
  });

  take(await ask("state"));
  // Quietly: a newer release only changes the update button's words.
  ask("check_update").then((answer) => {
    if (answer.ok && answer.available && state.layer && state.layer.installed) {
      latest = answer.latest;
      render();
    }
  });
  if (location.hash === "#settings") {
    document.body.classList.add("instant");
    open(true);
  }
})();
