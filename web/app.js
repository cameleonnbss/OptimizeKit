/* OptimizeKit v2.2 — WormGPT design system + Gaming Control Center */
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

/* ===================== i18n (EN base, FR) ===================== */
const I18N = {
  en: { dashboard:"Dashboard", gaming:"Gaming Center", scan:"Scan PC", optimize:"Optimize", tweaks:"Tweaks", games:"Games",
        network:"Network", ram:"RAM", storage:"Storage", startup:"Startup", drivers:"Drivers", privacy:"Privacy",
        diag:"Diagnostics", bench:"Benchmark", tools:"Tools", logs:"Logs", settings:"Settings", about:"About" },
  fr: { dashboard:"Tableau de bord", gaming:"Centre Gaming", scan:"Analyser le PC", optimize:"Optimiser", tweaks:"Tweaks", games:"Jeux",
        network:"Réseau", ram:"RAM", storage:"Stockage", startup:"Démarrage", drivers:"Pilotes", privacy:"Confidentialité",
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

/* ===================== theme picker ===================== */
function applyAccent(hex) {
  if (!/^#[0-9a-fA-F]{6}$/.test(hex)) return;
  const r = parseInt(hex.slice(1, 3), 16), g = parseInt(hex.slice(3, 5), 16), b = parseInt(hex.slice(5, 7), 16);
  document.documentElement.style.setProperty("--accent", hex);
  document.documentElement.style.setProperty("--accent-hover", hex);
  document.documentElement.style.setProperty("--accent-rgb", `${r},${g},${b}`);
  $$(".theme-opt").forEach((o) => o.classList.toggle("on", o.dataset.accent === hex));
  localStorage.setItem("ok_accent", hex);
  if (currentView === "dashboard") { const mc = monitorColors(); sparkline($("#m-cpu-g"), histCpu, mc.cpu); sparkline($("#m-gpu-g"), histGpu, mc.gpu); sparkline($("#m-disk-g"), histDisk, mc.disk); }
  if (currentView === "gaming") { const ring = $(".gc-ring"); if (ring) ring.style.background = `conic-gradient(${hex} calc(var(--p,0)*1%),rgba(255,255,255,.07) 0)`; }
}
async function setTheme(hex, persist) {
  applyAccent(hex);
  if (persist) { await api("/api/settings", { ui_accent: hex }).catch(() => {}); toast("Theme updated"); }
}

/* ===================== theme packs (full-surface) ===================== */
const THEME_PACKS = [
  ["magma",  "#ff3d57", "Magma — deep black + crimson"],
  ["ocean",  "#38bdf8", "Ocean — abyssal blue"],
  ["matrix", "#22c55e", "Matrix — terminal green"],
  ["violet", "#a78bfa", "Violet — neon purple"],
  ["gold",   "#f59e0b", "Gold — amber elite"],
  ["steel",  "#94a3b8", "Steel — cold silver"],
  ["rose",   "#f472b6", "Rose — soft pink"],
];
function applyThemePack(name) {
  const pack = THEME_PACKS.find(([id]) => id === name) || THEME_PACKS[0];
  document.body.dataset.theme = pack[0];
  applyAccent(pack[1]);
  localStorage.setItem("ok_theme", pack[0]);
  $$(".pack-opt").forEach((o) => o.classList.toggle("on", o.dataset.pack === pack[0]));
}
async function setThemePack(name, persist) {
  applyThemePack(name);
  if (persist) { await api("/api/settings", { ui_theme: name, ui_accent: THEME_PACKS.find(([id]) => id === name)[1] }).catch(() => {}); toast("Theme pack: " + name); }
}
on("#btn-theme", () => $("#theme-flyout").classList.toggle("hidden"));
document.addEventListener("click", (e) => {
  const fly = $("#theme-flyout");
  if (fly && !fly.classList.contains("hidden") && !e.target.closest("#theme-flyout") && !e.target.closest("#btn-theme")) fly.classList.add("hidden");
});
$$("#theme-flyout .theme-opt").forEach((o) => o.addEventListener("click", () => { applyAccent(o.dataset.accent); api("/api/settings", { ui_accent: o.dataset.accent }).catch(() => {}); }));
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
    if (s.ui_theme) applyThemePack(s.ui_theme);
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
  el.innerHTML = list.map((t, i) => `
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
    </div>`).join("") || `<div class="conv-empty muted" style="padding:14px">no tweak matches</div>`;
  $$("#tweaks-list .tw2").forEach((row) => row.addEventListener("click", (e) => {
    if (e.target.closest(".ok-switch")) return;       // switch has its own handler
    row.querySelector(".ok-switch")?.click();          // clicking the card toggles too
  }));
  $$("#tweaks-list .ok-switch").forEach((sw) => sw.addEventListener("click", (e) => { e.stopPropagation(); toggleTweak(sw); }));
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

/* ===================== games ===================== */
let gamesCache = [];
async function loadGames() {
  const grid = $("#games-grid");
  grid.innerHTML = `<div class="conv-empty muted" style="padding:14px">scanning launcher libraries…</div>`;
  try {
    gamesCache = await api("/api/games");
    if (!gamesCache.length) {
      grid.innerHTML = `<div class="conv-empty muted" style="padding:14px">No games found — install one via Steam/Epic/Xbox, or add a game manually to a launcher library folder.</div>`;
      return;
    }
    grid.innerHTML = gamesCache.map((g, i) => `
      <div class="game-card" data-i="${i}">
        ${g.icon ? `<img src="/api/game-icon/${encodeURIComponent(g.icon)}" alt=""/>` : `<span class="gph">☰</span>`}
        <div class="g-meta"><b>${esc(g.name)}</b><span>${esc(g.launcher)}</span></div>
        ${g.running ? `<span class="g-run" title="running"></span>` : ""}
      </div>`).join("");
    $$("#games-grid .game-card").forEach((c) => c.addEventListener("click", () => selectGame(+c.dataset.i)));
    const noIcon = gamesCache.filter((g) => !g.icon).slice(0, 12);
    for (const g of noIcon) {
      await api("/api/games/icon", { id: g.id }).catch(() => { });
    }
    if (noIcon.length) loadGamesIcons();
  } catch (e) {
    grid.innerHTML = `<div class="conv-empty muted" style="padding:14px">game detection failed</div>`;
  }
}
async function loadGamesIcons() {
  gamesCache = await api("/api/games");
  $$("#games-grid .game-card").forEach((c) => {
    const g = gamesCache[+c.dataset.i];
    if (g && g.icon) c.querySelector(".gph")?.replaceWith(Object.assign(document.createElement("img"), { src: "/api/game-icon/" + encodeURIComponent(g.icon) }));
  });
}
on("#btn-games-refresh", loadGames);
function selectGame(i) {
  const g = gamesCache[i]; if (!g) return;
  $$("#games-grid .game-card").forEach((c, j) => c.classList.toggle("sel", i === j));
  const el = $("#game-profile");
  el.classList.remove("hidden");
  el.innerHTML = `
    <div class="profile-hero">
      <h2>${esc(g.name)} <span style="color:var(--accent)">profile</span></h2>
      <p>${esc(g.exe)}</p>
      <div style="display:flex;gap:10px;flex-wrap:wrap;margin-top:10px">
        <button class="btn primary" id="gp-apply">Boost this game (high priority + gaming flags)</button>
        <button class="btn" id="gp-gmode">▶ Start in Gaming Mode</button>
        <button class="btn danger" id="gp-clear">↺ Clear boost</button>
      </div>
    </div>`;
  $("#gp-apply").addEventListener("click", async () => {
    const r = await api("/api/games/profile", { id: g.id, action: "apply" });
    toast(r.ok ? "✔ " + g.name + " boosted" : "✖ " + r.error);
  });
  $("#gp-gmode").addEventListener("click", async () => {
    const r = await api("/api/gaming/enter", { game: g.id });
    toast(r.ok ? "▶ Gaming mode for " + g.name : "✖ " + r.error);
    updateGamingStatus();
  });
  $("#gp-clear").addEventListener("click", async () => {
    await api("/api/games/profile", { id: g.id, action: "clear" });
    toast("↺ boost cleared");
  });
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

/* ===================== storage ===================== */
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
    const files = await api("/api/storage/files", { root: "C:\\", top: 12 });
    $("#largest-files").classList.remove("conv-empty");
    $("#largest-files").innerHTML = files.map((f) =>
      `<div class="file-row"><span class="fp">${esc(f.path)}</span><span class="fs">${esc(f.pretty)}</span></div>`).join("");
  } catch (e) { }
}

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

/* ===================== tools ===================== */
const TOOLS = [
  ["taskmgr", "Task Manager", "processes & resources"],
  ["resmon", "Resource Monitor", "CPU/disk/net in depth"],
  ["perfmon", "Performance Monitor", "counters & graphs"],
  ["devmgmt", "Device Manager", "drivers & devices"],
  ["eventvwr", "Event Viewer", "system logs"],
  ["services", "Services", "manage services"],
  ["msinfo32", "System Information", "full hardware report"],
  ["diskmgmt", "Disk Management", "partitions"],
  ["regedit", "Registry Editor", "advanced"],
  ["cleanmgr", "Disk Cleanup", "Windows built-in"],
  ["dfrgui", "Defragment & Optimize", "drive maintenance"],
  ["wscui", "Windows Security", "antivirus & firewall"],
  ["ncpa", "Network Connections", "adapters"],
  ["powercfg", "Power Options", "plans"],
  ["wt", "Windows Terminal", "terminal"],
  ["powershell", "PowerShell", "automation"],
];
$("#tools-grid").innerHTML = TOOLS.map(([id, n, d]) => `<div class="tool-card" data-t="${id}"><b>${n}</b><span>${d}</span></div>`).join("");
$$("#tools-grid .tool-card").forEach((c) => c.addEventListener("click", async () => {
  await api("/api/tools", { tool: c.dataset.t });
  toast("Opening " + c.querySelector("b").textContent + "…");
}));

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
    $("#set-accent").value = s.ui_accent || "#ff3d57";
    $("#set-theme").value = s.ui_theme || "magma";
    $("#set-lang").value = s.ui_lang || "en";
    applyLang(s.ui_lang || "en");
    $("#set-anim").checked = s.ui_particles !== false;
    $("#set-glitch").checked = s.ui_glitch !== false;
    $("#set-confirm").checked = s.confirm_destructive !== false;
    $("#set-autoback").checked = s.auto_backup !== false;
    $("#set-dns").checked = s.dns_managed === true;
    $("#set-killlist").value = (s.gaming_kill_list || []).join(", ");
    applyAccent(s.ui_accent || "#ff3d57");
  } catch (e) { }
}
on("#btn-set-save", async () => {
  const body = {
    ui_accent: $("#set-accent").value,
    ui_theme: $("#set-theme").value,
    ui_lang: $("#set-lang").value,
    ui_particles: $("#set-anim").checked,
    ui_glitch: $("#set-glitch").checked,
    confirm_destructive: $("#set-confirm").checked,
    auto_backup: $("#set-autoback").checked,
    dns_managed: $("#set-dns").checked,
    gaming_kill_list: $("#set-killlist").value.split(",").map((x) => x.trim()).filter(Boolean),
  };
  await api("/api/settings", body);
  applyThemePack(body.ui_theme);
  applyAccent(body.ui_accent);
  applyLang(body.ui_lang);
  $("#set-saved").textContent = "saved ✓";
  setTimeout(() => ($("#set-saved").textContent = ""), 2200);
  toast("✔ Settings saved");
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
      await overlay.done(true, r.errors.length ? r.applied + " applied, " + r.errors.length + " need admin via OptimizeKit.bat" : "competitive preset active");
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
        <div class="wiz-themes">${[["magma","#ff3d57"],["ocean","#38bdf8"],["matrix","#22c55e"],["violet","#a78bfa"],["gold","#f59e0b"],["steel","#94a3b8"],["rose","#f472b6"]].map(([id,c])=>`<div class="pack-opt" data-pack="${id}" title="${id}"><i style="--c:${c}"></i><span>${id}</span></div>`).join("")}</div></div></div>
      <div class="wiz-row"><span class="wiz-n">2</span><div><b>Measure your machine first</b><p>The benchmark scores your CPU, RAM and disk against a reference machine (1000 = mainstream modern build). Run it now — it takes ~4 seconds, saved to history so you can compare after optimizing.</p>
        <button class="btn sm primary" id="wiz-bench">▲ Run benchmark now</button></div></div>
      <div class="wiz-row"><span class="wiz-n">3</span><div><b>Some tweaks need admin</b><p>Close this, then run <span class="mono">OptimizeKit.bat</span> (option 1) for the full set (HAGS, timer, network stack…). User-safe tweaks work right now.</p></div></div>
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
  await step("tw", () => updateTweakState(true));
  await step("net", () => api("/api/net/status"));
  await step("ok", () => sleep(160));
  await sleep(240);
  document.body.classList.add("ready");
  loadState();
  pollMonitor();
  setInterval(pollMonitor, 1000);
  setInterval(loadState, 10000);
  setInterval(updateGamingStatus, 8000);
  updateGamingStatus();
  // restore last theme pack + accent without toasting; ?theme= overrides (deep link / screenshots)
  const wantTheme = new URLSearchParams(location.search).get("theme");
  if (wantTheme && THEME_PACKS.some(([id]) => id === wantTheme)) { applyThemePack(wantTheme); }
  else {
    try { const s = await api("/api/settings"); if (s.ui_theme) applyThemePack(s.ui_theme); } catch (e) { }
    const savedTheme = localStorage.getItem("ok_theme");
    if (savedTheme) applyThemePack(savedTheme);
  }
  // deep link: index.html?view=gaming
  const want = new URLSearchParams(location.search).get("view");
  if (want && $(".nav-item[data-view=" + want + "]")) show(want);
  const st = await loadState();
  maybeWizard(st);
})();
