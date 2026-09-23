/* OptimizeKit v2.9 — Windows Gaming Control Center
   Shell: grouped rail + command bar · 34 modules · 12 themes · Ctrl+K palette
   Games: every store + every fixed drive, matched against the built-in game database;
   Library: cover art plus that database, installed titles badged with their real icon.
   Firmware panel (live SecureBoot/TPM/VT/kernel state) + driver auto-update engine.
   v2.9: dashboard Highlights, featured pack categories, firmware spotlight.
   Compiled C++20 binary (embedded HTTP server) + HTML/CSS/JS dashboard — WormGPT-desktop style. */
"use strict";
const $ = (s) => document.querySelector(s);
const $$ = (s) => Array.from(document.querySelectorAll(s));
const fmtB = (n) => {
  if (n == null) return "—";
  const u = ["B", "KB", "MB", "GB", "TB"]; let i = 0; let v = n;
  while (v >= 1024 && i < 4) { v /= 1024; i++; }
  return (i ? v.toFixed(1) : v) + " " + u[i];
};
const esc = (s) => String(s ?? "").replace(/[&<>\"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

let toastTimer = null;
function toast(msg, ms = 2600) {
  const t = $("#toast"); t.textContent = msg; t.classList.add("visible");
  clearTimeout(toastTimer); toastTimer = setTimeout(() => t.classList.remove("visible"), ms);
}
async function api(path, body) {
  const opt = body ? { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) } : undefined;
  const r = await fetch(path, opt);
  if (!r.ok && r.status !== 404) throw new Error(path + " -> " + r.status);
  return r.json();
}
function on(sel, fn) { const el = $(sel); if (el) el.addEventListener("click", fn); }

/* ===================== apply overlay ===================== */
const overlay = {
  show(title, sub) {
    $("#ao-title").textContent = title;
    $("#ao-sub").textContent = sub || "one moment";
    $("#ao-steps").innerHTML = "";
    $("#ao-bar").style.width = "8%";
    $("#apply-overlay").classList.remove("hidden");
    $("#apply-overlay").querySelector(".ao-card").classList.remove("ao-done");
  },
  step(text, cls) {
    const box = $("#ao-steps");
    const d = document.createElement("div");
    if (cls) d.className = cls;
    d.textContent = text;
    box.appendChild(d); box.scrollTop = box.scrollHeight;
  },
  progress(p) { $("#ao-bar").style.width = Math.round(p) + "%"; },
  async done(ok, msg) {
    const card = $("#apply-overlay").querySelector(".ao-card");
    card.classList.add("ao-done");
    $("#ao-title").textContent = ok ? "✔ Done" : "✖ Failed";
    $("#ao-sub").textContent = msg || (ok ? "everything applied — fully reversible" : "check the Logs page for details");
    $("#ao-bar").style.width = "100%";
    await sleep(ok ? 850 : 1600);
    $("#apply-overlay").classList.add("hidden");
  },
};

/* ===================== accent helpers (themes that actually work) ===================== */
function accentRGB() {
  const v = getComputedStyle(document.documentElement).getPropertyValue("--accent-rgb").trim();
  return v || "255,61,87";
}
const monitorColors = () => ({
  cpu: `rgba(${accentRGB()},1)`,
  ram: "rgba(245,165,36,1)",
  gpu: "rgba(61,214,140,1)",
  disk: `rgba(${accentRGB()},.8)`,
});

/* ===================== particles (WormGPT) ===================== */
(function particles() {
  const cvs = $("#dg-particles"); if (!cvs) return;
  const ctx = cvs.getContext("2d");
  let W, H, dots = [];
  const mouse = { x: -9999, y: -9999 };
  function size() { W = cvs.width = innerWidth; H = cvs.height = innerHeight; }
  size(); addEventListener("resize", size);
  addEventListener("mousemove", (e) => { mouse.x = e.clientX; mouse.y = e.clientY; });
  const COUNT = Math.min(90, Math.floor(innerWidth / 18));
  for (let i = 0; i < COUNT; i++) dots.push({
    x: Math.random() * innerWidth, y: Math.random() * innerHeight,
    vx: (Math.random() - .5) * .28, vy: (Math.random() - .5) * .28,
    r: Math.random() * 1.6 + .4, a: Math.random() * .5 + .15,
  });
  (function tick() {
    ctx.clearRect(0, 0, W, H);
    const accent = getComputedStyle(document.documentElement).getPropertyValue("--accent-rgb") || "255,61,87";
    for (const d of dots) {
      d.x += d.vx; d.y += d.vy;
      const dx = d.x - mouse.x, dy = d.y - mouse.y, dist2 = dx * dx + dy * dy;
      if (dist2 < 120 * 120) { const f = (120 - Math.sqrt(dist2)) / 120 * .05; d.vx += dx * f * .01; d.vy += dy * f * .01; }
      d.vx = Math.max(-.6, Math.min(.6, d.vx)); d.vy = Math.max(-.6, Math.min(.6, d.vy));
      if (d.x < 0 || d.x > W) d.vx *= -1;
      if (d.y < 0 || d.y > H) d.vy *= -1;
      ctx.beginPath(); ctx.arc(d.x, d.y, d.r, 0, 7);
      ctx.fillStyle = `rgba(${accent.trim()},${d.a})`; ctx.fill();
    }
    requestAnimationFrame(tick);
  })();
})();

/* ===================== dashboard spotlights (v2.9 Highlights) =====================
   The dashboard gets a live, clickable row of the newest modules: firmware state,
   driver ages and the game count update as soon as the machine answers. Nothing
   is invented — a card shows "—" until its real data is on screen. */
let spotFirmware = null, spotDriverAge = null, spotGameCount = null;
const spotCards = [
  { key: "firmware", cls: "spot-firmware", ico: "⛭", label: "Firmware & platform", view: "bios",
    sub: () => spotFirmware ? esc((spotFirmware.biosVendor || "?") + " " + (spotFirmware.biosVersion || "")) : "reading…" },
  { key: "secureboot", cls: "spot-secureboot", ico: "🛡", label: "Secure Boot", view: "bios",
    sub: () => spotFirmware ? esc(spotFirmware.secureBoot || "?") : "reading…" },
  { key: "drivers", cls: "spot-drivers", ico: "⇥", label: "Driver ages", view: "drivers",
    sub: () => spotDriverAge ? esc(spotDriverAge) : "measuring…" },
  { key: "games", cls: "spot-games", ico: "☰", label: "Games detected", view: "games",
    sub: () => spotGameCount != null ? spotGameCount + " on this machine" : "detecting…" },
  { key: "reducer", cls: "spot-reducer", ico: "⏬", label: "Process Reducer", view: "reducer", sub: () => "EcoQoS sweep & undo" },
  { key: "security", cls: "spot-security", ico: "◎", label: "Security Scan", view: "security", sub: () => "read-only audit" },
  { key: "diskscope", cls: "spot-diskscope", ico: "▤", label: "DiskScope", view: "storage", sub: () => "dupes · folders · cleanup" },
  { key: "bench", cls: "spot-bench", ico: "▲", label: "Benchmark", view: "bench", sub: () => "1000 = reference machine" },
];
function renderSpotlights() {
  const grid = $("#dash-spotlights"); if (!grid) return;
  if (!grid.childElementCount) {
    grid.innerHTML = spotCards.map((c) =>
      `<div class="spot-card ${c.cls}" data-view="${c.view}" style="animation-delay:${Math.min(grid.childElementCount * 40, 300)}ms">
        <span class="sc-ico">${c.ico}</span>
        <div class="sc-txt"><b>${esc(c.label)}</b><span class="sub muted">…</span></div>
        <span class="sc-go">→</span>
      </div>`).join("");
    $$("#dash-spotlights .spot-card").forEach((card) => card.addEventListener("click", () => show(card.dataset.view)));
  }
  $$("#dash-spotlights .spot-card").forEach((card) => {
    const c = spotCards.find((x) => x.key === card.dataset.view || card.classList.contains(x.cls));
    if (!c) return;
    const sub = card.querySelector(".sc-txt .sub");
    if (sub) sub.innerHTML = c.sub();
    if (c.key === "secureboot" && spotFirmware)
      card.classList.toggle("spot-warn", spotFirmware.secureBoot !== "on");
  });
}

/* ===================== i18n (EN base, FR) ===================== */
const I18N = {
  en: { themes:"Themes", dashboard:"Dashboard", smart:"Smart Optimize", gaming:"Gaming Center", scan:"Scan PC", optimize:"Optimize", tweaks:"Tweaks", games:"Games",
        library:"Game Library", packs:"Packs", inputlag:"Input Lag", render:"Rendering & FPS", background:"Background load", power:"Power & thermals", debloat:"Debloat & boot",
        network:"Network", ram:"RAM", storage:"Storage", startup:"Startup", drivers:"Drivers", bios:"BIOS guide", privacy:"Privacy",
        diag:"Diagnostics", bench:"Benchmark", tools:"Tools", logs:"Logs", settings:"Settings", about:"About" },
  fr: { themes:"Thèmes", dashboard:"Tableau de bord", smart:"Optimisation intelligente", gaming:"Centre Gaming", scan:"Analyser le PC", optimize:"Optimiser", tweaks:"Tweaks", games:"Jeux",
        library:"Bibliothèque de jeux", packs:"Packs", inputlag:"Latence d'entrée", render:"Rendu & FPS", background:"Charge de fond", power:"Énergie & thermique", debloat:"Débloat & démarrage",
        network:"Réseau", ram:"RAM", storage:"Stockage", startup:"Démarrage", drivers:"Pilotes", bios:"Guide BIOS", privacy:"Confidentialité",
        diag:"Diagnostics", bench:"Benchmark", tools:"Outils", logs:"Journaux", settings:"Paramètres", about:"À propos" }
};
function applyLang(lang) {
  const t = I18N[lang] || I18N.en;
  document.querySelectorAll(".nav-item[data-view]").forEach((n) => {
    const v = n.dataset.view, s = n.querySelector("span:nth-child(2)");
    if (s && t[v]) s.textContent = t[v];
  });
  document.documentElement.lang = lang;
}

/* ===================== theme engine =====================
   Three layers, in this order: the pack (body[data-theme]) sets every surface,
   the inline accent on <html> refines the accent, localStorage makes it instant
   on the next open, and config.json is the source of truth across machines.
   currentTheme/currentAccent exist so the 10 s state poll can never silently
   revert a choice the user just made. */
let currentTheme = null, currentAccent = null;

/* Accent memory: each theme keeps ITS OWN accent. That is the fix for the old
   behaviour where a colour picked on one theme leaked onto another (the config
   ended up as e.g. ui_theme=steel with ui_accent=#38bdf8 — silver surfaces with
   a blue accent). A theme now always looks the way it was designed, unless you
   deliberately refined that theme's accent yourself. */
function accentMem() {
  try { return JSON.parse(localStorage.getItem("ok_accents") || "{}"); } catch (e) { return {}; }
}
function rememberAccent(theme, hex) {
  if (!theme || !hex) return;
  const m = accentMem(); m[theme] = hex.toLowerCase();
  localStorage.setItem("ok_accents", JSON.stringify(m));
}
function themeAccent(theme) {
  const pack = THEME_PACKS.find(([id]) => id === theme) || THEME_PACKS[0];
  return accentMem()[pack[0]] || pack[1];
}

function shade(hex, amt) {   // lighten (amt>0) or darken (amt<0) a #rrggbb colour
  const r = parseInt(hex.slice(1, 3), 16), g = parseInt(hex.slice(3, 5), 16), b = parseInt(hex.slice(5, 7), 16);
  const f = (v) => Math.max(0, Math.min(255, Math.round(amt > 0 ? v + (255 - v) * amt : v * (1 + amt))));
  return "#" + [f(r), f(g), f(b)].map((v) => v.toString(16).padStart(2, "0")).join("");
}
function applyAccent(hex, persist = true) {
  if (!/^#[0-9a-fA-F]{6}$/.test(hex)) return;
  currentAccent = hex.toLowerCase();
  const r = parseInt(hex.slice(1, 3), 16), g = parseInt(hex.slice(3, 5), 16), b = parseInt(hex.slice(5, 7), 16);
  const root = document.documentElement.style;
  root.setProperty("--accent", hex);
  root.setProperty("--accent-hover", shade(hex, .14));
  root.setProperty("--accent-rgb", `${r},${g},${b}`);
  $$(".theme-opt").forEach((o) => o.classList.toggle("on", (o.dataset.accent || "").toLowerCase() === currentAccent));
  if (persist) localStorage.setItem("ok_accent", currentAccent);
  if (currentView === "dashboard") { const mc = monitorColors(); sparkline($("#m-cpu-g"), histCpu, mc.cpu); sparkline($("#m-gpu-g"), histGpu, mc.gpu); sparkline($("#m-disk-g"), histDisk, mc.disk); }
  if (currentView === "gaming") { const ring = $(".gc-ring"); if (ring) ring.style.background = `conic-gradient(${hex} calc(var(--p,0)*1%),rgba(255,255,255,.07) 0)`; }
}
async function setTheme(hex, persist) {
  applyAccent(hex, persist !== false);
  if (persist) { await api("/api/settings", { ui_accent: hex }).catch(() => {}); toast("Accent updated"); }
}

/* ===================== theme packs (full-surface) ===================== */
const THEME_PACKS = [
  ["magma",   "#ff3d57", "Magma — deep black + crimson"],
  ["khadafi", "#ff3b4e", "Khadafi — near-black #0a0a0c + signal red"],
  ["carbon",  "#ffb04d", "Carbon — graphite + amber"],
  ["discord", "#5865f2", "Discord — blurple on nocturne"],
  ["fusion",  "#ff37c7", "Fusion — hot magenta"],
  ["acid",    "#7cff3d", "Acid — lime terminal"],
  ["ocean",   "#38bdf8", "Ocean — abyssal blue"],
  ["matrix",  "#22c55e", "Matrix — terminal green"],
  ["violet",  "#a78bfa", "Violet — neon purple"],
  ["gold",    "#f59e0b", "Gold — amber elite"],
  ["steel",   "#94a3b8", "Steel — cold silver"],
  ["rose",    "#f472b6", "Rose — soft pink"],
];
function applyThemePack(name, persist = true) {
  const pack = THEME_PACKS.find(([id]) => id === name) || THEME_PACKS[0];
  currentTheme = pack[0];
  document.body.dataset.theme = pack[0];
  applyAccent(themeAccent(pack[0]), persist);
  if (persist) localStorage.setItem("ok_theme", pack[0]);
  $$(".pack-opt").forEach((o) => o.classList.toggle("on", o.dataset.pack === pack[0]));
  $$(".theme-card").forEach((c) => c.classList.toggle("on", c.dataset.pack === pack[0]));
  const nm = $("#theme-name"); if (nm) nm.textContent = pack[2];
}
async function setThemePack(name, persist = true) {
  applyThemePack(name, persist);
  if (persist) {
    const pack = THEME_PACKS.find(([id]) => id === name) || THEME_PACKS[0];
    await api("/api/settings", { ui_theme: pack[0], ui_accent: pack[1] }).catch(() => {});
    toast("Theme: " + pack[0]);
  }
}
on("#btn-theme", () => {
  const fly = $("#theme-flyout");
  fly.classList.toggle("hidden");
  if (!fly.classList.contains("hidden")) fly.classList.add("fly-in");   // animation hook
});
document.addEventListener("click", (e) => {
  const fly = $("#theme-flyout");
  if (fly && !fly.classList.contains("hidden") && !e.target.closest("#theme-flyout") && !e.target.closest("#btn-theme")) fly.classList.add("hidden");
});
$$("#theme-flyout .theme-opt").forEach((o) => o.addEventListener("click", () => {
  applyAccent(o.dataset.accent);                       // keeps the current pack, only refines its accent
  rememberAccent(currentTheme, o.dataset.accent);
  o.classList.add("picked");                           // pick feedback before the flyout closes
  setTimeout(() => o.classList.remove("picked"), 450);
  api("/api/settings", { ui_accent: o.dataset.accent }).catch(() => {});
  toast("Accent " + o.dataset.accent + " for " + currentTheme);
  setTimeout(() => $("#theme-flyout").classList.add("hidden"), 280);
}));
$$(".pack-opt").forEach((o) => o.addEventListener("click", () => setThemePack(o.dataset.pack, true)));

/* ===================== navigation ===================== */
let currentView = "dashboard";
function show(view) {
  currentView = view;
  $$(".view").forEach((v) => { v.classList.add("hidden"); v.classList.remove("view-anim"); });
  const el = $("#view-" + view); if (el) { el.classList.remove("hidden"); void el.offsetWidth; el.classList.add("view-anim"); }
  $$(".nav-item").forEach((n) => n.classList.toggle("active", n.dataset.view === view));
  const crumb = $("#crumb");
  if (crumb) { const t = $(`.nav-item[data-view="${view}"] span:nth-child(2)`); crumb.textContent = t ? t.textContent : view; }
  if (view === "network") refreshNet();
  if (view === "startup") refreshStartup();
  if (view === "storage") refreshStorage();
  if (view === "bench") refreshBenchHistory();
  if (view === "logs") refreshLogs();
  if (view === "ram") refreshRam();
  if (view === "games") loadGames();
  if (view === "settings") loadSettings();
  if (view === "gaming") refreshGamingCenter();
  if (view === "tweaks") updateTweakState();
  if (view === "diag") refreshDiag();
  if (view === "smart") refreshSmart();
  if (view === "packs") renderPacks();
  if (view === "reducer") refreshReducer();
  if (view === "security") refreshSecurity(false);
  if (view === "bios") { renderBios(); refreshFirmware(); }
  if (view === "drivers") refreshDriverReport(false);
  if (view === "dashboard") renderSpotlights();
  if (view === "library") renderLibrary();
  if (view === "themes") renderThemes();
  if (view === "dashboard") refreshKpis(null);
  if (CENTERS[view]) renderCenter(view);
}
$("#nav").addEventListener("click", (e) => {
  const item = e.target.closest(".nav-item");
  if (item) show(item.dataset.view);
});

/* ===================== monitor (dashboard) ===================== */
function sparkline(canvas, values, color, fill = true) {
  if (!canvas) return;
  const dpr = window.devicePixelRatio || 1;
  const w = canvas.clientWidth, h = canvas.clientHeight;
  if (!w || !h) return;
  canvas.width = w * dpr; canvas.height = h * dpr;
  const ctx = canvas.getContext("2d");
  ctx.scale(dpr, dpr);
  ctx.clearRect(0, 0, w, h);
  if (!values.length) return;
  const max = 100;
  const step = w / Math.max(values.length - 1, 1);
  ctx.beginPath();
  values.forEach((v, i) => { const x = i * step, y = h - (Math.min(v, max) / max) * (h - 4) - 2; i ? ctx.lineTo(x, y) : ctx.moveTo(x, y); });
  ctx.strokeStyle = color; ctx.lineWidth = 1.6; ctx.lineJoin = "round";
  ctx.shadowColor = color; ctx.shadowBlur = 6;
  ctx.stroke();
  ctx.shadowBlur = 0;
  if (fill) {
    ctx.lineTo(w, h); ctx.lineTo(0, h); ctx.closePath();
    const g = ctx.createLinearGradient(0, 0, 0, h);
    g.addColorStop(0, color.replace("1)", ".22)")); g.addColorStop(1, color.replace("1)", "0)"));
    ctx.fillStyle = g; ctx.fill();
  }
}
const histCpu = [], histRam = [], histGpu = [], histDisk = [];

async function pollMonitor() {
  try {
    const s = await api("/api/monitor");
    $("#m-cpu-v").textContent = s.cpu.toFixed(0) + "%";
    $("#m-ram-v").textContent = s.ram.toFixed(0) + "%";
    $("#m-gpu-v").textContent = s.gpu.toFixed(0) + "%";
    $("#m-disk-v").textContent = s.disk.toFixed(0) + "%";
    $("#m-cpu-clock").textContent = s.cpuClock ? (s.cpuClock.toFixed(0) + "% of base clock") : "";
    $("#m-ram-abs").textContent = fmtB(s.ramUsed) + " / " + fmtB(s.ramTotal);
    $("#m-disk-io").textContent = (s.diskReadMBs || 0).toFixed(1) + " ↓ " + (s.diskWriteMBs || 0).toFixed(1) + " ↑ MB/s";
    $("#m-net-v").textContent = "↓ " + fmtB((s.netDownKBs || 0) * 1024) + "/s  ↑ " + fmtB((s.netUpKBs || 0) * 1024) + "/s";
    $("#m-proc-v").textContent = s.procs + " procs · " + s.threads + " threads";
    $("#foot-procs").textContent = s.procs;
    $("#st-uptime").textContent = fmtUptime(s.uptimeSec);
    histCpu.push(s.cpu); histRam.push(s.ram); histGpu.push(s.gpu); histDisk.push(s.disk);
    for (const h of [histCpu, histRam, histGpu, histDisk]) if (h.length > 90) h.shift();
    if (currentView === "dashboard") {
      const mc = monitorColors();
      sparkline($("#m-cpu-g"), histCpu, mc.cpu);
      sparkline($("#m-ram-g"), histRam, mc.ram);
      sparkline($("#m-gpu-g"), histGpu, mc.gpu);
      sparkline($("#m-disk-g"), histDisk, mc.disk);
      pollProcesses();
      refreshKpis(s);
    }
    if (currentView === "ram") { const mc = monitorColors(); drawRamBig(s, mc.cpu); }
  } catch (e) { /* server restarting */ }
}
function fmtUptime(sec) {
  const d = Math.floor(sec / 86400), h = Math.floor((sec % 86400) / 3600), m = Math.floor((sec % 3600) / 60);
  return (d ? d + "d " : "") + h + "h " + m + "m";
}
let procTimer = 0;
async function pollProcesses() {
  const now = Date.now();
  if (now - procTimer < 2400) return;
  procTimer = now;
  try {
    const list = await api("/api/processes");
    $("#proc-list").innerHTML = list.map((p) =>
      `<div class="proc-row"><span class="p-name">${esc(p.name)}</span><span class="p-host mono">${p.pid}</span><span class="p-ms mono">${(p.cpu || 0).toFixed(1)}%</span><span class="p-ms mono muted">${fmtB((p.ramMB || 0) * 1048576)}</span></div>`
    ).join("");
  } catch (e) { }
}

async function loadState() {
  try {
    const s = await api("/api/state");
    $("#st-os").textContent = s.os;
    $("#st-cpu").textContent = `${s.cpu} (${s.threads} threads)`;
    $("#st-gpu").textContent = s.gpu + (s.gpuDriver ? ` · driver ${s.gpuDriver}` : "");
    $("#st-ram").textContent = fmtB(s.ramTotal - s.ramAvail) + " / " + fmtB(s.ramTotal);
    $("#st-plan").textContent = s.plan;
    $("#st-gm").textContent = s.gameMode ? "Enabled" : "Disabled";
    $("#st-hags").textContent = s.hags ? "Enabled" : "Disabled";
    // only react to a real change: re-applying on every poll used to wipe a custom accent
    if (s.ui_theme && s.ui_theme !== currentTheme) applyThemePack(s.ui_theme, false);
    const adm = s.admin;
    for (const id of ["admin-pill", "admin-pill-side"])
      $("#" + id).textContent = adm ? "Administrator" : "Standard user";
    return s;
  } catch (e) { }
}

/* ===================== tweaks (v2: switches) ===================== */
let tweaksCache = [];
async function updateTweakState(silent) {
  try {
    tweaksCache = await api("/api/tweaks");
    if (currentView === "tweaks") renderTweaks($(".chip.on")?.dataset.filter || "all");
    if (currentView === "gaming") refreshGamingCenter();
    if (CENTERS[currentView]) renderCenter(currentView);
    const applied = tweaksCache.filter((t) => t.applied).length;
    const badge = $("#nav-tw-count");
    badge.textContent = applied;
    badge.classList.toggle("show", applied > 0);
  } catch (e) { if (!silent) toast("✖ could not read tweak state"); }
}
async function loadTweaks() { await updateTweakState(true); }
function catOf(t) {
  const n = (t.name + t.desc).toLowerCase();
  if (/(telemetry|privacy|advertis|bing|copilot|activity|tailored)/.test(n)) return "privacy";
  if (/(onedrive|bloat|store|xbox live|edge)/.test(n)) return "debloat";
  return "gaming";
}
let twSearch = "";
function renderTweaks(filter) {
  const el = $("#tweaks-list");
  const q = twSearch.toLowerCase();
  const list = tweaksCache
    .filter((t) => filter === "all" || catOf(t) === filter)
    .filter((t) => !q || (t.name + t.desc + t.id).toLowerCase().includes(q));
  el.innerHTML = list.map((t, i) => twRowHTML(t, i)).join("") || `<div class="conv-empty muted" style="padding:14px">no tweak matches</div>`;
  wireSwitches("#tweaks-list");
}
async function toggleTweak(sw) {
  const id = sw.dataset.id;
  const t = tweaksCache.find((x) => x.id === id); if (!t || sw.classList.contains("busy")) return;
  const willApply = !t.applied;
  sw.classList.add("busy");
  overlay.show(willApply ? "Applying · " + t.name : "Restoring · " + t.name,
    willApply ? "backing up the registry key, then applying" : "putting back the Windows default");
  overlay.step((willApply ? "apply " : "restore ") + id);
  overlay.progress(35);
  try {
    const r = await api(willApply ? "/api/tweaks/apply" : "/api/tweaks/restore", [id]);
    overlay.progress(90);
    const okb = willApply ? r.applied > 0 : r.restored > 0;
    const err = (r.errors || [])[0];
    if (err) overlay.step("✖ " + err, "er");
    else overlay.step(okb ? "✓ done" : "no change needed", okb ? "ok" : "");
    await overlay.done(okb, err ? err : undefined);
    if (!okb && err) toast("✖ " + err, 4200);
  } catch (e) { await overlay.done(false, String(e)); }
  sw.classList.remove("busy");
  await updateTweakState(true);
}
function updateSel() { /* bulk buttons retired — switches apply instantly */ }
["#btn-sel-all", "#btn-sel-none", "#btn-restore-sel", "#btn-apply-sel"].forEach((s) => { const el = $(s); if (el) el.style.display = "none"; });
$("#tw-search").addEventListener("input", (e) => { twSearch = e.target.value; renderTweaks($(".chip.on")?.dataset.filter || "all"); });
$$(".chips .chip").forEach((c) => c.addEventListener("click", () => {
  $$(".chips .chip").forEach((x) => x.classList.remove("on")); c.classList.add("on");
  renderTweaks(c.dataset.filter);
}));

/* ===================== quick profiles ===================== */
on("#btn-privacy", () => applyProfile("privacy"));
on("#btn-full-gaming", () => applyProfile("gaming"));
on("#btn-quick-optimize", () => applyProfile("gaming"));
on("#btn-quick-esport", () => show("gaming"));
on("#btn-quick-firmware", () => show("bios"));
on("#btn-clean", () => doClean());
on("#btn-clean2", () => doClean());
async function applyProfile(name) {
  overlay.show("Applying " + name.toUpperCase() + " profile", "snapshot → apply → verify");
  overlay.step("creating registry snapshot"); overlay.progress(15);
  await sleep(250);
  overlay.step("running profile: " + name); overlay.progress(45);
  try {
    const r = await api("/api/profile", { name });
    overlay.progress(95);
    overlay.step("✓ " + r.summary, "ok");
    await overlay.done(true, r.summary);
    toast("✔ " + r.summary, 4200);
  } catch (e) { await overlay.done(false, String(e)); }
  updateTweakState();
}
async function doClean() {
  overlay.show("Cleaning junk", "temporary files, caches, recycle bin");
  overlay.step("measuring junk…"); overlay.progress(30);
  try {
    const r = await api("/api/clean");
    overlay.progress(100);
    overlay.step("✓ freed " + fmtB(r.freedBytes), "ok");
    await overlay.done(true, fmtB(r.freedBytes) + " freed");
    toast(`🧹 Freed ${fmtB(r.freedBytes)}`, 4200);
  } catch (e) { await overlay.done(false, String(e)); }
}

/* ===================== scanner ===================== */
on("#btn-scan-run", runScan);
on("#btn-quick-scan", () => { show("scan"); runScan(); });
async function runScan() {
  $("#scan-progress").classList.remove("hidden");
  $("#scan-findings").innerHTML = "";
  $("#scan-summary").classList.add("hidden");
  try {
    const r = await api("/api/scan");
    $("#scan-progress").classList.add("hidden");
    $("#scan-summary").classList.remove("hidden");
    $("#scan-summary").innerHTML = `
      <div class="stat-card high"><b>${r.high}</b><span>high impact</span></div>
      <div class="stat-card med"><b>${r.medium}</b><span>medium</span></div>
      <div class="stat-card good"><b>${r.findings.length}</b><span>total findings</span></div>`;
    $("#scan-findings").innerHTML = r.findings.map((f) => `
      <div class="finding">
        <span class="sev ${esc(f.severity)}"></span>
        <div class="fbody"><b>${esc(f.title)}</b><p>${esc(f.detail)}</p></div>
        <div class="fx">
          <span class="impact">${esc(f.impact || "")}</span>
          ${f.action && f.action.id ? `<button class="btn sm primary" data-at="${esc(f.action.type)}" data-aid="${esc(f.action.id)}">Apply</button>` : ""}
        </div>
      </div>`).join("");
    $$("#scan-findings .btn").forEach((b) => b.addEventListener("click", async () => {
      const at = b.dataset.at, aid = b.dataset.aid;
      b.disabled = true; b.textContent = "…";
      try {
        if (at === "tweak") { await api("/api/tweaks/apply", [aid]); toast("✔ " + aid + " applied"); }
        else if (at === "clean") { const r = await api("/api/clean"); toast("🧹 Freed " + fmtB(r.freedBytes)); }
        else if (at === "net") { await api("/api/net/profile", { profile: aid }); toast("✔ network: " + aid); }
        updateTweakState();
      } catch (e) { toast("✖ failed — see logs"); }
      b.disabled = false; b.textContent = "Apply";
    }));
    toast("Scan complete: " + r.summary);
  } catch (e) {
    $("#scan-progress").classList.add("hidden");
    toast("✖ Scan failed");
  }
}

/* ===================== optimize page (streaming log) ===================== */
on("#btn-opt-safe", () => runOptProfile("gaming"));
on("#btn-opt-gaming", () => runOptProfile("gaming"));
on("#btn-opt-privacy", () => runOptProfile("privacy"));
on("#btn-opt-restore", async () => {
  overlay.show("Restoring all Windows defaults", "every tweak goes back to Microsoft defaults");
  try {
    const r = await api("/api/profile", { name: "restore" });
    await overlay.done(true, r.summary || "done");
    toast("↺ " + (r.summary || "done"), 4000);
  } catch (e) { await overlay.done(false, String(e)); }
  updateTweakState();
});
async function runOptProfile(name) {
  const box = $("#opt-log"); box.innerHTML = "";
  overlay.show("Running " + name + " profile", "watch the live log below when it finishes");
  try {
    const r = await api("/api/profile", { name });
    const rep = await api("/api/logs2");
    box.innerHTML = rep.items.map((i) => `<div class="log-line ${i.sev.toLowerCase()}">[${i.time}] ${i.sev} [${esc(i.cat)}] ${esc(i.msg)}</div>`).join("");
    await overlay.done(true, r.summary);
    toast("✔ " + r.summary, 5000);
  } catch (e) { await overlay.done(false, String(e)); }
  updateTweakState();
}

/* ===================== GAMING CENTER ===================== */
const GC_CATS = [
  { id: "fps", label: "FPS & rendering", keys: ["hags_on", "game_mode", "fso_on", "mpo_off", "gpu_preference"] },
  { id: "latency", label: "Latency & input", keys: ["timer_high", "network_gaming", "mouse_precision", "menu_delay_0", "win32_priority"] },
  { id: "background", label: "Background load", keys: ["game_dvr_off", "background_apps", "sysmain_off", "search_index", "xbox_live_off"] },
  { id: "power", label: "Power & thermals", keys: ["power_ultimate", "hpets_off"] },
];
let gcCatFilter = null;
async function refreshGamingCenter() {
  if (!tweaksCache.length) await updateTweakState(true);
  try { if (!window.__netStatus) window.__netStatus = await api("/api/net/status"); } catch (e) { }
  const get = (id) => tweaksCache.find((t) => t.id === id);
  const on = (id) => !!get(id)?.applied;
  // ---- score: weighted mix of gaming-relevant tweaks + plan
  const weights = { hags_on: 12, game_mode: 8, game_dvr_off: 14, timer_high: 14, network_gaming: 14, mouse_precision: 10, fso_on: 6, mpo_off: 6, background_apps: 8, sysmain_off: 8, power_ultimate: 10, win32_priority: 8, gpu_preference: 5, menu_delay_0: 4, search_index: 5, xbox_live_off: 4, hpets_off: 6 };
  let score = 38;   // baseline: a healthy Windows install
  for (const t of tweaksCache) if (t.applied && weights[t.id]) score += weights[t.id];
  if ((window.__netStatus?.autotuning || "") !== "disabled") score += 6;   // sane autotuning = good
  score = Math.max(5, Math.min(100, Math.round(score)));
  const ring = $(".gc-ring");
  ring.style.setProperty("--p", score);
  $("#gc-num").textContent = score;
  const sum = $("#gc-summary");
  const appliedCount = tweaksCache.filter((t) => t.applied).length;
  sum.innerHTML = score >= 80 ? `<b style="color:var(--ok)">Competition ready.</b> ${appliedCount} optimizations active on this machine.`
    : score >= 55 ? `<b style="color:var(--warn)">Good base.</b> ${appliedCount} active — ${tweaksCache.length - appliedCount} reversible tweaks still available.`
    : `<b style="color:var(--err)">Stock Windows.</b> ${tweaksCache.length} curated optimizations available — all reversible.`;
  // ---- summary cards
  const pct = (arr) => Math.round(arr.filter(Boolean).length / Math.max(arr.length, 1) * 100);
  const cards = [
    { t: "Performance", v: pct(["hags_on", "gpu_preference", "power_ultimate", "win32_priority"].map(on)), n: on("power_ultimate") ? "ultimate plan active" : "stock power plan" },
    { t: "Latency", v: pct(["timer_high", "network_gaming", "mouse_precision", "menu_delay_0"].map(on)), n: on("timer_high") ? "0.5 ms timer" : "default timer" },
    { t: "Background", v: 100 - pct(["game_dvr_off", "background_apps", "sysmain_off", "search_index", "xbox_live_off"].map(on)), n: on("game_dvr_off") ? "DVR off" : "Game DVR recording" },
    { t: "Rendering", v: pct(["hags_on", "fso_on", "mpo_off"].map(on)), n: on("hags_on") ? "HAGS on" : "HAGS off" },
    { t: "Storage", v: 100 - Math.min(95, Math.round(((window.__lastJunk || 0) / (50 * 1024 * 1024)) * 10)), n: window.__lastJunk ? fmtB(window.__lastJunk) + " junk" : "no scan yet" },
  ];
  $("#gc-cards").innerHTML = cards.map((c, i) => `
    <div class="gc-card ${c.v >= 70 ? "good" : c.v >= 40 ? "warn" : "bad"}" style="animation-delay:${i * 60}ms">
      <small>${c.t}</small><b>${c.v}%</b><span class="gc-note">${esc(c.n)}</span>
    </div>`).join("");
  // ---- categories with switches
  const wrap = $("#gc-switches");
  wrap.innerHTML = GC_CATS.map((cat, ci) => {
    const items = cat.keys.map(get).filter(Boolean);
    const onN = items.filter((t) => t.applied).length;
    const status = onN === items.length ? ["good", "OPTIMIZED"] : onN ? ["warn", onN + "/" + items.length] : ["bad", "STOCK"];
    return `<div class="gc-cat ${status[0]}" style="animation-delay:${ci * 70}ms">
      <div class="gc-cat-head"><b>${cat.label}</b><span class="gc-tag ${status[0]}">${status[1]}</span></div>
      <p>${items.map((t) => `<div class="gaming-key" data-id="${esc(t.id)}"><span><b style="color:var(--text)">${esc(t.name)}</b><br/><small>${esc(t.desc)}</small></span><b class="${t.applied ? "ok-t" : "dim-t"}" style="flex:none">${t.applied ? "ON" : "OFF"}</b>
        <div class="ok-switch ${t.applied ? "on" : ""}" data-id="${esc(t.id)}"></div></div>`).join("")}</p>
    </div>`;
  }).join("");
  $$("#gc-switches .ok-switch").forEach((sw) => sw.addEventListener("click", async (e) => {
    e.stopPropagation();
    const before = sw.dataset.id;
    await toggleTweak(sw);
    window.__netStatus = null;
    refreshGamingCenter();
  }));
  updateGamingStatus();
}
on("#btn-gc-refresh", async () => { window.__netStatus = null; await updateTweakState(true); refreshGamingCenter(); toast("↻ re-analyzed"); });

/* ===================== games (every store + every drive) ===================== */
let gamesCache = [], gamesQuery = "", gamesLauncher = "all", gamesSel = "";
const FAMILY_PACK = {
  fps: "esport", br: "esport", fighting: "esport", sports: "esport", racing: "esport",
  rpg: "esport", openworld: "esport", moba: "lowlatency", mmo: "lowlatency",
  coop: "lowlatency", horror: "lowlatency", party: "cleanboot", sandbox: "cleanboot",
  survival: "cleanboot", strategy: "cleanboot", sim: "cleanboot", roguelike: "cleanboot",
};
const normKey = (s) => String(s || "").toLowerCase().replace(/[^a-z0-9]/g, "");
const initials = (s) => String(s || "?").replace(/[^A-Za-z0-9 ]/g, "").split(/\s+/).filter(Boolean)
  .slice(0, 2).map((w) => w[0].toUpperCase()).join("") || "?";

async function loadGames() {
  const grid = $("#games-grid");
  if (grid) grid.innerHTML = `<div class="conv-empty muted" style="padding:14px">scanning Steam · Epic · Riot · GOG · Battle.net · Ubisoft · EA · Xbox · itch.io · every fixed drive…</div>`;
  try {
    gamesCache = await api("/api/games");
    renderGames();
    // one batch pass fills in every icon we do not have yet (bounded server-side)
    if (gamesCache.some((g) => !g.icon)) {
      const r = await api("/api/games/icons", { all: true }).catch(() => null);
      if (r && r.extracted) { gamesCache = await api("/api/games"); renderGames(); }
    }
    toast("☰ " + gamesCache.length + " games detected", 3000);
  } catch (e) {
    if (grid) grid.innerHTML = `<div class="conv-empty muted" style="padding:14px">game detection failed</div>`;
  }
}
function renderGames() {
  const grid = $("#games-grid"); if (!grid) return;
  const launchers = {};
  for (const g of gamesCache) launchers[g.launcher] = (launchers[g.launcher] || 0) + 1;
  const chips = $("#games-launchers");
  if (chips) chips.innerHTML =
    `<span class="chip ${gamesLauncher === "all" ? "on" : ""}" data-l="all">All · ${gamesCache.length}</span>` +
    Object.keys(launchers).sort((a, b) => launchers[b] - launchers[a]).map((l) =>
      `<span class="chip ${gamesLauncher === l ? "on" : ""}" data-l="${esc(l)}">${esc(l)} · ${launchers[l]}</span>`).join("");
  const stats = $("#games-stats");
  if (stats) stats.innerHTML = [
    { t: "detected", v: gamesCache.length, n: "every store + every fixed drive" },
    { t: "with icon", v: gamesCache.filter((g) => g.icon).length, n: "extracted from the real .exe" },
    { t: "genres known", v: new Set(gamesCache.map((g) => g.family).filter(Boolean)).size, n: "matched in the game database" },
    { t: "running now", v: gamesCache.filter((g) => g.running).length, n: "matched against the live process list" },
  ].map((c) => `<div class="stat-card"><small>${c.t}</small><b>${c.v}</b><span>${esc(c.n)}</span></div>`).join("");
  const q = gamesQuery.toLowerCase();
  const list = gamesCache.filter((g) => (gamesLauncher === "all" || g.launcher === gamesLauncher) &&
    (!q || (g.name + " " + g.launcher + " " + (g.family || "") + " " + (g.matched || "")).toLowerCase().includes(q)));
  if (!list.length) {
    grid.innerHTML = `<div class="conv-empty muted" style="padding:14px">${gamesCache.length
      ? "no game matches this filter"
      : "No games found — install one via Steam/Epic/Xbox/Battle.net/GOG, or drop it in a games folder on any drive."}</div>`;
    return;
  }
  grid.innerHTML = list.map((g) => `
    <div class="game-card ${gamesSel === g.id ? "sel" : ""}" data-id="${esc(g.id)}">
      ${g.icon ? `<img src="/api/game-icon/${encodeURIComponent(g.icon)}" alt=""/>` : `<span class="gph">${esc((g.launcher || "?")[0])}</span>`}
      <div class="g-meta"><b>${esc(g.name)}</b><span>${esc(g.launcher)}${g.family ? " · " + esc(g.family) : ""}</span></div>
      ${g.running ? `<span class="g-run" title="running"></span>` : ""}
    </div>`).join("");
  $$("#games-grid .game-card").forEach((c) => c.addEventListener("click", () => selectGame(c.dataset.id)));
}
$("#games-search").addEventListener("input", (e) => { gamesQuery = e.target.value; renderGames(); });
$("#games-launchers").addEventListener("click", (e) => {
  const chip = e.target.closest(".chip"); if (!chip) return;
  gamesLauncher = chip.dataset.l; renderGames();
});
on("#btn-games-refresh", loadGames);
on("#btn-games-icons", async () => {
  toast("🖼 extracting icons for every detected game…", 4000);
  const r = await api("/api/games/icons", { all: true });
  gamesCache = await api("/api/games"); renderGames();
  toast("🖼 " + (r.extracted || 0) + " new icons — " + gamesCache.filter((g) => g.icon).length + "/" + gamesCache.length + " games have one", 3800);
});
on("#btn-games-boost", async () => {
  if (!window.confirm("Give every detected game a persistent High priority profile (via IFEO)? You can undo it with the same button's Clear pass.")) return;
  const r = await api("/api/games/boost-all", { on: true, limit: 40 });
  toast("⚡ " + (r.applied || 0) + " games boosted" + ((r.errors || []).length ? " — " + r.errors.length + " need admin" : ""), 4000);
});
on("#btn-games-gmode", async () => {
  const r = await api("/api/gaming/enter", {});
  toast(r.ok ? "▶ gaming mode active — power plan & priorities will be restored" : "✖ " + r.error);
  updateGamingStatus();
});
on("#btn-games-random", () => {
  if (!gamesCache.length) return toast("☰ detect games first");
  selectGame(gamesCache[Math.floor(Math.random() * gamesCache.length)].id);
});
function libFindByGame(g) {
  if (!libGames || !libGames.length) return null;
  const keys = [normKey(g.name), normKey(g.matched)].filter((k) => k.length > 3);
  if (!keys.length) return null;
  return libGames.find((x) => keys.includes(normKey(x.name))) ||
    libGames.find((x) => { const k = normKey(x.name); return k.length > 5 && keys.some((y) => y.length > 5 && (k.includes(y) || y.includes(k))); }) || null;
}
function selectGame(id) {
  const g = gamesCache.find((x) => x.id === id); if (!g) return;
  gamesSel = id;
  $$("#games-grid .game-card").forEach((c) => c.classList.toggle("sel", c.dataset.id === id));
  const el = $("#game-profile");
  el.classList.remove("hidden");
  const lg = libFindByGame(g);
  el.innerHTML = `
    <div class="profile-hero">
      <h2>${esc(g.name)} <span style="color:var(--accent)">profile</span></h2>
      <p class="mono" style="font-size:11px;word-break:break-all">${esc(g.exe)}</p>
      <p class="muted" style="font-size:11.5px">${esc(g.launcher)}${g.family ? " · " + esc(g.family) : ""}${g.matched ? " · known title: " + esc(g.matched) : ""}${g.running ? " · <b style=\"color:var(--ok)\">running</b>" : ""}</p>
      <div class="g-actions">
        <button class="btn primary" id="gp-launch">▶ Launch</button>
        <button class="btn" id="gp-apply">⚡ Boost this game</button>
        <button class="btn" id="gp-gmode">▶ Start in Gaming Mode</button>
        <button class="btn" id="gp-icon">🖼 Extract icon</button>
        <button class="btn" id="gp-folder">📂 Open folder</button>
        <button class="btn" id="gp-copy">⧉ Copy path</button>
        ${lg ? `<button class="btn" id="gp-lib">▦ Library: ${esc(lg.name)}</button>` : ""}
        <button class="btn danger" id="gp-clear">↺ Clear boost</button>
      </div>
    </div>`;
  const note = (msg, ok) => toast((ok === false ? "✖ " : "✔ ") + msg, 3400);
  $("#gp-launch").addEventListener("click", async () => {
    const r = await api("/api/games/launch", { id: g.id });
    note(r.ok ? "launching " + g.name : r.error, r.ok);
  });
  $("#gp-apply").addEventListener("click", async () => {
    await api("/api/games/profile", { id: g.id, action: "save", profile: { priority: "high", disable_fso: true, game: g.name } });
    const r = await api("/api/games/profile", { id: g.id, action: "apply" });
    note(r.ok ? g.name + " boosted (persistent high priority)" : r.error, r.ok);
  });
  $("#gp-gmode").addEventListener("click", async () => {
    const r = await api("/api/gaming/enter", { game: g.id });
    note(r.ok ? "gaming mode for " + g.name : r.error, r.ok);
    updateGamingStatus();
  });
  $("#gp-icon").addEventListener("click", async () => {
    const r = await api("/api/games/icon", { id: g.id });
    gamesCache = await api("/api/games"); renderGames();
    note(r.ok ? "icon extracted" : "no icon available", r.ok);
  });
  $("#gp-folder").addEventListener("click", async () => {
    const r = await api("/api/games/launch", { id: g.id, reveal: true });
    note(r.ok ? "folder opened" : r.error, r.ok);
  });
  $("#gp-copy").addEventListener("click", async () => {
    try { await navigator.clipboard.writeText(g.exe); note("path copied"); } catch (e) { toast(g.exe, 5000); }
  });
  if (lg) $("#gp-lib").addEventListener("click", () => { show("library"); setTimeout(() => selectLibGame(lg.key), 180); });
  $("#gp-clear").addEventListener("click", async () => {
    await api("/api/games/profile", { id: g.id, action: "clear" });
    toast("↺ boost cleared");
  });
  if (!libGames) ensureLibrary().then(() => { if (gamesSel === id) selectGame(id); });
}

/* ===================== gaming mode ===================== */
on("#btn-gm-enter", async () => {
  const r = await api("/api/gaming/enter", {});
  toast(r.ok ? "▶ Gaming mode active — everything will be restored on exit" : "✖ " + r.error);
  updateGamingStatus();
});
on("#btn-gm-exit", async () => {
  await api("/api/gaming/exit", {});
  toast("■ Gaming mode ended — power plan & priorities restored");
  updateGamingStatus();
});
async function updateGamingStatus() {
  try {
    const st = await api("/api/state");
    const act = st.gamingMode;
    $("#btn-gm-enter").disabled = act;
    $("#btn-gm-exit").disabled = !act;
  } catch (e) { }
}

/* ===================== network center ===================== */
async function refreshNet() {
  try {
    const s = await api("/api/net/status");
    window.__netStatus = s;
    $("#net-cards").innerHTML = `
      <div class="net-card"><span>TCP autotuning</span><b>${esc(s.autotuning)}</b></div>
      <div class="net-card"><span>RSC</span><b>${s.rsc ? "enabled" : "disabled"}</b></div>
      <div class="net-card"><span>RSS</span><b>${s.rss ? "enabled" : "disabled"}</b></div>
      <div class="net-card"><span>DNS</span><b>${esc(s.dns || "default")}</b></div>
      <div class="net-card"><span>MTU</span><b>${s.mtu || "?"}</b></div>`;
    const profiles = [
      { id: "gaming", t: "Gaming", d: "RSC off · no NIC power saving · autotuning normal" },
      { id: "low_latency", t: "⏱ Low latency", d: "Same as gaming, DNS managed if allowed in Settings" },
      { id: "download", t: "⬇ Download", d: "Autotuning experimental (max throughput)" },
      { id: "balanced", t: "⚖ Balanced", d: "Windows defaults + flushed DNS" },
      { id: "restore", t: "↺ Restore defaults", d: "Autotuning normal · RSC on · NIC power saving on" },
    ];
    $("#net-profiles").innerHTML = profiles.map((p) => `
      <div class="tool-card" data-p="${p.id}"><b>${p.t}</b><span>${p.d}</span></div>`).join("");
    $$("#net-profiles .tool-card").forEach((c) => c.addEventListener("click", async () => {
      toast("Applying network profile: " + c.dataset.p + "…");
      const r = await api("/api/net/profile", { profile: c.dataset.p });
      toast(r.failed ? `✖ ${r.failed} failed — admin?` : `✔ ${r.applied} changes applied`);
      refreshNet();
    }));
    // ---- MTU slider card
    const mtu = s.mtu || 1500;
    const mtuCard = document.createElement("div");
    mtuCard.className = "mtu-card";
    mtuCard.innerHTML = `
      <div class="mtu-head"><b>MTU — maximum transmission unit</b><span class="mtu-val" id="mtu-val">${mtu}</span></div>
      <input type="range" class="ok-range" id="mtu-range" min="576" max="9000" step="1" value="${mtu}"/>
      <div class="mtu-presets">
        <span class="mtu-pre" data-v="1500">Ethernet 1500</span>
        <span class="mtu-pre" data-v="1492">PPPoE 1492</span>
        <span class="mtu-pre" data-v="1472">VPN 1472</span>
        <span class="mtu-pre" data-v="9000">Jumbo 9000</span>
      </div>
      <div class="mtu-note">Leave at 1500 on standard Ethernet. Values above 1500 (jumbo frames) only help when <b>every</b> device on the path supports them — otherwise they fragment and slow down.</div>
      <div style="margin-top:10px"><button class="btn sm primary" id="btn-mtu-apply">Apply MTU</button></div>`;
    const old = $("#mtu-card-slot"); if (old) old.remove();
    mtuCard.id = "mtu-card-slot";
    $(".net-cards").after(mtuCard);
    const range = $("#mtu-range");
    const paint = () => {
      const p = (range.value - range.min) / (range.max - range.min) * 100;
      range.style.setProperty("--fill", p + "%");
      $("#mtu-val").textContent = range.value;
    };
    paint();
    range.addEventListener("input", paint);
    $$(".mtu-pre").forEach((b) => b.addEventListener("click", () => { range.value = b.dataset.v; paint(); }));
    $("#btn-mtu-apply").addEventListener("click", async () => {
      overlay.show("Setting MTU", "netsh · persistent across reboots");
      overlay.step("set subinterface mtu=" + range.value);
      try {
        const r = await api("/api/net/mtu", { mtu: +range.value });
        overlay.step(r.ok ? "✓ applied" : "✖ " + (r.error || "failed"), r.ok ? "ok" : "er");
        await overlay.done(r.ok, r.ok ? "MTU = " + range.value : "admin required?");
      } catch (e) { await overlay.done(false, String(e)); }
      refreshNet();
    });
    refreshPing();
  } catch (e) { }
}
on("#btn-net-refresh", refreshNet);

async function refreshPing() {
  try {
    const list = await api("/api/ping");
    $("#ping-list").innerHTML = list.map((t) => `
      <div class="p-row"><span class="p-dot ${t.ok ? (t.ms < 40 ? "ok" : "warn") : "err"}"></span>
        <span class="p-name">${esc(t.name)}</span><span class="p-host mono">${esc(t.host)}</span>
        <span class="p-ms ${t.ok ? (t.ms < 40 ? "ok" : "warn") : "err"}">${t.ok ? t.ms + " ms" : "timeout"}</span></div>`).join("");
  } catch (e) { }
}
on("#btn-ping-refresh", refreshPing);
on("#btn-ping-go", async () => {
  const host = $("#ping-input").value.trim(); if (!host) return;
  $("#ping-quick-out").textContent = "pinging " + host + "…";
  const r = await api("/api/ping", { host });
  $("#ping-quick-out").textContent = r.ok ? `${host} → ${r.ms} ms` : `${host} → timeout`;
});

/* ===================== RAM ===================== */
async function refreshRam() {
  try {
    const m = await api("/api/ram");
    const used = m.total - m.avail;
    $("#ram-stats").innerHTML = `
      <div class="sys-row"><span>Used</span><b>${fmtB(used)}</b></div>
      <div class="sys-row"><span>Available</span><b>${fmtB(m.avail)}</b></div>
      <div class="sys-row"><span>Committed</span><b>${fmtB(m.committed)} / ${fmtB(m.commitLimit)}</b></div>
      <div class="sys-row"><span>Kernel cached</span><b>${fmtB(m.cached)}</b></div>`;
    const list = await api("/api/processes");
    const procs = list.slice().sort((a, b) => (b.ramMB || 0) - (a.ramMB || 0)).slice(0, 10);
    $("#ram-procs").innerHTML = procs.map((p) =>
      `<div class="proc-row"><span class="p-name">${esc(p.name)}</span><span class="p-host mono">${p.pid}</span><span class="p-ms mono muted">${fmtB((p.ramMB || 0) * 1048576)}</span></div>`).join("");
  } catch (e) { }
}
function drawRamBig(s, color) {
  $("#ram-big").textContent = s.ram.toFixed(0) + "%";
  $("#ram-abs2").textContent = fmtB(s.ramUsed) + " / " + fmtB(s.ramTotal);
  sparkline($("#ram-big-g"), histRam, color || `rgba(${accentRGB()},1)`);
}
on("#btn-ram-trim", async () => {
  const r = await api("/api/ram/trim");
  toast(r.ok ? "🧹 Standby list purged — cached memory released" : "✖ " + r.error, 4000);
  refreshRam();
});

/* ===================== PROCESS REDUCER (EcoQoS) ===================== */
let redCache = null;
async function refreshReducer() {
  try {
    redCache = await api("/api/reducer");
    renderReducer();
  } catch (e) { }
}
function renderReducer() {
  if (!redCache) return;
  const procs = redCache.procs || [];
  const reduced = redCache.reduced || 0;
  const ring = $("#view-reducer .gc-ring");
  ring.style.setProperty("--p", Math.min(100, reduced * 8));
  $("#red-num").textContent = reduced;
  const busy = procs.filter((p) => p.cpu >= 1).length;
  $("#red-summary").innerHTML = reduced
    ? `<b style="color:var(--ok)">${reduced} process(es) in Eco mode</b> — Undo all puts back their original priority.`
    : `${procs.length} candidate processes · <b>${busy}</b> above 1% CPU right now. Eco the noisy ones.`;
  const badge = $("#nav-red-count");
  if (badge) { badge.textContent = reduced || ""; badge.classList.toggle("hidden", !reduced); }
  $("#red-stats").innerHTML = [
    { t: "candidates", v: procs.length, n: "your user processes, critical ones excluded" },
    { t: "> 1% cpu", v: busy, n: "worth an Eco pass" },
    { t: "in eco", v: reduced, n: "below-normal priority + EcoQoS" },
    { t: "killed (session)", v: redCache.killed || 0, n: "explicit kills only, never bulk" },
  ].map((c) => `<div class="stat-card"><small>${c.t}</small><b>${c.v}</b><span>${esc(c.n)}</span></div>`).join("");
  $("#red-list").innerHTML = procs.map((p) => `
    <div class="red-row ${p.eco ? "eco" : ""}">
      <b class="red-name" title="pid ${p.pid}">${esc(p.name)}</b>
      <span class="red-cpu mono">${p.cpu >= 10 ? Math.round(p.cpu) : p.cpu.toFixed(1)}%</span>
      <span class="red-ram mono">${p.ramMB} MB</span>
      <span class="red-cls">${esc(p.cls)}${p.eco ? " · ECO" : ""}</span>
      <span class="red-act">
        <button class="btn sm ${p.eco ? "" : "primary"}" data-act="${p.eco ? "undo" : "eco"}" data-pid="${p.pid}">${p.eco ? "↺ Undo" : "⏬ Eco"}</button>
        <button class="btn sm" data-act="boost" data-pid="${p.pid}" title="above-normal priority">⚡</button>
        <button class="btn sm danger" data-act="kill" data-pid="${p.pid}" title="terminate this process">✕</button>
      </span>
    </div>`).join("") || `<div class="conv-empty muted" style="padding:14px">nothing to show</div>`;
  $$("#red-list .btn").forEach((b) => b.addEventListener("click", async () => {
    const r = await api("/api/reducer/act", { action: b.dataset.act, pid: +b.dataset.pid });
    toast(r.ok ? (b.dataset.act === "kill" ? "✕ killed " + b.dataset.pid : b.dataset.act === "eco" ? "⏬ Eco applied to " + b.dataset.pid : "process updated") : "✖ " + r.error, 3200);
    refreshReducer();
  }));
}
on("#btn-red-eco-all", async () => {
  overlay.show("Process Reducer", "Eco (EcoQoS + below-normal priority) on everything above 1% CPU");
  const r = await api("/api/reducer/act", { action: "eco-all", minCpu: 1 });
  await overlay.done(true, r.n + " processes reduced — Undo all restores them");
  refreshReducer();
});
on("#btn-red-eco-all2", async () => {
  overlay.show("Process Reducer", "Eco on every candidate process");
  const r = await api("/api/reducer/act", { action: "eco-all", minCpu: 0 });
  await overlay.done(true, r.n + " processes reduced");
  refreshReducer();
});
on("#btn-red-undo-all", async () => {
  const r = await api("/api/reducer/act", { action: "undo-all" });
  toast("↺ " + r.n + " processes restored");
  refreshReducer();
});
on("#btn-red-refresh", refreshReducer);

/* ===================== SECURITY SCAN ===================== */
let secCache = null;
async function refreshSecurity(auto) {
  if (!secCache) { $("#sec-list").innerHTML = `<div class="conv-empty muted" style="padding:14px">Press “Run full security scan” — read-only, a few seconds.</div>`; return; }
  renderSecurity();
}
function renderSecurity() {
  if (!secCache) return;
  const f = secCache.findings || [];
  const s = secCache.summary || { high: 0, med: 0, low: 0, ok: 0 };
  const risk = s.high * 10 + s.med * 4 + s.low;
  $("#sec-num").textContent = s.high + s.med + s.low;
  const ring = $("#view-security .gc-ring");
  ring.style.setProperty("--p", Math.max(4, Math.min(100, 100 - risk)));
  $("#sec-summary").innerHTML = s.high
    ? `<b style="color:var(--err)">${s.high} high-risk finding(s)</b> — review them below, each row says what and where.`
    : s.med ? `<b style="color:var(--warn)">No high findings, ${s.med} worth attention.</b>`
    : `<b style="color:var(--ok)">Clean bill.</b> ${s.ok} checks passed.`;
  const badge = $("#nav-sec-count");
  if (badge) { const n = s.high + s.med; badge.textContent = n || ""; badge.classList.toggle("hidden", !n); }
  $("#sec-stats").innerHTML = [
    { t: "high", v: s.high, n: "act today", cls: s.high ? "high" : "" },
    { t: "medium", v: s.med, n: "worth a look", cls: s.med ? "med" : "" },
    { t: "low", v: s.low, n: "informational", cls: "" },
    { t: "passed", v: s.ok, n: "checks green", cls: "good" },
  ].map((c) => `<div class="stat-card ${c.cls}"><small>${c.t}</small><b>${c.v}</b><span>${esc(c.n)}</span></div>`).join("");
  const SEVIC = { high: "high", med: "medium", low: "low", ok: "info", info: "info" };
  $("#sec-list").innerHTML = f.map((x) => `
    <div class="finding sec-finding sev-anim" style="animation-delay:${Math.min((x.sev === "high" ? 0 : 20) + f.indexOf(x) * 26, 900)}ms">
      <span class="sev ${SEVIC[x.sev] || "info"}"></span>
      <div class="fbody">
        <b>${esc(x.title)}</b>
        <p>${esc(x.detail)}</p>
        <p class="mono" style="font-size:10px;opacity:.75">${esc(x.where)}</p>
      </div>
      <span class="fx">
        ${x.id ? `<button class="btn sm danger" data-id="${esc(x.id)}">Revoke</button>` : ""}
      </span>
    </div>`).join("");
  $$("#sec-list .btn[data-id]").forEach((b) => b.addEventListener("click", async () => {
    const r = await api("/api/security/revoke", { id: b.dataset.id });
    toast(r.ok ? "✖ entry revoked (stashed — Restore possible)" : "✖ " + r.error, 3600);
    runSecurityScan();
  }));
}
async function runSecurityScan() {
  overlay.show("Security scan", "ports, connections, persistence, disk artifacts — read-only");
  overlay.step("antivirus + firewall"); overlay.progress(15);
  const p = api("/api/security/scan").then((r) => { overlay.progress(70); return r; });
  // the scan takes a few seconds; keep the overlay honest with steps
  overlay.step("listening ports + connections"); overlay.progress(45);
  overlay.step("persistence keys + startup"); overlay.progress(65);
  overlay.step("disk artifacts + UAC"); overlay.progress(85);
  try {
    secCache = await p;
    (secCache.findings || []).filter((x) => x.sev === "high").slice(0, 5).forEach((x) => overlay.step("✖ " + x.title, "er"));
    const s = secCache.summary;
    await overlay.done(true, `${s.high} high · ${s.med} medium · ${s.low} low`);
  } catch (e) { await overlay.done(false, String(e)); }
  renderSecurity();
}
on("#btn-sec-run", runSecurityScan);
on("#btn-sec-refresh", () => secCache ? renderSecurity() : runSecurityScan());
on("#btn-sec-defender", () => api("/api/tools", { tool: "ms-settings:windowsdefender" }));

/* ===================== DiskScope storage ===================== */
async function refreshStorage() {
  try {
    const s = await api("/api/storage");
    window.__lastJunk = s.junkBytes || 0;
    $("#drives-grid").innerHTML = s.drives.map((d) => {
      const usedPct = Math.round(((d.total - d.free) / d.total) * 100);
      const cls = usedPct > 90 ? "crit" : usedPct > 75 ? "warn" : "";
      return `<div class="drive-card">
        <div class="d-top"><span class="d-letter">${esc(d.letter)}</span><span class="d-sub">${esc(d.bus || "")} ${esc(d.fs || "")}</span></div>
        <div class="d-bar"><i class="${cls}" style="width:${usedPct}%"></i></div>
        <div class="d-sub">${fmtB(d.total - d.free)} used of ${fmtB(d.total)} · ${fmtB(d.free)} free</div>
      </div>`;
    }).join("");
    const pick = $("#disk-pick");
    if (pick && !pick.options.length) {
      pick.innerHTML = s.drives.map((d) => `<option>${esc(d.letter)}</option>`).join("");
      pick.addEventListener("change", () => scopeFolders());
    }
    scopeFolders();
    cleanupTargets();
    const files = await api("/api/storage/files", { root: "C:\\", top: 12 });
    $("#largest-files").classList.remove("conv-empty");
    $("#largest-files").innerHTML = files.map((f) =>
      `<div class="file-row"><span class="fp">${esc(f.path)}</span><span class="fs">${esc(f.pretty)}</span></div>`).join("");
  } catch (e) { }
}
async function scopeFolders() {
  const drive = ($("#disk-pick")?.value || "C:").replace(/[:\\]/g, "");
  const box = $("#disk-folders");
  if (!box) return;
  box.innerHTML = `<div class="conv-empty muted" style="padding:14px">measuring ${esc(drive)}\ top folders…</div>`;
  try {
    const r = await api("/api/disk/folders", { drive, top: 16 });
    const max = Math.max(...r.folders.map((f) => f.bytes), 1);
    $("#disk-folders-note").textContent = r.truncated ? "measurement budget reached — partial view" : "full first-level pass";
    box.innerHTML = r.folders.map((f, i) => `
      <div class="dsz-row" style="animation-delay:${Math.min(i * 40, 600)}ms">
        <span class="dsz-name" title="${esc(f.path)}">${esc(f.path.split("\\").pop() || f.path)}</span>
        <span class="dsz-bar"><i style="width:${Math.max(2, Math.round(f.bytes / max * 100))}%"></i></span>
        <b class="dsz-bytes mono">${fmtB(f.bytes)}</b>
        <span class="muted mono" style="font-size:10px;width:70px;text-align:right">${f.files} files</span>
      </div>`).join("");
  } catch (e) { box.innerHTML = `<div class="conv-empty muted" style="padding:14px">folder measurement failed</div>`; }
}
on("#btn-disk-scope", scopeFolders);
async function cleanupTargets() {
  try {
    const r = await api("/api/disk/cleanup");
    const t = r.targets.filter((x) => x.exists).sort((a, b) => b.bytes - a.bytes);
    $("#disk-cleanup-total").textContent = fmtB(r.total) + " reclaimable across " + t.length + " targets";
    $("#disk-cleanup").innerHTML = t.map((x) => `
      <div class="k-panel dsz-card">
        <b style="font-size:12px">${esc(x.path.split("\\").slice(-2).join("\\"))}</b>
        <span class="muted" style="font-size:11px">${esc(x.reason)}</span>
        <div class="dsz-foot"><b class="mono" style="color:var(--accent)">${fmtB(x.bytes)}</b><span class="muted mono" style="font-size:10px">${x.files} files</span></div>
      </div>`).join("") || `<div class="conv-empty muted" style="padding:14px">every cache is already clean 🎉</div>`;
  } catch (e) { }
}
on("#btn-disk-dupes", async () => {
  const box = $("#disk-dupes");
  box.innerHTML = `<div class="conv-empty muted" style="padding:14px">hashing files &gt; 32 MB (SHA-256, bounded)…</div>`;
  try {
    const r = await api("/api/disk/duplicates", { root: "C:\\Users", max: 3000 });
    $("#disk-dupes-note").textContent = `${r.scanned} big files scanned · ${r.hashed} hashed · ${fmtB(r.wasted)} wasted`;
    box.innerHTML = r.groups.length ? r.groups.map((g) => `
      <div class="dupe-group">
        <div class="dupe-head"><b>${g.count} × same content</b><span class="mono">${fmtB(g.size)} each</span><b style="color:var(--warn)">${fmtB(g.wasted)} wasted</b></div>
        ${g.paths.map((p) => `<div class="file-row"><span class="fp">${esc(p)}</span></div>`).join("")}
      </div>`).join("") : `<div class="conv-empty muted" style="padding:14px">no duplicates found in the scanned set 🎉</div>`;
  } catch (e) { box.innerHTML = `<div class="conv-empty muted" style="padding:14px">duplicate scan failed</div>`; }
});

/* ===================== startup ===================== */
async function refreshStartup() {
  try {
    const list = await api("/api/startup");
    $("#startup-list").innerHTML = list.length ? list.map((e, i) => `
      <div class="su-row">
        <b style="min-width:170px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis">${esc(e.name)}</b>
        <span class="su-cmd mono">${esc(e.command)}</span>
        <span class="su-loc">${esc(e.location)}</span>
        <button class="btn sm danger" data-i="${i}">Disable</button>
      </div>`).join("") : `<div class="conv-empty muted" style="padding:14px">No startup entries 🎉</div>`;
    $$("#startup-list .btn").forEach((b) => b.addEventListener("click", async () => {
      const e = list[+b.dataset.i];
      const r = await api("/api/startup/toggle", { location: e.location, name: e.name, enable: false });
      toast(r.ok ? "↺ disabled: " + e.name : "✖ " + r.error);
      refreshStartup();
    }));
  } catch (e) { }
}
on("#btn-startup-refresh", refreshStartup);

/* ===================== benchmark ===================== */
function fmtNum(v, d = 0) { return (v === null || v === undefined || isNaN(v)) ? "—" : Number(v).toFixed(d); }
function renderBench(b, prev) {
  const hero = $("#bench-hero"); if (!hero) return;
  hero.classList.remove("hidden");
  const tot = b.total ?? null;
  $("#bh-total").textContent = tot === null ? "—" : tot;
  $("#bh-class").textContent = b.class ? b.class.toUpperCase() : "—";
  $("#bh-verdict").textContent = b.verdict || "";
  const ring = $("#bh-ring");
  ring.style.setProperty("--p", Math.min(100, (tot || 0) / 40));   // ring out of 4000
  ring.style.background = `conic-gradient(var(--accent) calc(var(--p)*1%),rgba(255,255,255,.06) 0)`;
  const SUBS = [["cpuMt","CPU multi-core",40],["cpuSt","CPU single-core",15],["ram","Memory",20],["disk","Storage",25]];
  $("#bh-subs").innerHTML = (b.subs ? SUBS.map(([k, label, w]) => {
    const v = b.subs[k];
    if (v === undefined) return "";
    const pw = Math.min(100, v / 30);
    const d = prev && prev.subs && prev.subs[k] !== undefined ? prev.subs[k] - v : null;
    const dtxt = d === null ? "" : `<span class="bh-delta ${d >= 0 ? "up" : "down"}">${d >= 0 ? "▲" : "▼"} ${Math.abs(d)}</span>`;
    return `<div class="bh-sub"><span class="bh-l">${label} <em>w ${Math.round(w * 100) / 10}%</em></span>
      <div class="bh-bar"><i style="width:${pw}%"></i></div><b>${v}</b>${dtxt}</div>`;
  }).join("") : `<div class="muted" style="font-size:12px">Sub-scores unavailable for this run.</div>`);
  $("#bh-raw").innerHTML =
    `<span>CPU ${fmtNum(b.cpu?.score)} ${b.cpu?.unit || "MOPS"}</span><span>ST ${fmtNum(b.cpuSt?.score)} MOPS</span>` +
    `<span>RAM ${fmtNum(b.ram?.score, 1)} GB/s</span><span>DISK ${fmtNum(b.disk?.score)} MB/s</span>` +
    `<span>PING ${b.latencyMs > 0 ? fmtNum(b.latencyMs, 1) + " ms" : "—"}</span>`;
  $("#bench-cards").innerHTML = "";
}
async function refreshBenchHistory() {
  try {
    const h = await api("/api/bench/history");
    if (h.length) renderBench(h[h.length - 1], h.length > 1 ? h[h.length - 2] : null);
    $("#bench-history").innerHTML = h.length ? h.slice().reverse().map((b, i, arr) => {
      const prev = arr[i + 1];
      const d = b.total && prev && prev.total ? b.total - prev.total : null;
      const dtxt = d === null ? "" : `<span class="bh-delta ${d >= 0 ? "up" : "down"}">${d >= 0 ? "+" : ""}${d}</span>`;
      return `<div class="bench-row"><span class="mono">${new Date(b.timestamp * 1000).toLocaleString()}</span>
        <span><b class="bh-tot">${b.total ?? "—"}</b> total ${dtxt}</span>
        <span>CPU ${fmtNum(b.cpu?.score)} ${b.cpu?.unit || ""}</span>
        <span>RAM ${fmtNum(b.ram?.score, 1)} ${b.ram?.unit || ""}</span>
        <span>DISK ${fmtNum(b.disk?.score)} ${b.disk?.unit || ""}</span>
        <span>PING ${b.latencyMs > 0 ? fmtNum(b.latencyMs, 1) + " ms" : "—"}</span></div>`;
    }).join("")
      : `<div class="conv-empty muted" style="padding:14px">No runs yet — hit "Run full benchmark".</div>`;
  } catch (e) { }
}
on("#btn-bench-run", async () => {
  const btn = $("#btn-bench-run"); btn.disabled = true; btn.textContent = "▲ Measuring…";
  toast("Benchmark running — CPU / RAM / disk under real load, ~4 s");
  try {
    const h = await api("/api/bench/history");
    const b = await api("/api/bench");
    renderBench(b, h.length ? h[h.length - 1] : null);
    toast("✔ Benchmark done — saved to history");
  } catch (e) { toast("✖ benchmark failed: " + e.message); }
  btn.disabled = false; btn.textContent = "▲ Run full benchmark";
  refreshBenchHistory();
});

/* ===================== tools — full launcher, categorized ===================== */
const TOOL_CATS = [
  ["system", "System & Admin"],
  ["perf", "Performance"],
  ["gaming", "Gaming"],
  ["net", "Network"],
  ["security", "Security"],
  ["storage", "Storage"],
  ["av", "Display & Sound"],
  ["settings", "Windows Settings"],
  ["dev", "Power user"],
];
const TOOLS = [
  // system & admin
  ["system","taskmgr","Task Manager","processes & resources"],
  ["system","resmon","Resource Monitor","CPU/disk/net in depth"],
  ["system","perfmon","Performance Monitor","counters & graphs"],
  ["system","devmgmt","Device Manager","drivers & devices"],
  ["system","eventvwr","Event Viewer","system & app logs"],
  ["system","services","Services","manage services"],
  ["system","msinfo32","System Information","full hardware report"],
  ["system","compmgmt","Computer Management","everything in one console"],
  ["system","taskchd","Task Scheduler","scheduled tasks"],
  ["system","regedit","Registry Editor","advanced"],
  ["system","msconfig","System Configuration","boot & startup"],
  ["system","optionalfeatures","Windows Features","toggle components"],
  ["system","lusrmgr","Users & Groups","local accounts"],
  ["system","certmgr","Certificate Manager","certificates"],
  ["system","sysdm","System Properties","computer name, env, remote"],
  ["system","appwiz","Programs & Features","uninstall apps"],
  ["system","hdwwiz","Hardware Wizard","legacy devices"],
  ["system","winver","About Windows","version & build"],
  // performance
  ["perf","powercfg","Power Options","power plans"],
  ["perf","sysperf","Visual Effects","animations & effects"],
  ["perf","sysadvanced","Advanced System","virtual memory, DEP"],
  ["perf","sysprotection","System Protection","restore points"],
  ["perf","rstrui","System Restore","rollback to a point"],
  ["perf","mdsched","Memory Diagnostic","RAM test on reboot"],
  ["perf","verifier","Driver Verifier","catch bad drivers"],
  ["perf","ms-settings:storagesense","Storage Sense","auto disk cleanup"],
  ["perf","ms-settings:startupapps","Startup Apps","what boots with Windows"],
  ["perf","sysdefaultinput","Troubleshooters","control /name fixers"],
  // gaming
  ["gaming","ms-settings:gaming-gamemode","Game Mode","Windows gaming mode"],
  ["gaming","ms-settings:gaming-gamebar","Game Bar","overlay & shortcuts"],
  ["gaming","ms-settings:gaming-captures","Captures","background recording"],
  ["gaming","ms-settings:gaming-broadcasting","Broadcasting","live streaming setup"],
  ["gaming","ms-settings:gaming-xboxnetworking","Xbox Networking","NAT & multiplayer"],
  ["gaming","ms-settings:display-advanced","Graphics Settings","per-app GPU preference"],
  ["gaming","joy","Game Controllers","gamepads & joysticks"],
  ["gaming","ms-settings:quiethours","Focus Assist","notifications while gaming"],
  // network
  ["net","ncpa","Network Connections","adapters"],
  ["net","firewall","Firewall (applet)","allowed apps"],
  ["net","wfmsc","Firewall (advanced)","rules & profiles"],
  ["net","inetcpl","Internet Options","proxy, TLS, LAN"],
  ["net","ms-settings:network","Network Status","connection overview"],
  ["net","ms-settings:network-proxy","Proxy","system proxy"],
  ["net","ms-settings:datausage","Data Usage","per-app traffic"],
  ["net","timedate","Time & Date","clock sync"],
  // security
  ["security","wscui","Windows Security","antivirus & firewall"],
  ["security","ms-settings:windowsdefender","Defender","virus protection"],
  ["security","secpol","Local Security Policy","security settings"],
  ["security","tpm","TPM Management","trusted platform chip"],
  ["security","ms-settings:windowsupdate","Windows Update","drivers & patches"],
  ["security","ms-settings:privacy","Privacy","permissions & telemetry"],
  ["security","ms-settings:signin","Sign-in Options","PIN, Windows Hello"],
  ["security","recoverydrive","Recovery Drive","USB rescue disk"],
  // storage
  ["storage","diskmgmt","Disk Management","partitions & letters"],
  ["storage","dfrgui","Defragment & Optimize","drive maintenance"],
  ["storage","cleanmgr","Disk Cleanup","Windows built-in"],
  ["storage","ms-settings:savelocations","Where Content Saves","default save drives"],
  ["storage","ms-settings:privacy-location","Location","per-app location"],
  // display & sound
  ["av","display","Display","resolution & scale"],
  ["av","ms-settings:nightlight","Night Light","blue light filter"],
  ["av","mmsys","Sound","playback & recording"],
  ["av","mouse","Mouse","pointer & buttons"],
  ["av","ms-settings:mousetouchpad","Mouse Settings","pointer speed"],
  ["av","ms-settings:themes","Themes","wallpapers & colors"],
  ["av","ms-settings:colors","Colors","accent & dark mode"],
  // windows settings
  ["settings","ms-settings:","Windows Settings","home page"],
  ["settings","ms-settings:about","About","device specs & Windows build"],
  ["settings","ms-settings:bluetooth","Bluetooth & Devices","pairing"],
  ["settings","ms-settings:defaultapps","Default Apps","file associations"],
  ["settings","ms-settings:multitasking","Multitasking","snap & desktops"],
  ["settings","ms-settings:appsfeatures","Installed Apps","manage per app"],
  ["settings","ms-settings:fonts","Fonts","installed fonts"],
  ["settings","ms-settings:windowsactivation","Activation","license status"],
  // power user
  ["dev","wt","Windows Terminal","modern terminal"],
  ["dev","powershell","PowerShell","automation"],
  ["dev","cmd","Command Prompt","classic shell"],
  ["dev","gpedit","Group Policy","local policy editor"],
  ["dev","wmimgmt","WMI Management","instrumentation"],
  ["dev","ms-settings:developers","Developer Mode","side-loading & dev"],
  ["dev","ms-settings:advanced-startup","Advanced Startup","boot options"],
];
let toolsCat = "all";
function renderTools() {
  const q = ($("#tools-search").value || "").toLowerCase();
  const items = TOOLS.filter(([cat, id, n, d]) =>
    (toolsCat === "all" || cat === toolsCat) &&
    (!q || (n + " " + d + " " + id).toLowerCase().includes(q)));
  const byCat = {};
  for (const [cat, id, n, d] of items) (byCat[cat] = byCat[cat] || []).push([id, n, d]);
  $("#tools-grid-wrap").innerHTML = TOOL_CATS.filter(([c]) => byCat[c]).map(([c, label]) => `
    <div class="sec-head" style="margin-top:14px"><h2 style="font-size:14px">${label}</h2><span class="muted" style="font-size:11px">${byCat[c].length}</span></div>
    <div class="quick-grid">${byCat[c].map(([id, n, d]) => `<div class="tool-card" data-t="${id}"><b>${n}</b><span>${d}</span></div>`).join("")}</div>`
  ).join("") || `<div class="conv-empty muted" style="padding:20px">no tool matches “${esc(q)}”</div>`;
  $$("#tools-grid-wrap .tool-card").forEach((card) => card.addEventListener("click", async () => {
    await api("/api/tools", { tool: card.dataset.t });
    toast("Opening " + card.querySelector("b").textContent + "…");
  }));
  const total = TOOLS.filter(([c]) => toolsCat === "all" || c === toolsCat).length;
  $("#tools-count").textContent = total + " tools";
}
$("#tools-cats").innerHTML = [`<span class="chip on" data-cat="all">All · ${TOOLS.length}</span>`]
  .concat(TOOL_CATS.map(([c, label]) => `<span class="chip" data-cat="${c}">${label}</span>`)).join("");
$$("#tools-cats .chip").forEach((ch) => ch.addEventListener("click", () => {
  $$("#tools-cats .chip").forEach((x) => x.classList.remove("on"));
  ch.classList.add("on"); toolsCat = ch.dataset.cat; renderTools();
}));
$("#tools-search").addEventListener("input", renderTools);
renderTools();

/* ===================== logs ===================== */
let logSevClass = { INFO: "", SUCCESS: "ok", WARNING: "warn", ERROR: "err", CRITICAL: "err", DEBUG: "", TRACE: "" };
let logFilterText = "";
async function refreshLogs() {
  try {
    const r = await api("/api/logs2");
    renderLogs(r.items);
  } catch (e) { }
}
function renderLogs(items) {
  const box = $("#log-box");
  box.innerHTML = items
    .filter((i) => !logFilterText || (i.msg + i.cat + i.sev).toLowerCase().includes(logFilterText))
    .map((i) => `<div class="log-line ${logSevClass[i.sev] || ""}"><span class="log-t">${esc(i.time)}</span><b style="color:var(--muted-2)">[${esc(i.cat)}]</b> ${esc(i.msg)}</div>`)
    .join("");
  box.scrollTop = box.scrollHeight;
}
on("#btn-log-refresh", refreshLogs);
on("#btn-log-open", () => fetch("/api/open?what=log"));
on("#btn-open-logfolder", () => fetch("/api/open?what=logfolder"));
$("#log-filter").addEventListener("input", async (e) => { logFilterText = e.target.value.toLowerCase(); await refreshLogs(); });

/* ===================== settings ===================== */
async function loadSettings() {
  try {
    const s = await api("/api/settings");
    $("#set-accent").value = currentAccent || "#ff3d57";
    $("#set-theme").value = s.ui_theme || "magma";
    $("#set-lang").value = s.ui_lang || "en";
    applyLang(s.ui_lang || "en");
    applyVisualPrefs(s.ui_particles, s.ui_glitch_text);   // these two settings are now actually applied
    $("#set-confirm").checked = s.confirm_destructive !== false;
    $("#set-autoback").checked = s.auto_backup !== false;
    $("#set-dns").checked = s.dns_managed === true;
    $("#set-killlist").value = (s.gaming_kill_list || []).join(", ");
    const dlOpt = $("#set-dl-opt"); if (dlOpt) dlOpt.checked = s.delivery_optimization_off === true;
    const cons = $("#set-consumer"); if (cons) cons.checked = s.consumer_features_off === true;
    const fwNag = $("#set-fw-nag"); if (fwNag) fwNag.checked = s.firmware_nag === true;
    applyAccent(s.ui_accent || "#ff3d57");
  } catch (e) { }
}
on("#btn-set-save", async () => {
  const body = {
    ui_accent: $("#set-accent").value,
    ui_theme: $("#set-theme").value,
    ui_lang: $("#set-lang").value,
    ui_particles: $("#set-anim").checked,
    ui_glitch_text: $("#set-glitch").checked,
    confirm_destructive: $("#set-confirm").checked,
    auto_backup: $("#set-autoback").checked,
    dns_managed: $("#set-dns").checked,
    gaming_kill_list: $("#set-killlist").value.split(",").map((x) => x.trim()).filter(Boolean),
  };
  await api("/api/settings", body);
  applyThemePack(body.ui_theme);
  applyAccent(body.ui_accent);
  rememberAccent(body.ui_theme, body.ui_accent);
  applyVisualPrefs(body.ui_particles, body.ui_glitch_text);
  applyLang(body.ui_lang);
  $("#set-saved").textContent = "saved ✓";
  setTimeout(() => ($("#set-saved").textContent = ""), 2200);
  toast("✔ Settings saved");
});
on("#btn-set-dns-reset", async () => {
  if (window.confirm("Clear the configured DNS on every physical adapter and go back to the router's DHCP?")) {
    const r = await api("/api/net/dns/reset", {});
    toast(r.ok ? "↺ DNS back to DHCP default" : "✖ " + (r.error || "failed"), 4000);
  }
});
on("#btn-set-fw", () => show("bios"));
on("#btn-set-drvscan", async () => {
  try { await api("/api/drvupdate/scan", { driversOnly: true }); toast("⤓ driver scan triggered"); }
  catch (e) { toast("✖ scan failed"); }
});
// the two update toggles apply their matching tweak instantly (reversible from Tweaks)
document.addEventListener("change", async (e) => {
  if (e.target && e.target.id === "set-dl-opt") {
    const onb = e.target.checked;
    await api("/api/tweaks/" + (onb ? "apply" : "restore"), ["delivery_optimization"]);
    await api("/api/settings", { delivery_optimization_off: onb });
    toast(onb ? "⏬ Delivery Optimization disabled (P2P upload off)" : "↺ Delivery Optimization back to Windows default");
    updateTweakState(true);
  }
  if (e.target && e.target.id === "set-consumer") {
    const onb = e.target.checked;
    await api("/api/tweaks/" + (onb ? "apply" : "restore"), ["consumer_features"]);
    await api("/api/settings", { consumer_features_off: onb });
    toast(onb ? "⏬ Store suggestions & sponsored apps blocked" : "↺ Store suggestions back to default");
    updateTweakState(true);
  }
});

/* ===================== DIAGNOSTICS ===================== */
let diagCache = null;
async function refreshDiag(force) {
  if (diagCache && !force) return renderDiag();
  try {
    diagCache = await api("/api/diagnostics");
    renderDiag();
  } catch (e) { toast("✖ diagnostics failed"); }
}
function renderDiag() {
  const d = diagCache; if (!d) return;
  $("#dg-clock").textContent = d.clockMHz ? (d.clockMHz / 1000).toFixed(2) + " GHz" : "—";
  $("#dg-clock-sub").textContent = d.clockMHz ? "base × measured performance" : "could not read";
  $("#dg-dpc").textContent = d.dpc.available ? d.dpc.dpcPercent + "% / " + d.dpc.isrPercent + "%" : "—";
  $("#dg-dpc-sub").textContent = d.dpc.available ? d.dpc.verdict : "unavailable";
  $("#dg-disk").textContent = d.disk.available ? d.disk.avgLatencyMs + " ms" : "—";
  $("#dg-disk-sub").textContent = d.disk.available ? d.disk.verdict : "could not measure";
  const n = d.network;
  $("#diag-net").innerHTML = n.available ? `
    <div class="net-card"><span>Ping avg</span><b>${n.avgMs} ms</b></div>
    <div class="net-card"><span>Min / Max</span><b>${n.minMs} / ${n.maxMs}</b></div>
    <div class="net-card"><span>Jitter</span><b style="color:${n.jitterMs > 2 ? "var(--warn)" : "var(--ok)"}">${n.jitterMs} ms</b></div>
    <div class="net-card"><span>Packet loss</span><b style="color:${n.lossPercent > 0 ? "var(--err)" : "var(--ok)"}">${n.lossPercent}%</b></div>
    <div class="net-card"><span>Verdict</span><b style="font-size:12px">${esc(n.verdict)}</b></div>`
    : `<div class="net-card"><span>Network</span><b>no response</b></div>`;
  $("#diag-cards").innerHTML = `
    <div class="stat-card ${d.game.running ? "high" : "good"}"><b>${d.game.running ? "IN GAME" : "IDLE"}</b><span>${esc(d.game.process || "no game detected")}</span></div>
    <div class="stat-card ${d.dpc.available && d.dpc.dpcPercent < 5 ? "good" : "med"}"><b>${d.dpc.available ? d.dpc.dpcPercent + "%" : "—"}</b><span>DPC load</span></div>
    <div class="stat-card good"><b>${d.network.jitterMs ?? "—"}</b><span>jitter ms</span></div>`;
}
on("#btn-diag-run", async () => {
  overlay.show("Running diagnostics", "2 s DPC sample + 20 pings + disk test — everything measured, nothing invented");
  overlay.step("reading effective CPU clock"); overlay.progress(20);
  try {
    diagCache = await api("/api/diagnostics");
    overlay.progress(100); overlay.step("✓ all measurements done", "ok");
    await overlay.done(true);
    renderDiag();
  } catch (e) { await overlay.done(false, String(e)); }
});

/* ===================== ESPORT MODE ===================== */
const ESPORT_IDS = ["game_mode", "game_dvr_off", "timer_high", "network_gaming", "mouse_precision", "menu_delay_0", "background_apps", "win32_priority", "hags_on", "power_ultimate", "usb_powersave", "pcie_aspm", "vrr"];
async function toggleEsport() {
  const btn = $("#btn-esport");
  const active = btn.classList.toggle("on");
  if (active) {
    overlay.show("ESPORT MODE", "applying the competitive preset — everything is remembered and reversible");
    overlay.step("applying " + ESPORT_IDS.length + " esport settings"); overlay.progress(30);
    try {
      const r = await api("/api/tweaks/apply", ESPORT_IDS);
      overlay.progress(100);
      overlay.step(`✓ ${r.applied} applied${r.errors.length ? " · " + r.errors.length + " need admin" : ""}`, "ok");
      await overlay.done(true, r.errors.length ? r.applied + " applied, " + r.errors.length + " need admin — run OptimizeKit-Admin.bat" : "competitive preset active");
      toast("⚑ ESPORT MODE ON — " + r.applied + " optimizations", 4000);
    } catch (e) { await overlay.done(false, String(e)); }
  } else {
    overlay.show("Leaving ESPORT MODE", "restoring Windows defaults for the esport set");
    try {
      const r = await api("/api/tweaks/restore", ESPORT_IDS);
      await overlay.done(true, r.restored + " restored");
      toast("↺ esport preset off");
    } catch (e) { await overlay.done(false, String(e)); }
  }
  updateTweakState();
}
on("#btn-esport", toggleEsport);
on("#btn-boost", (e) => quickBoost(e.currentTarget));

/* ===================== SMART OPTIMIZE ===================== */
let smartPlan = null, smartGoal = "gaming";
const smartSel = new Set();
async function refreshSmart(force) {
  if (smartPlan && !force && smartPlan.goal === smartGoal) return renderSmart();
  try {
    smartPlan = await api("/api/smart?goal=" + encodeURIComponent(smartGoal));
    // drop selections that are gone / already applied
    for (const id of Array.from(smartSel)) {
      const it = smartPlan.items.find((x) => x.id === id);
      if (!it || it.applied) smartSel.delete(id);
    }
    renderSmart();
  } catch (e) { toast("✖ smart analysis failed"); }
}
function smartSelUI() {
  const n = smartSel.size;
  $("#sm-sel").textContent = n;
  $("#btn-smart-apply").disabled = n === 0;
}
function renderSmart() {
  const p = smartPlan; if (!p) return;
  const ring = $("#view-smart .gc-ring");
  if (ring) { ring.style.setProperty("--p", p.score); ring.style.background = `conic-gradient(var(--accent) calc(var(--p)*1%),rgba(255,255,255,.07) 0)`; }
  $("#sm-num").textContent = p.score;
  $("#sm-summary").innerHTML = `<b>${esc(p.headline)}</b><br/><span class="muted" style="font-size:11.5px">goal <b style="color:var(--accent)">${esc(p.goal)}</b> · ${p.admin ? "running as administrator" : "standard user — admin items are listed but cannot apply"}</span>`;
  $("#sm-stats").innerHTML = `
    <div class="stat-card good"><b>${p.applied}</b><span>applied</span></div>
    <div class="stat-card med"><b>${p.pending}</b><span>on the table</span></div>
    <div class="stat-card ${p.adminMissing ? "high" : "good"}"><b>${p.adminMissing}</b><span>need admin</span></div>
    <div class="stat-card"><b>${p.total}</b><span>catalog size</span></div>`;
  const pending = p.items.filter((i) => !i.applied).length;
  $("#sm-count").textContent = `${pending} pending · ranked for ${p.goal}`;
  const list = $("#smart-list");
  list.innerHTML = p.items.map((it, i) => {
    const sel = smartSel.has(it.id);
    const sev = it.applied ? "low" : it.impact >= 3 ? "high" : it.impact === 2 ? "medium" : "low";
    return `<div class="finding ${it.applied ? "is-done" : ""} ${sel ? "is-sel" : ""}" data-id="${esc(it.id)}" style="animation-delay:${Math.min(i * 14, 300)}ms">
      <span class="sev ${sev}"></span>
      <div class="fbody">
        <b>${esc(it.name)}</b>
        <p>${esc(it.desc)}</p>
        <div class="tw2-foot">
          <span class="badge ${it.admin ? "badge-admin" : "badge-user"}">${it.admin ? "ADMIN" : "USER"}</span>
          <span class="badge ${it.applied ? "badge-on" : ""}">${it.applied ? "APPLIED" : "PENDING"}</span>
          <span class="badge">${esc(it.category)}</span>
          <span class="badge">weight ${it.weight}</span>
        </div>
        <p class="sm-why">${esc(it.why)}</p>
      </div>
      <div class="fx">
        <span class="impact">${"▮".repeat(it.impact || 1)}</span>
        ${it.applied
          ? `<button class="btn sm" data-restore="${esc(it.id)}">↺ Restore</button>`
          : `<input type="checkbox" class="sm-pick" data-pick="${esc(it.id)}" ${sel ? "checked" : ""} />`}
      </div>
    </div>`;
  }).join("") || `<div class="conv-empty muted" style="padding:14px">nothing to show</div>`;
  $$("#smart-list .sm-pick").forEach((cb) => cb.addEventListener("change", () => {
    if (cb.checked) smartSel.add(cb.dataset.pick); else smartSel.delete(cb.dataset.pick);
    cb.closest(".finding")?.classList.toggle("is-sel", cb.checked);
    smartSelUI();
  }));
  $$("#smart-list [data-restore]").forEach((b) => b.addEventListener("click", async () => {
    b.disabled = true;
    const r = await api("/api/tweaks/restore", [b.dataset.restore]);
    toast(r.restored > 0 ? "↺ restored to Windows default" : "✖ " + (r.errors[0] || "failed"));
    await updateTweakState(true); await refreshSmart(true);
  }));
  smartSelUI();
}
$("#sm-goals").addEventListener("click", (e) => {
  const chip = e.target.closest(".chip[data-goal]"); if (!chip) return;
  $$("#sm-goals .chip").forEach((c) => c.classList.remove("on"));
  chip.classList.add("on");
  smartGoal = chip.dataset.goal; smartSel.clear(); smartPlan = null;
  refreshSmart(true);
});
on("#btn-smart-refresh", () => { smartPlan = null; refreshSmart(true); toast("↻ re-analyzing"); });
on("#btn-smart-none", () => { smartSel.clear(); renderSmart(); });
on("#btn-smart-top", () => {
  smartSel.clear();
  (smartPlan?.items || []).filter((i) => !i.applied && !i.needAdmin).slice(0, 5).forEach((i) => smartSel.add(i.id));
  renderSmart(); toast("selected " + smartSel.size + " top items");
});
on("#btn-smart-apply", async () => {
  const ids = Array.from(smartSel); if (!ids.length) return;
  overlay.show("Smart Optimize", `${ids.length} change${ids.length > 1 ? "s" : ""} — snapshot first, then apply`);
  overlay.step("creating registry snapshot"); overlay.progress(12);
  await sleep(220);
  try {
    const r = await api("/api/tweaks/apply", ids);
    overlay.progress(96);
    (r.errors || []).slice(0, 6).forEach((e) => overlay.step("✖ " + e, "er"));
    overlay.step(`✓ ${r.applied} applied · ${ids.length - r.applied} skipped`, r.applied ? "ok" : "er");
    await overlay.done(r.applied > 0, `${r.applied} applied${r.errors.length ? " — " + r.errors.length + " need admin" : ""}`);
    toast(`✔ Smart Optimize: ${r.applied} applied`, 4200);
  } catch (e) { await overlay.done(false, String(e)); }
  smartSel.clear();
  await updateTweakState(true);
  await refreshSmart(true);
});

/* ===================== PACKS ===================== */
const PACKS = [
  { id: "esport", name: "⚑ Esport", featured: true, desc: "Competitive FPS: 0.5 ms timer, no background recording, raw mouse, background apps off.",
    ids: ["game_mode", "game_dvr_off", "timer_high", "network_gaming", "mouse_precision", "menu_delay_0", "background_apps", "win32_priority", "hags_on", "power_ultimate", "usb_powersave", "pcie_aspm", "vrr"] },
  { id: "lowlatency", name: "⏱ Low latency", featured: true, desc: "The latency set on its own: timer, input, network stack and interrupt handling.",
    ids: ["timer_high", "network_gaming", "mouse_precision", "usb_powersave", "pcie_aspm", "win32_priority", "bcdedit_tsc", "msi_mode"] },
  { id: "cleanboot", name: "🚀 Fast, clean boot", featured: true, desc: "Fewer things starting and running: Superfetch, indexing, telemetry tasks, boot logo.",
    ids: ["sysmain_off", "search_index", "hpets_boot", "telemetry_tasks", "edge_bing_blocking", "storage_sense"] },
  { id: "privacy", name: "🛡 Privacy lock-down", featured: true, desc: "Telemetry, advertising ID, activity history, Bing in search, Copilot.",
    ids: ["telemetry_off", "advertising_off", "activity_history", "bing_search", "tailored_experiences", "telemetry_tasks", "windows_copilot"] },
  { id: "streaming", name: "🎥 Play & stream", desc: "Keep capture available without fighting your encoder — game priority stays first.",
    ids: ["game_mode", "windowed_games", "vrr", "hags_on", "gpu_preference", "fso_on", "timer_high"] },
  { id: "laptop", name: "💻 Laptop / thermals", desc: "Cooler and quieter: compositor, transparency, animations and background churn off.",
    ids: ["transparency_off", "taskbar_anim", "background_apps", "search_index", "visual_fx_balloff", "mouse_trails"] },
];
function applyPackPreset(preset) {
  const grid = $("#packs-grid"); if (!grid) return;
  const want = (p) => preset.startsWith("pack:") ? p.id === preset.slice(5)
    : preset === "featured" ? !!p.featured : true;
  $$("#packs-grid .pack-card").forEach((card) => {
    const p = PACKS.find((x) => x.id === card.dataset.pack);
    card.style.display = p && want(p) ? "" : "none";
  });
}
async function renderPacks() {
  if (!tweaksCache.length) await updateTweakState(true);
  const grid = $("#packs-grid"); if (!grid) return;
  // v2.9: the bar row filters the grid — "In the spotlight" shows the featured packs first
  const bar = $("#packs-presets");
  if (bar && !bar.childElementCount) {
    bar.innerHTML =
      `<span class="chip on" data-preset="all">All</span>` +
      `<span class="chip" data-preset="featured">★ In the spotlight</span>` +
      PACKS.filter((p) => p.featured).map((p) => `<span class="chip" data-preset="pack:${p.id}">${p.name}</span>`).join("");
    bar.querySelectorAll(".chip").forEach((chip) => chip.addEventListener("click", () => {
      bar.querySelectorAll(".chip").forEach((c) => c.classList.remove("on"));
      chip.classList.add("on");
      applyPackPreset(chip.dataset.preset);
    }));
  }
  const preset = bar?.querySelector(".chip.on")?.dataset.preset || "all";
  const list = PACKS.filter((p) => {
    if (preset.startsWith("pack:")) return p.id === preset.slice(5);
    if (preset === "featured") return !!p.featured;
    return true;
  });
  grid.innerHTML = list.map((p) => {
    const known = p.ids.filter((id) => tweaksCache.some((t) => t.id === id));
    const on = known.filter((id) => tweaksCache.find((t) => t.id === id)?.applied).length;
    const pct = known.length ? Math.round(on / known.length * 100) : 0;
    return `<div class="pack-card ${pct === 100 ? "full" : pct ? "part" : ""} ${p.featured ? "featured" : ""}" data-pack="${p.id}">
      <div class="pc-top"><b>${p.name}</b>${p.featured ? '<span class="pc-star" title="In the spotlight">★</span>' : ""}<span class="pc-pct">${on}/${p.ids.length}</span></div>
      <p>${esc(p.desc)}</p>
      <div class="pc-bar"><i style="width:${pct}%"></i></div>
      <div class="pc-actions">
        <button class="btn sm primary" data-act="apply">Apply</button>
        <button class="btn sm" data-act="restore">Restore</button>
        <button class="btn sm" data-act="open">What's inside</button>
      </div>
    </div>`;
  }).join("");
  $("#pack-detail").classList.add("hidden");
  const busy = (b) => { b.disabled = true; setTimeout(() => (b.disabled = false), 1500); };
  $$("#packs-grid .pack-card").forEach((card) => card.addEventListener("click", async (e) => {
    const btn = e.target.closest("button[data-act]"); if (!btn) return;
    const p = PACKS.find((x) => x.id === card.dataset.pack); if (!p) return;
    busy(btn);
    if (btn.dataset.act === "open") return showPack(p);
    const applying = btn.dataset.act === "apply";
    overlay.show((applying ? "Applying pack · " : "Restoring pack · ") + p.name,
      `${p.ids.length} tweaks — snapshot first, reversible either way`);
    overlay.step((applying ? "apply " : "restore ") + p.ids.length + " items"); overlay.progress(30);
    try {
      const r = applying ? await api("/api/tweaks/apply", p.ids) : await api("/api/tweaks/restore", p.ids);
      overlay.progress(95);
      const n = applying ? r.applied : r.restored;
      (r.errors || []).slice(0, 5).forEach((x) => overlay.step("✖ " + x, "er"));
      overlay.step(`✓ ${n} ${applying ? "applied" : "restored"}`, n ? "ok" : "er");
      await overlay.done(n > 0, `${n}/${p.ids.length} ${applying ? "applied" : "restored"}${(r.errors || []).length ? " — some need admin" : ""}`);
      toast(`✔ ${p.name}: ${n} ${applying ? "applied" : "restored"}`, 4000);
    } catch (err) { await overlay.done(false, String(err)); }
    await updateTweakState(true); await renderPacks();
  }));
}
function showPack(p) {
  el.classList.remove("hidden");
  el.innerHTML = `
    <div class="profile-hero">
      <h2>${p.name} — all ${p.ids.length} tweaks</h2>
      <p>${esc(p.desc)}</p>
      <div class="pack-list">${p.ids.map((id) => {
        const t = tweaksCache.find((x) => x.id === id);
        if (!t) return `<span class="badge">${esc(id)}</span>`;
        return `<span class="badge ${t.applied ? "badge-on" : (t.admin ? "badge-admin" : "badge-user")}" title="${esc(t.desc)}">${t.applied ? "✓ " : ""}${esc(t.name)}</span>`;
      }).join("")}</div>
      <div style="display:flex;gap:10px;flex-wrap:wrap;margin-top:14px">
        <button class="btn primary" id="pdetail-apply">Apply whole pack</button>
        <button class="btn danger" id="pdetail-restore">↺ Restore whole pack</button>
        <button class="btn" id="pdetail-close">Close</button>
      </div>
    </div>`;
  $("#pdetail-close").addEventListener("click", () => el.classList.add("hidden"));
  $("#pdetail-apply").addEventListener("click", async (e) => { e.target.disabled = true; await api("/api/tweaks/apply", p.ids); await updateTweakState(true); await renderPacks(); showPack(p); toast("✔ pack applied"); });
  $("#pdetail-restore").addEventListener("click", async (e) => { e.target.disabled = true; await api("/api/tweaks/restore", p.ids); await updateTweakState(true); await renderPacks(); showPack(p); toast("↺ pack restored"); });
}

/* ===================== v2.3 TUNING CENTERS =====================
   Each center is a real module: it consolidates a slice of the catalog, shows the
   live state of the machine behind it, and applies/restores through the same
   snapshot-backed endpoints as the Tweaks page. Nothing here is decorative —
   every number comes from /api/tweaks, /api/state or /api/diagnostics. */
const CENTERS = {
  inputlag: {
    ico: "⏱", label: "Input Lag",
    title: 'Input <span>Lag</span>',
    sub: "The delay between your mouse moving and the pixel changing. Timer resolution, interrupt handling, USB/PCIe power states and the network stack all sit on that path — every switch below is on it too.",
    live: ["dpc", "jitter"],
    ids: ["timer_high", "win32_priority", "mouse_precision", "menu_delay_0", "usb_powersave", "pcie_aspm",
          "msi_mode", "interrupt_affinity", "bcdedit_tsc", "hpets_off", "network_gaming", "tcp_congestion",
          "dns_cache_big", "nic_powersave"],
  },
  render: {
    ico: "▲", label: "Rendering & FPS",
    title: 'Rendering <span>&amp; FPS</span>',
    sub: "How Windows hands frames to your GPU: scheduling, overlays, fullscreen flips and per-app preferences. These are the documented graphics paths — no injection, no overlay hooks.",
    live: ["hags", "driver"],
    ids: ["hags_on", "mpo_off", "fso_on", "windowed_games", "vrr", "auto_hdr_off", "gpu_preference",
          "game_dvr_off", "game_bar_off", "game_mode"],
  },
  background: {
    ico: "▤", label: "Background load",
    title: 'Background <span>load</span>',
    sub: "What keeps running while you play: Store apps, indexing, Superfetch, Xbox services, telemetry tasks, compositor effects. Each one is a few percent of something — together they add up.",
    live: ["procs"],
    ids: ["background_apps", "sysmain_off", "search_index", "xbox_live_off", "xbox_presence", "bloat_uninstall",
          "onedrive_off", "edge_bing_blocking", "telemetry_tasks", "visual_fx_perf", "visual_fx_balloff",
          "transparency_off", "taskbar_anim", "mouse_trails", "storage_sense"],
  },
  power: {
    ico: "⚡", label: "Power & thermals",
    title: 'Power <span>&amp; thermals</span>',
    sub: "A CPU that downclocks is a CPU that stutters. These settings keep clocks up and links awake; the flip side is heat and idle draw, which is why laptops get a note instead of a blind recommendation.",
    live: ["plan", "pcie"],
    ids: ["power_ultimate", "hpets_off", "pcie_aspm", "usb_powersave", "shutdown_fast", "sysmain_off", "search_index"],
  },
  debloat: {
    ico: "🧹", label: "Debloat & boot",
    title: 'Debloat <span>&amp; boot</span>',
    sub: "Fewer things installed, fewer things starting, less disk churn: Store bloat, OneDrive, telemetry tasks, Storage Sense, boot logo. Reversible app by app — read the description before you tick.",
    live: ["junk", "procs"],
    ids: ["bloat_uninstall", "onedrive_off", "hpets_boot", "edge_bing_blocking", "telemetry_tasks", "storage_sense",
          "sysmain_off", "search_index", "shutdown_fast", "telemetry_off"],
  },
};
let centerLive = null;        // cheap ambient data (state / monitor / storage) — ~0.15 s
let latencySample = null;     // /api/diagnostics — ~20 s of real measuring, only ever on demand

async function centerLiveData() {
  if (centerLive && Date.now() - centerLive.t < 30000) return centerLive;
  const out = { t: Date.now(), state: null, storage: null, mon: null };
  await Promise.all([
    api("/api/state").then((s) => (out.state = s)).catch(() => {}),
    api("/api/storage").then((s) => (out.storage = s)).catch(() => {}),
    api("/api/monitor").then((s) => (out.mon = s)).catch(() => {}),
  ]);
  centerLive = out;
  return out;
}

// DPC / ISR and jitter need a real sample (DPC counter + 20 pings ≈ 20 s), so it is
// never fired behind the user's back: the Input Lag center has an explicit button.
async function measureLatency() {
  overlay.show("Measuring latency", "kernel DPC + ISR sample and 20 real pings — about 20 seconds, nothing is written");
  overlay.step("sampling deferred procedure calls"); overlay.progress(25);
  try {
    latencySample = await api("/api/diagnostics");
    overlay.progress(90);
    const d = latencySample;
    overlay.step(d.dpc && d.dpc.available ? `✓ DPC ${d.dpc.dpcPercent}% / ISR ${d.dpc.isrPercent}%` : "✖ DPC sample unavailable", d.dpc && d.dpc.available ? "ok" : "er");
    if (d.network && d.network.available) overlay.step(`✓ jitter ${d.network.jitterMs} ms · avg ${d.network.avgMs} ms · loss ${d.network.lossPercent}%`, "ok");
    await overlay.done(true, "measured on this machine, right now");
  } catch (e) { await overlay.done(false, String(e)); }
  renderCenter("inputlag");
}

function centerMetrics(key, live) {
  const d = latencySample || {}, st = live.state || {};
  const cards = {
    dpc: d.dpc && d.dpc.available
      ? { k: "DPC / ISR", v: d.dpc.dpcPercent + "% / " + d.dpc.isrPercent + "%", n: d.dpc.verdict }
      : { k: "DPC / ISR", v: "not measured", n: "press Measure in this center" },
    jitter: d.network && d.network.available
      ? { k: "Network jitter", v: d.network.jitterMs + " ms", n: "avg " + d.network.avgMs + " ms · loss " + d.network.lossPercent + "%" }
      : { k: "Network jitter", v: "not measured", n: "press Measure in this center" },
    hags: { k: "GPU scheduling", v: st.hags ? "HAGS on" : "HAGS off", n: st.gpu || "" },
    driver: { k: "GPU driver", v: st.gpuDriver || "—", n: st.gpu || "" },
    plan: { k: "Power plan", v: st.plan || "—", n: st.admin ? "administrator" : "standard user" },
    pcie: { k: "PCIe / USB power", v: tweaksCache.find((t) => t.id === "pcie_aspm")?.applied ? "full speed" : "link may sleep", n: "PCIe ASPM + USB selective suspend" },
    procs: { k: "Processes", v: live.mon ? String(live.mon.procs) : "—", n: live.mon ? live.mon.threads + " threads running" : "live count" },
    junk: { k: "Junk on disk", v: live.storage ? fmtB(live.storage.junkBytes || 0) : "—", n: "temp, caches, recycle bin" },
  };
  return (CENTERS[key].live || []).map((id) => cards[id]).filter(Boolean);
}

function twRowHTML(t, i) {
  return `
    <div class="tw2 ${t.applied ? "is-on" : ""}" data-id="${esc(t.id)}" style="animation-delay:${Math.min(i * 22, 400)}ms">
      <div class="t-main" style="flex:1">
        <div class="tw2-top"><b>${esc(t.name)}</b>
          <span class="badge ${t.admin ? "badge-admin" : "badge-user"}">${t.admin ? "ADMIN" : "USER"}</span>
          <span class="badge ${t.applied ? "badge-on" : ""}">${t.applied ? "APPLIED" : "DEFAULT"}</span>
        </div>
        <div class="tw2-desc">${esc(t.desc)}</div>
        <div class="tw2-foot"><span class="tw2-impact">impact <b>${"▮".repeat(t.impact || 1)}</b></span></div>
      </div>
      <div class="tw2-side">
        <div class="ok-switch ${t.applied ? "on" : ""}" data-id="${esc(t.id)}" title="${t.applied ? "Click to restore Windows default" : "Click to apply"}"></div>
        <span class="tw2-impact">${t.applied ? "ON" : "OFF"}</span>
      </div>
    </div>`;
}
function wireSwitches(root) {
  $$(root + " .tw2").forEach((row) => row.addEventListener("click", (e) => {
    if (e.target.closest(".ok-switch")) return;
    row.querySelector(".ok-switch")?.click();
  }));
  $$(root + " .ok-switch").forEach((sw) => sw.addEventListener("click", (e) => { e.stopPropagation(); toggleTweak(sw); }));
}

async function renderCenter(key) {
  const c = CENTERS[key]; if (!c) return;
  if (!tweaksCache.length) await updateTweakState(true);
  const items = c.ids.map((id) => tweaksCache.find((t) => t.id === id)).filter(Boolean);
  const onW = items.filter((t) => t.applied).reduce((a, t) => a + (t.impact || 1), 0);
  const totW = items.reduce((a, t) => a + (t.impact || 1), 0) || 1;
  const pct = Math.round(onW / totW * 100);
  const applied = items.filter((t) => t.applied).length;
  const needAdmin = items.filter((t) => !t.applied && t.admin).length;
  const ring = $(`#view-${key} .gc-ring`);
  if (ring) { ring.style.setProperty("--p", pct); ring.style.background = `conic-gradient(var(--accent) calc(var(--p)*1%),rgba(255,255,255,.07) 0)`; }
  $(`#${key}-num`).textContent = pct + "%";
  $(`#${key}-summary`).innerHTML =
    `<b>${applied} of ${items.length} switches active</b> in this center — ` +
    `${pct >= 80 ? "this area is tuned." : pct >= 40 ? "half way there." : "stock Windows here."}` +
    `<br/><span class="muted" style="font-size:11.5px">${needAdmin} of the remaining items require elevation · ${c.ids.length} curated settings in this module</span>`;
  // catalog + switches first: the live measurement below can take ~2 s and must never block the page
  $(`#${key}-stats`).innerHTML = `
    <div class="stat-card good"><b>${applied}</b><span>active</span></div>
    <div class="stat-card med"><b>${items.length - applied}</b><span>still pending</span></div>
    <div class="stat-card ${needAdmin ? "high" : "good"}"><b>${needAdmin}</b><span>need admin</span></div>`;
  const list = $(`#${key}-list`);
  list.innerHTML = items.map((t, i) => twRowHTML(t, i)).join("") || `<div class="conv-empty muted" style="padding:14px">nothing in this center</div>`;
  wireSwitches(`#${key}-list`);
  const bA = $(`#${key}-apply`), bR = $(`#${key}-restore`);
  const safeN = items.filter((t) => !t.applied && !t.admin).length;
  if (bA) bA.textContent = `✔ Apply the ${safeN} user-safe switch${safeN === 1 ? "" : "es"}`;
  if (bA) bA.disabled = safeN === 0;
  if (bR) bR.disabled = applied === 0;
  const live = await centerLiveData();
  if (currentView !== key) return;   // user moved on while we measured
  const stat = $(`#${key}-stats`);
  if (stat) stat.insertAdjacentHTML("beforeend", centerMetrics(key, live).map((m) =>
    `<div class="stat-card live"><b style="font-size:15px">${esc(m.v)}</b><span>${esc(m.k)}</span></div>`).join(""));
}

async function centerBulk(key, mode) {
  const c = CENTERS[key]; if (!c) return;
  const items = c.ids.map((id) => tweaksCache.find((t) => t.id === id)).filter(Boolean);
  const ids = mode === "apply" ? items.filter((t) => !t.applied && !t.admin).map((t) => t.id) : items.filter((t) => t.applied).map((t) => t.id);
  if (!ids.length) { toast("nothing to do in this center"); return; }
  overlay.show((mode === "apply" ? "Applying · " : "Restoring · ") + c.label,
    `${ids.length} settings — snapshot first, one by one reversible`);
  overlay.step((mode === "apply" ? "apply " : "restore ") + ids.length + " settings"); overlay.progress(30);
  try {
    const r = mode === "apply" ? await api("/api/tweaks/apply", ids) : await api("/api/tweaks/restore", ids);
    const n = mode === "apply" ? r.applied : r.restored;
    (r.errors || []).slice(0, 5).forEach((x) => overlay.step("✖ " + x, "er"));
    overlay.progress(95);
    overlay.step(`✓ ${n} ${mode === "apply" ? "applied" : "restored"}`, n ? "ok" : "er");
    await overlay.done(n > 0, `${n}/${ids.length} · admin items are left alone on purpose`);
  } catch (e) { await overlay.done(false, String(e)); }
  await updateTweakState(true);
  centerLive = null;
  renderCenter(key);
}

// one delegated handler for every center hero button (#<key>-apply | -restore | -refresh)
on("#inputlag-measure", measureLatency);
document.addEventListener("click", (e) => {
  const b = e.target.closest("button[id$='-apply'],button[id$='-restore'],button[id$='-refresh']");
  if (!b) return;
  const m = b.id.match(/^([a-z]+)-(apply|restore|refresh)$/); if (!m) return;
  const key = m[1]; if (!CENTERS[key]) return;
  if (m[2] === "refresh") { centerLive = null; toast("↻ re-analyzing " + CENTERS[key].label); updateTweakState(true).then(() => renderCenter(key)); }
  else centerBulk(key, m[2]);
});

/* ============ GAME LIBRARY (cover art + the built-in game database) ============ */
let libGames = null, libFilter = "all", libQuery = "", libSort = "az", libInstalledOnly = false;
// readable labels for the suggested-set ids stored in the library manifest
const PACK_LABEL = {
  esport: "Esport", lowlatency: "Low latency", streaming: "Play & stream",
  laptop: "Laptop / thermals", cleanboot: "Clean boot", privacy: "Privacy",
};
const FAMILY_LABEL = {
  fps: "Competitive FPS", br: "Battle royale", moba: "MOBA", mmo: "MMO / live service",
  coop: "Co-op / PvE", rpg: "Single-player RPG", openworld: "Open world", racing: "Racing & sim",
  fighting: "Fighting & arena", sports: "Sports", horror: "Horror", sandbox: "Sandbox", party: "Party",
  strategy: "Strategy", sim: "Sim & management", survival: "Survival", roguelike: "Roguelike",
};
// The cover library (manifest) plus every title the exe knows about: the database is the
// source of truth for names + genres, so a game only needs art to get a pretty tile.
// Steam most-played entries carry g.rank (1..100) and g.peak — surfaced as a badge.
async function ensureLibrary(force) {
  if (libGames && !force) return libGames;
  const out = [], seen = new Set();
  try {
    const m = await api("/assets/gamelogos/manifest.json");
    for (const g of (m && m.games) || []) { out.push(g); seen.add(normKey(g.name)); }
  } catch (e) { }
  try {
    const db = await api("/api/games/catalog");
    for (const r of db || []) {
      const k = normKey(r.name);
      if (!k || seen.has(k)) continue;
      seen.add(k);
      out.push({ key: r.exe, name: r.name, family: r.family, pack: FAMILY_PACK[r.family] || "esport", cover: "" });
    }
  } catch (e) { }
  out.sort((a, b) => (a.rank || 999) - (b.rank || 999) || a.name.localeCompare(b.name));
  libGames = out;
  return libGames;
}
function libDetectedMap() {
  const m = new Map();
  for (const g of gamesCache)
    for (const k of [normKey(g.name), normKey(g.matched)])
      if (k.length > 3) m.set(k, g);
  return m;
}
function libIsInstalled(g, det) {
  const k = normKey(g.name);
  if (!k) return null;
  if (det.has(k)) return det.get(k);
  for (const [dk, dg] of det)
    if (dk.length > 5 && k.length > 5 && (dk.includes(k) || k.includes(dk))) return dg;
  return null;
}
async function renderLibrary(force) {
  const grid = $("#lib-grid"); if (!grid) return;
  if (libGames === null || force) {
    grid.innerHTML = `<div class="conv-empty muted" style="padding:14px">loading cover art + the built-in game database…</div>`;
    await ensureLibrary(true);
  }
  if (!libGames.length) {
    grid.innerHTML = `<div class="conv-empty muted" style="padding:14px">Library unavailable — the built-in database ships inside the exe (<span class="mono">/api/games/catalog</span>) and the cover art lives in <span class="mono">web/assets/gamelogos</span>.</div>`;
    return;
  }
  const det = libDetectedMap();
  const all = libGames.map((g) => ({ ...g, _inst: libIsInstalled(g, det) }));
  const instN = all.filter((g) => g._inst).length;
  const covN = all.filter((g) => g.cover).length;
  const fams = {};
  for (const g of all) fams[g.family] = (fams[g.family] || 0) + 1;
  $("#lib-families").innerHTML =
    `<span class="chip ${libFilter === "all" ? "on" : ""}" data-fam="all">All · ${all.length}</span>` +
    `<span class="chip ${libFilter === "_installed" ? "on" : ""}" data-fam="_installed">🖥 Installed · ${instN}</span>` +
    `<span class="chip ${libFilter === "_covers" ? "on" : ""}" data-fam="_covers">🖼 Cover art · ${covN}</span>` +
    Object.keys(fams).sort((a, b) => fams[b] - fams[a]).map((f) =>
      `<span class="chip ${libFilter === f ? "on" : ""}" data-fam="${f}">${FAMILY_LABEL[f] || f} · ${fams[f]}</span>`).join("");
  const q = libQuery.trim().toLowerCase();
  const list = all.filter((g) => {
    const okFam = libFilter === "all" || g.family === libFilter ||
      (libFilter === "_installed" && !!g._inst) || (libFilter === "_covers" && !!g.cover);
    const okInst = !libInstalledOnly || !!g._inst;
    const okQ = !q || (g.name + " " + g.family + " " + (FAMILY_LABEL[g.family] || "") + " " + g.pack).toLowerCase().includes(q);
    return okFam && okInst && okQ;
  });
  const byName = (a, b) => a.name.localeCompare(b.name);
  const byInst = (a, b) => (b._inst ? 1 : 0) - (a._inst ? 1 : 0) || byName(a, b);
  if (libSort === "za") list.sort((a, b) => -byName(a, b));
  else if (libSort === "genre") list.sort((a, b) => (a.family || "").localeCompare(b.family || "") || byInst(a, b));
  else if (libSort === "installed") list.sort(byInst);
  else if (libSort === "covers") list.sort((a, b) => (b.cover ? 1 : 0) - (a.cover ? 1 : 0) || byInst(a, b));
  else if (libSort === "top") list.sort((a, b) => (a.rank || 999) - (b.rank || 999) || (b.cover ? 1 : 0) - (a.cover ? 1 : 0) || byName(a, b));
  else list.sort((a, b) => (a.rank || 999) - (b.rank || 999) || byName(a, b));
  grid.innerHTML = list.map((g, i) => `
    <div class="lib-card ${g._inst ? "inst" : ""}" data-key="${esc(g.key)}" style="animation-delay:${Math.min(i * 8, 320)}ms">
      ${g.cover
      ? `<img loading="lazy" src="/assets/gamelogos/${esc(g.cover)}" alt="${esc(g.name)}"/>`
      : `<div class="lib-ph"><b>${esc(initials(g.name))}</b><span>${esc(FAMILY_LABEL[g.family] || g.family || "PC")}</span></div>`}
      ${g._inst && g._inst.icon ? `<img class="lib-inst-ico" src="/api/game-icon/${encodeURIComponent(g._inst.icon)}" alt=""/>` : ""}
      ${g._inst ? `<span class="lib-badge inst">INSTALLED</span>` : ""}
      ${g.rank ? `<span class="lib-badge rank" title="Steam most-played chart — ${esc(String(g.peak || ""))} concurrent">#${g.rank}</span>` : ""}
      <div class="lib-meta"><b>${esc(g.name)}</b><span>${esc(FAMILY_LABEL[g.family] || g.family)} · ${esc(PACK_LABEL[g.pack] || g.pack)}</span></div>
    </div>`).join("");
  $("#lib-count").textContent = `${list.length} of ${all.length} titles · ${instN} installed here`;
  const desc = $("#lib-desc");
  if (desc) desc.innerHTML = `<b>${all.length} titles</b> — ${covN} with cover art, the rest from the built-in database. <b>${instN}</b> are installed on this machine and show their real icon. Pick one and OptimizeKit prepares the set that fits its genre: you see the exact tweak list before anything is written, and one click puts it all back.`;
  $$("#lib-grid .lib-card").forEach((c) => c.addEventListener("click", () => selectLibGame(c.dataset.key)));
  // first visit: learn what is installed so the INSTALLED badges can appear (no icon pass here)
  if (!gamesCache.length && !window.__libAutoScan) {
    window.__libAutoScan = true;
    const st = $("#lib-scan-state");
    if (st) st.textContent = "looking for installed titles…";
    api("/api/games").then((g) => {
      gamesCache = g || [];
      if (st) st.textContent = `${gamesCache.length} games on this PC`;
      renderLibrary();
    }).catch(() => { if (st) st.textContent = ""; });
  }
}
function selectLibGame(key) {
  const g = (libGames || []).find((x) => x.key === key); if (!g) return;
  const pack = PACKS.find((p) => p.id === g.pack) || PACKS[0];
  $$("#lib-grid .lib-card").forEach((c) => c.classList.toggle("sel", c.dataset.key === key));
  const el = $("#lib-detail");
  el.classList.remove("hidden");
  const known = pack.ids.filter((id) => tweaksCache.some((t) => t.id === id));
  const on = known.filter((id) => tweaksCache.find((t) => t.id === id)?.applied).length;
  const inst = libIsInstalled(g, libDetectedMap());
  el.innerHTML = `
    <div class="lib-hero">
      ${g.cover
      ? `<img src="/assets/gamelogos/${esc(g.cover)}" alt=""/>`
      : `<div class="lib-hero-ph"><b>${esc(initials(g.name))}</b><span>${esc(FAMILY_LABEL[g.family] || g.family || "PC")}</span></div>`}
      <div>
        <h2>${esc(g.name)}${inst ? ` <span class="lib-badge inst" style="position:static">INSTALLED</span>` : ""}</h2>
        <p class="muted" style="font-size:12px">${esc(FAMILY_LABEL[g.family] || g.family)} · suggested set <b style="color:var(--accent)">${esc(pack.name)}</b> — ${esc(pack.desc)}${inst ? `<br/>found on this PC as <b>${esc(inst.name)}</b> (${esc(inst.launcher)})${inst.icon ? " — real icon loaded" : ""}` : ""}</p>
        <div class="lib-progress"><i style="width:${known.length ? Math.round(on / known.length * 100) : 0}%"></i></div>
        <span class="muted" style="font-size:11px">${on}/${known.length} of this set already active</span>
        <div class="g-actions">
          <button class="btn primary" id="lib-apply">⚡ Prepare this game</button>
          ${inst ? `<button class="btn" id="lib-launch">▶ Launch</button>` : ""}
          ${inst ? `<button class="btn" id="lib-folder">📂 Open folder</button>` : ""}
          <button class="btn" id="lib-inside">What's inside</button>
          <button class="btn" id="lib-pick">🎲 Surprise me</button>
          <button class="btn danger" id="lib-restore">↺ Restore</button>
          <button class="btn" id="lib-games">Detected on this PC →</button>
        </div>
      </div>
    </div>`;
  $("#lib-games").addEventListener("click", () => show("games"));
  $("#lib-pick").addEventListener("click", () => {
    const pool = (libGames || []); if (!pool.length) return;
    selectLibGame(pool[Math.floor(Math.random() * pool.length)].key);
  });
  if (inst) {
    $("#lib-launch").addEventListener("click", async () => {
      const r = await api("/api/games/launch", { id: inst.id });
      toast(r.ok ? "▶ launching " + inst.name : "✖ " + r.error);
    });
    $("#lib-folder").addEventListener("click", async () => {
      const r = await api("/api/games/launch", { id: inst.id, reveal: true });
      toast(r.ok ? "📂 folder opened" : "✖ " + r.error);
    });
  }
  $("#lib-inside").addEventListener("click", () => { show("packs"); setTimeout(() => showPack(pack), 120); });
  $("#lib-restore").addEventListener("click", async (e) => { e.target.disabled = true; await api("/api/tweaks/restore", pack.ids); await updateTweakState(true); selectLibGame(key); toast("↺ " + pack.name + " restored"); });
  $("#lib-apply").addEventListener("click", async (e) => {
    e.target.disabled = true; e.target.textContent = "applying…";
    overlay.show("Preparing " + g.name, pack.name + " — snapshot first, reversible in one click");
    overlay.step("apply " + pack.ids.length + " settings"); overlay.progress(40);
    try {
      const r = await api("/api/tweaks/apply", pack.ids);
      overlay.progress(95);
      (r.errors || []).slice(0, 4).forEach((x) => overlay.step("✖ " + x, "er"));
      overlay.step(`✓ ${r.applied} applied`, r.applied ? "ok" : "er");
      await overlay.done(r.applied > 0, `${r.applied}/${pack.ids.length} applied${(r.errors || []).length ? " — some need admin" : ""}`);
    } catch (err) { await overlay.done(false, String(err)); }
    await updateTweakState(true);
    selectLibGame(key);
  });
}
$("#lib-search").addEventListener("input", (e) => { libQuery = e.target.value; renderLibrary(); });
if ($("#lib-sort")) $("#lib-sort").addEventListener("change", (e) => { libSort = e.target.value; renderLibrary(); });
$("#lib-families").addEventListener("click", (e) => {
  const chip = e.target.closest(".chip"); if (!chip) return;
  libFilter = chip.dataset.fam; renderLibrary();
});
on("#lib-random", () => {
  const cards = $$("#lib-grid .lib-card");
  if (!cards.length) return toast("▦ nothing to pick — clear the filters");
  selectLibGame(cards[Math.floor(Math.random() * cards.length)].dataset.key);
});
on("#lib-toggle-installed", (e) => {
  libInstalledOnly = !libInstalledOnly;
  e.currentTarget.classList.toggle("on", libInstalledOnly);
  e.currentTarget.textContent = libInstalledOnly ? "▲ Installed only: ON" : "▲ Installed only";
  renderLibrary();
});
on("#lib-clear", () => {
  libFilter = "all"; libQuery = ""; libInstalledOnly = false;
  if ($("#lib-search")) $("#lib-search").value = "";
  const b = $("#lib-toggle-installed"); if (b) { b.classList.remove("on"); b.textContent = "▲ Installed only"; }
  renderLibrary();
});
on("#lib-scan", async () => {
  const st = $("#lib-scan-state");
  if (st) st.textContent = "scanning every store + every drive…";
  await loadGames();
  renderLibrary();
  if (st) st.textContent = `${gamesCache.length} games on this PC · ${gamesCache.filter((g) => g.icon).length} icons`;
});
on("#btn-lib-icons", async () => {
  const st = $("#lib-scan-state");
  if (st) st.textContent = "extracting icons…";
  const r = await api("/api/games/icons", { all: true });
  gamesCache = await api("/api/games");
  renderLibrary();
  if (st) st.textContent = `${r.extracted || 0} new icons`;
  toast("🖼 " + (r.extracted || 0) + " new icons", 3200);
});

/* ===================== quick BOOST (topbar) ===================== */
async function quickBoost(btn) {
  const active = btn.classList.contains("on");
  if (active) {
    overlay.show("Leaving BOOST", "restoring the Windows defaults for the boost set");
    try {
      const r = await api("/api/tweaks/restore", ESPORT_IDS);
      await overlay.done(true, r.restored + " restored");
      toast("↺ boost off");
    } catch (e) { await overlay.done(false, String(e)); }
  } else {
    overlay.show("BOOST", "the competitive set in one click — everything is remembered");
    overlay.step("apply " + ESPORT_IDS.length + " settings"); overlay.progress(35);
    try {
      const r = await api("/api/tweaks/apply", ESPORT_IDS);
      overlay.progress(95);
      overlay.step(`✓ ${r.applied} applied${(r.errors || []).length ? " · " + r.errors.length + " need admin" : ""}`, "ok");
      await overlay.done(true, r.applied + " applied");
      toast("⚡ BOOST ON — " + r.applied + " settings", 3600);
    } catch (e) { await overlay.done(false, String(e)); }
  }
  await updateTweakState(true);
  btn.classList.toggle("on", !active);
}

/* ===================== APPEARANCE (themes gallery) ===================== */
const ACCENT_SWATCHES = [
  "#ff3d57", "#ff3b4e", "#ffb04d", "#5865f2", "#ff37c7", "#7cff3d",
  "#a78bfa", "#38bdf8", "#22c55e", "#f59e0b", "#94a3b8", "#f472b6",
];
function applyVisualPrefs(p, glitch) {
  document.body.classList.toggle("no-particles", p === false);
  document.body.classList.toggle("glitch", glitch === true);
  const a = $("#set-anim"), g = $("#set-glitch");
  if (a) a.checked = p !== false;
  if (g) g.checked = glitch === true;
  const tp = $("#theme-particles"), tg = $("#theme-glitch");
  if (tp) tp.checked = p !== false;
  if (tg) tg.checked = glitch === true;
}
async function renderThemes() {
  const g = $("#theme-gallery"); if (!g) return;
  g.innerHTML = THEME_PACKS.map(([id, hex, desc]) => `
    <div class="theme-card ${currentTheme === id ? "on" : ""}" data-pack="${id}" style="--c:${hex}">
      <div class="tc-preview"><i class="tc-bar"></i><i class="tc-dot"></i><i class="tc-line"></i><i class="tc-line s"></i></div>
      <div class="tc-meta"><b>${id}</b><span>${esc(desc)}</span></div>
    </div>`).join("");
  $$("#theme-gallery .theme-card").forEach((c) => c.addEventListener("click", () => setThemePack(c.dataset.pack, true)));
  $("#accent-swatches").innerHTML = ACCENT_SWATCHES.map((h) =>
    `<i class="swatch ${currentAccent === h ? "on" : ""}" data-accent="${h}" style="--c:${h}" title="${h}"></i>`).join("");
  $$("#accent-swatches .swatch").forEach((s) => s.addEventListener("click", () => {
    applyAccent(s.dataset.accent);                    // apply first: applyThemePack would reset it
    rememberAccent(currentTheme, s.dataset.accent);
    api("/api/settings", { ui_accent: s.dataset.accent }).catch(() => {});
    renderThemes();
  }));
  const ca = $("#theme-accent");
  if (ca) ca.value = currentAccent || "#ff3d57";
  $("#theme-name").textContent = `${currentTheme} · accent ${currentAccent}`;
  try {
    const s = await api("/api/settings");
    applyVisualPrefs(s.ui_particles, s.ui_glitch_text);
  } catch (e) { }
}
$("#theme-accent").addEventListener("input", (e) => { applyAccent(e.target.value, false); rememberAccent(currentTheme, e.target.value); });
on("#btn-theme-save", async () => {
  await api("/api/settings", {
    ui_theme: currentTheme, ui_accent: currentAccent,
    ui_particles: $("#theme-particles").checked, ui_glitch_text: $("#theme-glitch").checked,
  }).catch(() => {});
  localStorage.setItem("ok_theme", currentTheme);
  rememberAccent(currentTheme, currentAccent);
  applyVisualPrefs($("#theme-particles").checked, $("#theme-glitch").checked);
  toast("✔ Appearance saved");
});

/* ===================== COMMAND PALETTE (Ctrl+K) ===================== */
let palItems = [], palSel = 0, palOpenNow = false;
function palSources() {
  const out = [];
  $$(".nav-item[data-view]").forEach((n) => {
    const label = (n.querySelector("span:nth-child(2)")?.textContent || "").trim();
    const sec = n.previousElementSibling?.classList.contains("nav-sep") ? n.previousElementSibling.textContent : "module";
    out.push({ g: "module", label, hint: sec.toLowerCase(), run: () => show(n.dataset.view) });
  });
  for (const t of tweaksCache) out.push({
    g: "tweak", label: t.name, hint: t.applied ? "ON · Enter restores the Windows default" : "OFF · Enter applies it",
    run: async () => {
      const r = t.applied ? await api("/api/tweaks/restore", [t.id]) : await api("/api/tweaks/apply", [t.id]);
      const n = t.applied ? r.restored : r.applied;
      toast(n > 0 ? (t.applied ? "↺ " : "✔ ") + t.name : "✖ " + ((r.errors || [])[0] || "no change"), 3400);
      await updateTweakState(true);
    },
  });
  for (const [, id, n, d] of TOOLS) out.push({
    g: "tool", label: n, hint: "tool · " + d,
    run: () => { api("/api/tools", { tool: id }); toast("Opening " + n + "…"); },
  });
  for (const p of PACKS) out.push({
    g: "pack", label: p.name, hint: "pack · " + p.ids.length + " tweaks",
    run: () => { show("packs"); setTimeout(() => showPack(p), 160); },
  });
  for (const c of Object.keys(CENTERS)) out.push({
    g: "center", label: CENTERS[c].label, hint: "tuning center", run: () => show(c),
  });
  for (const g of (libGames || [])) out.push({
    g: "game", label: g.name, hint: "game · " + (FAMILY_LABEL[g.family] || g.family),
    run: () => { show("library"); setTimeout(() => selectLibGame(g.key), 180); },
  });
  return out;
}
function palRender(q) {
  const terms = q.toLowerCase().split(/\s+/).filter(Boolean);
  const hits = palItems.filter((it) => {
    const hay = (it.label + " " + it.hint + " " + it.g).toLowerCase();
    return terms.every((t) => hay.includes(t));
  });
  const ORDER = { module: 0, center: 1, pack: 2, tweak: 3, game: 4, tool: 5 };
  hits.sort((a, b) => (ORDER[a.g] - ORDER[b.g]) || a.label.localeCompare(b.label));
  const shown = hits.slice(0, 60);
  palSel = Math.min(palSel, Math.max(shown.length - 1, 0));
  const box = $("#pal-results");
  box.innerHTML = shown.map((it, i) => `
    <div class="pal-row ${i === palSel ? "sel" : ""}" data-i="${i}">
      <span class="pal-kind k-${it.g}">${it.g}</span>
      <b>${esc(it.label)}</b><span class="pal-hint">${esc(it.hint)}</span>
      ${i === palSel ? '<kbd class="pal-enter">↵</kbd>' : ""}
    </div>`).join("") || `<div class="pal-empty muted">nothing matches “${esc(q)}”</div>`;
  $("#pal-count").textContent = shown.length + " result" + (shown.length === 1 ? "" : "s");
  const sel = box.querySelector(".pal-row.sel"); if (sel) sel.scrollIntoView({ block: "nearest" });
  return shown;
}
async function palOpen() {
  const pal = $("#palette");
  pal.classList.remove("hidden");
  palOpenNow = true;
  const inp = $("#pal-input");
  inp.value = ""; inp.focus(); palSel = 0;
  palItems = palSources();
  palRender("");
  if (libGames === null) {           // the game library joins the palette once it is loaded
    await ensureLibrary();
    palItems = palSources(); palRender(inp.value);
  }
}
function palClose() {
  $("#palette").classList.add("hidden");
  palOpenNow = false;
  $$("#theme-flyout").forEach((f) => f.classList.add("hidden"));
}
function palRun(i) {
  const terms = ($("#pal-input").value || "").toLowerCase().split(/\s+/).filter(Boolean);
  const hits = palItems.filter((it) => terms.every((t) => (it.label + " " + it.hint + " " + it.g).toLowerCase().includes(t)));
  const ORDER = { module: 0, center: 1, pack: 2, tweak: 3, game: 4, tool: 5 };
  hits.sort((a, b) => (ORDER[a.g] - ORDER[b.g]) || a.label.localeCompare(b.label));
  const it = hits[i];
  palClose();
  if (it) it.run();
}
on("#btn-palette", palOpen);
on("#cmdbar", palOpen);
$("#pal-input").addEventListener("input", (e) => { palSel = 0; palRender(e.target.value); });
$("#pal-results").addEventListener("click", (e) => {
  const row = e.target.closest(".pal-row"); if (row) palRun(+row.dataset.i);
});
$("#palette").addEventListener("click", (e) => { if (e.target.id === "palette") palClose(); });
document.addEventListener("keydown", (e) => {
  const typing = e.target.closest("input,textarea,select");
  if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "k") { e.preventDefault(); palOpen(); return; }
  if (!palOpenNow) { if (e.key === "Escape" && typing) e.target.blur(); return; }
  if (e.key === "Escape") { e.preventDefault(); palClose(); }
  else if (e.key === "ArrowDown") { e.preventDefault(); palSel++; palRender($("#pal-input").value); }
  else if (e.key === "ArrowUp") { e.preventDefault(); palSel = Math.max(0, palSel - 1); palRender($("#pal-input").value); }
  else if (e.key === "Enter") { e.preventDefault(); palRun(palSel); }
});

/* ===================== dashboard KPI tiles ===================== */
let kpiJunk = null;
async function refreshKpis(mon) {
  const box = $("#dash-kpis"); if (!box) return;
  if (kpiJunk === null) {
    try { const s = await api("/api/storage"); kpiJunk = s.junkBytes || 0; } catch (e) { kpiJunk = 0; }
  }
  const active = tweaksCache.filter((t) => t.applied).length;
  const pct = tweaksCache.length ? Math.round(active / tweaksCache.length * 100) : 0;
  const v = [
    { b: active + "/" + tweaksCache.length, s: "tweaks active", cls: pct >= 60 ? "good" : pct > 0 ? "med" : "" },
    { b: fmtB(kpiJunk || 0), s: "junk on disk", cls: (kpiJunk || 0) > 2 * 1024 * 1024 * 1024 ? "med" : "good" },
    { b: mon ? String(mon.procs) : "—", s: mon ? mon.threads + " threads" : "processes", cls: "" },
    { b: mon ? fmtUptime(mon.uptimeSec) : "—", s: "uptime", cls: "" },
  ];
  box.innerHTML = v.map((k) => `<div class="kpi ${k.cls}"><b>${esc(k.b)}</b><span>${esc(k.s)}</span></div>`).join("");
}

/* ===================== BIOS GUIDE ===================== */
/* ===================== FIRMWARE (live platform state) =====================
   Read-only bridge to /api/firmware: SecureBoot, TPM, VT, BIOS identity, S0/S3,
   HPET / WPBT / dynamic tick as the kernel actually sees them. The guide below
   stays a guide - the app never writes firmware. */
let fwCache = null;
const fwBadge = (val, good, warn) => val === good ? "good" : val && val.includes("off") ? "warn" : (warn || "");
async function refreshFirmware(force) {
  if (fwCache && !force) return renderFirmware();
  $("#fw-cards").innerHTML = `<div class="stat-card"><b>…</b><span>reading the platform</span></div>`;
  try {
    fwCache = await api("/api/firmware");
    renderFirmware();
    const navBadge = $("#nav-fw-badge");
    if (navBadge) { navBadge.textContent = ""; navBadge.classList.remove("show"); }
  } catch (e) {
    $("#fw-cards").innerHTML = `<div class="stat-card warn"><b>n/a</b><span>platform read failed</span></div>`;
  }
}
function fwCard(title, value, cls) {
  return `<div class="stat-card ${cls || ""}"><b>${esc(value)}</b><span>${esc(title)}</span></div>`;
}
function renderFirmware() {
  const f = fwCache; if (!f) return;
  const tpm = f.tpm || {};
  const sb = f.secureBoot || "unknown";
  const vt = f.virtualization || "unknown";
  const tpmS = tpm.present ? (tpm.version || "present") + (tpm.activated === false ? " (inactive)" : "") : "absent";
  $("#fw-cards").innerHTML = [
    fwCard("BIOS", (f.biosVendor || "?") + " " + (f.biosVersion || "")),
    fwCard("Motherboard", f.motherboard || "?"),
    fwCard("Boot mode", f.bootMode || "?", sb === "on" ? "good" : "warn"),
    fwCard("Secure Boot", sb, sb === "on" ? "good" : sb === "off" ? "bad" : "warn"),
    fwCard("TPM", tpmS, tpm.present ? "good" : "bad"),
    fwCard("Virtualization", vt + (f.hypervisorRunning ? " · hv on" : ""), vt === "on" ? "good" : "warn"),
    fwCard("Standby", f.modernStandby || "?", ""),
    fwCard("HPET", f.hpet || "?", f.hpet === "on" ? "good" : "warn"),
    fwCard("WPBT", f.wpbt || "?", String(f.wpbt || "").startsWith("blocked") ? "good" : "warn"),
    fwCard("Dynamic tick", f.dynamicTick || "?", ""),
    fwCard("Kernel dump", f.dumpLevel || "?", ""),
    fwCard("Reboot pending", f.rebootNeeded ? "yes" : "no", f.rebootNeeded ? "warn" : "good"),
  ].join("");
}
on("#btn-fw-refresh", () => refreshFirmware(true));

/* ===================== DRIVERS — auto-update engine =====================
   The app cannot silently install drivers (and should not), so it does what a
   careful human would: report real driver ages from the driver store, list
   devices with a problem code, then trigger the Windows Update orchestrator
   and open the vendor page for the manual confirm. */
let drvCache = null;
async function refreshDriverReport(force) {
  if (drvCache && !force) return renderDriverReport();
  $("#drv-table").innerHTML = `<div class="muted" style="padding:12px">reading the driver store…</div>`;
  try {
    const [rep, probs] = await Promise.all([api("/api/drvupdate/report"), api("/api/drvupdate/problems")]);
    drvCache = { rep, probs };
    renderDriverReport();
  } catch (e) {
    $("#drv-table").innerHTML = `<div class="muted" style="padding:12px">driver report failed</div>`;
  }
}
function renderDriverReport() {
  if (!drvCache) return;
  const { rep, probs } = drvCache;
  const ageCls = (d) => !d || d.ageDays == null ? "" : d.ageDays > 365 ? "bad" : d.ageDays > 180 ? "warn" : "good";
  const row = (d) => {
    if (!d) return "";
    const age = d.ageDays != null ? `${d.ageDays} days · ${d.age}` : "date unknown";
    return `<div class="proc-row"><span class="p-name">${esc(d.name)}</span>
      <span class="p-host mono">v${esc(d.version || "?")}</span>
      <span class="p-ms mono">${esc(d.date || "—")}</span>
      <span class="p-ms mono ${ageCls(d)}">${esc(age)}</span></div>`;
  };
  const gpu = rep.gpu || {};
  $("#st-gpu-h").textContent = "GPU — " + (gpu.name || "unknown");
  $("#st-gpu-d").textContent = `driver ${gpu.version || "?"} · ${gpu.date || "date unknown"}${gpu.ageDays != null ? " · " + gpu.ageDays + " days old" : ""}`;
  const sum = (rep.summary || {});
  $("#drv-sum").textContent = `${sum.devices ?? 0} devices read · ${sum.stale ?? 0} older than a year`;
  $("#drv-table").innerHTML =
    row(gpu) + (rep.net || []).slice(0, 3).map(row).join("") + (rep.audio || []).slice(0, 4).map(row).join("")
    || `<div class="muted" style="padding:12px">no driver metadata found</div>`;
  const pl = probs || [];
  $("#drv-problems").innerHTML = pl.length
    ? pl.map((d) => `<div class="proc-row"><span class="p-name" style="color:var(--err)">[!] ${esc(d.name)}</span><span class="p-host mono">code ${d.problem}</span><span class="p-ms mono muted">${esc(d.hint || "")}</span></div>`).join("")
    : `<div class="muted" style="padding:12px">✓ no device reports a problem code</div>`;
}
on("#btn-drv-report", () => refreshDriverReport(true));
on("#btn-drv-wu", async () => {
  toast("⤓ Windows Update driver scan triggered…", 3600);
  try { await api("/api/drvupdate/scan", { driversOnly: true }); toast("✔ Windows Update is looking for driver updates — open its window to confirm"); }
  catch (e) { toast("✖ scan failed — admin?", 4000); }
});
on("#btn-drv-wuwin", () => api("/api/drvupdate/wu-window", {}));
on("#btn-drv-rescan", async () => {
  const r = await api("/api/drvupdate/rescan", {});
  toast(r.triggered ? "↻ PnP rescan done" : "✖ rescan failed");
});
on("#btn-dxdiag", () => fetch("/api/open?what=dxdiag"));
on("#btn-devmgmt", () => fetch("/api/open?what=devmgmt"));

const BIOS_ITEMS = [
  ["XMP / EXPO", "Memory profile", "Loads your RAM kit's rated speed (e.g. 6000 MT/s) instead of the JEDEC fallback around 4800. On most builds this is the single biggest free gain — and it is vendor-sanctioned.", "safe"],
  ["Resizable BAR / ReBAR", "PCIe · graphics", "Lets the CPU address the whole GPU VRAM at once. Supported on RTX 30/40, RX 6000/7000 with a modern CPU. Free FPS in many titles.", "safe"],
  ["Above 4G decoding", "PCIe", "Required companion of ReBAR. If ReBAR is greyed out or missing, enable this first, save, and reboot.", "safe"],
  ["C-States / core parking", "CPU power", "Deep C-states save power but add wake latency. On a desktop that never idles in-game, limiting them can smooth frame times.", "advanced"],
  ["PBO / Precision Boost Overdrive", "CPU · AMD", "Vendor-sanctioned automatic boost within safe limits. Keep it on Auto or a mild curve — leave voltages alone.", "advanced"],
  ["Fan curve / Q-Fan", "Thermals", "Ramp earlier so boost clocks hold longer. A curve beats a fixed 100% fan speed: quieter and just as cool.", "safe"],
  ["BIOS / microcode update", "Firmware", "Newer firmware ships AGESA and microcode fixes (ReBAR bugs, memory stability). Flash from the BIOS itself using the vendor's official file only.", "advanced"],
  ["CSM off + Fast Boot", "Boot", "CSM must be off and UEFI on for ReBAR and Secure Boot. Fast Boot skips some POST checks — faster cold boot, at the cost of slower key-to-enter-setup.", "safe"],
  ["Secure Boot", "Security", "Keep it ON. Windows 11 and several anti-cheats require it. Disable only for specific legacy tooling, and re-enable afterwards.", "safe"],
  ["Virtualization (SVM / VT-x)", "Security · VMs", "Needed for VMs, WSL2 and Android emulators. Leave it on unless an older anti-cheat conflicts with it.", "safe"],
  ["Power supply / GPU cables", "Hardware", "Not a menu setting, but a real one: use separate PCIe cables per connector instead of daisy-chaining, and keep the PSU above 500 W headroom.", "safe"],
  ["Driver mode (XMP & memory training)", "Stability", "After enabling XMP/EXPO, run a memory test (MemTest86 or the built-in check) before trusting it. Unstable RAM corrupts files silently.", "safe"],
];
function renderBios() {
  const grid = $("#bios-grid"); if (!grid) return;
  grid.innerHTML = BIOS_ITEMS.map(([name, cat, desc, risk], i) => `
    <div class="bios-card ${risk}" style="animation-delay:${Math.min(i * 30, 320)}ms">
      <div class="bc-top"><b>${esc(name)}</b><span class="badge ${risk === "safe" ? "badge-user" : "badge-admin"}">${risk === "safe" ? "SAFE" : "ADVANCED"}</span></div>
      <small>${esc(cat)}</small>
      <p>${esc(desc)}</p>
    </div>`).join("");
}

/* ===================== first-run wizard ===================== */
function maybeWizard(state) {
  const skip = new URLSearchParams(location.search).get("wizard") === "0";
  const seen = localStorage.getItem("ok_wizard_done");
  if (seen || skip || !state) return;
  const el = document.createElement("div");
  el.id = "wizard";
  el.innerHTML = `
    <div class="wiz-card">
      <div class="wiz-logo"><b>Optimize<span>Kit</span></b></div>
      <h2>Welcome — four things before you start</h2>
      <div class="wiz-row"><span class="wiz-n">1</span><div><b>Pick your theme</b><p>Every pack redefines the whole surface — background, borders, glow, charts.</p>
        <div class="wiz-themes">${THEME_PACKS.map(([id,c])=>`<div class="pack-opt" data-pack="${id}" title="${id}"><i style="--c:${c}"></i><span>${id}</span></div>`).join("")}</div></div></div>
      <div class="wiz-row"><span class="wiz-n">2</span><div><b>Measure your machine first</b><p>The benchmark scores your CPU, RAM and disk against a reference machine (1000 = mainstream modern build). Run it now — it takes ~4 seconds, saved to history so you can compare after optimizing.</p>
        <button class="btn sm primary" id="wiz-bench">▲ Run benchmark now</button></div></div>
      <div class="wiz-row"><span class="wiz-n">3</span><div><b>Some tweaks need admin</b><p>Close this, then run <span class="mono">OptimizeKit-Admin.bat</span> for the full set (HAGS, timer, network stack…). User-safe tweaks work right now.</p></div></div>
      <div class="wiz-row"><span class="wiz-n">4</span><div><b>Everything is reversible</b><p>Every switch off restores the exact Windows default. Registry backups live in <span class="mono">%LOCALAPPDATA%\\OptimizeKit</span>.</p></div></div>
      <button class="btn primary" id="wiz-done">Let's go</button>
    </div>`;
  document.body.appendChild(el);
  el.querySelectorAll(".pack-opt").forEach((o) => o.addEventListener("click", () => setThemePack(o.dataset.pack, true)));
  $("#wiz-bench").addEventListener("click", async () => {
    const b = $("#wiz-bench"); b.disabled = true; b.textContent = "▲ Measuring…";
    try { await api("/api/bench"); b.textContent = "✔ Done — see Benchmark tab"; toast("✔ Benchmark saved"); } catch (e) { b.textContent = "✖ failed"; }
  });
  $("#wiz-done").addEventListener("click", () => { el.remove(); localStorage.setItem("ok_wizard_done", "1"); });
}

/* ===================== splash + boot ===================== */
(async function boot() {
  const steps = $$("#splash .sp-steps > div");
  const bar = $("#splash .sp-bar i");
  const step = async (key, fn) => {
    const el = steps.find((s) => s.dataset.s === key);
    if (el) el.classList.add("cur");
    try { await fn(); } catch (e) { }
    if (el) { el.classList.remove("cur"); el.classList.add("done"); }
    const doneN = $$("#splash .sp-steps > div.done").length;
    bar.style.width = Math.round(doneN / steps.length * 100) + "%";
  };
  await step("hw", () => api("/api/state"));
  await step("sys", () => api("/api/monitor"));
  // the catalog read (~3 s) and the network read (~3 s) are independent: start both, then advance the bar
  const twP = updateTweakState(true);
  const netP = api("/api/net/status").then((s) => { window.__netStatus = s; }).catch(() => {});
  await step("tw", () => twP);
  await step("net", () => netP);
  await step("ok", () => sleep(160));
  await sleep(240);
  document.body.classList.add("ready");
  loadState();
  renderSpotlights();          // v2.9: dashboard Highlights live from boot
  // fill the Highlights cards as soon as the machine answers (lazy, non-blocking)
  Promise.allSettled([
    api("/api/firmware").then((f) => { spotFirmware = f; }),
    api("/api/drvupdate/report").then((r) => {
      const g = r && r.gpu ? r.gpu : null;
      spotDriverAge = g ? (g.ageDays != null ? g.ageDays + " days" : (g.age || null)) : null;
    }),
    api("/api/games").then((g) => { spotGameCount = (g || []).length; }),
  ]).then(() => { if (currentView === "dashboard") renderSpotlights(); });
  pollMonitor();
  setInterval(pollMonitor, 1000);
  setInterval(loadState, 10000);
  setInterval(updateGamingStatus, 8000);
  updateGamingStatus();
  // restore last theme pack + accent without toasting; ?theme= overrides (deep link / screenshots)
  const wantTheme = new URLSearchParams(location.search).get("theme");
  if (wantTheme && THEME_PACKS.some(([id]) => id === wantTheme)) {
    applyThemePack(wantTheme, false);            // deep link: preview only, never hijacks the saved theme
  } else {
    const savedTheme = localStorage.getItem("ok_theme");
    let conf = null;
    try { conf = await api("/api/settings"); } catch (e) { }
    // one-time migration: drop the legacy flat ok_accent / ui_accent pair, which could
    // carry an accent that belongs to a different theme (silver surfaces, blue accent)
    localStorage.removeItem("ok_accent");
    applyThemePack(savedTheme || (conf && conf.ui_theme) || "magma", false);
    try { await api("/api/settings", { ui_theme: currentTheme, ui_accent: currentAccent }); } catch (e) { }
  }
  try { const s = await api("/api/settings"); applyVisualPrefs(s.ui_particles, s.ui_glitch_text); } catch (e) { }
  // deep link: index.html?view=gaming (used by the docs screenshot harness)
  const want = new URLSearchParams(location.search).get("view");
  if (want && $(".nav-item[data-view=" + want + "]")) show(want);
  const st = await loadState();
  maybeWizard(st);
})();
