// OptimizeKit v1.1 — WormGPT-style dashboard (monitoring, tweaks, network, logs)
// Reuses the DarkGPT design system: #ff3d57 accent, liquid glass, particles.
// Fonts: assets/fonts (Inter, Space Mono) served by the embedded C++ server.
(() => {
"use strict";

const $ = (s) => document.querySelector(s);
const $$ = (s) => document.querySelectorAll(s);
const esc = (s) => String(s ?? "").replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
const fmtBytes = (b) => {
  if (!b && b !== 0) return "—";
  if (b >= 1073741824) return (b / 1073741824).toFixed(1) + " GB";
  if (b >= 1048576) return Math.round(b / 1048576) + " MB";
  return Math.max(1, Math.round(b / 1024)) + " KB";
};
const api = {
  get: (p) => fetch(p).then((r) => r.json()),
  post: (p, body) => fetch(p, { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body || {}) }).then((r) => r.json()),
};

/* ================= particles (reprise WormGPT) ================= */
function particleColor() { return "#ff3d57"; }
function startParticles() {
  const cv = $("#dg-particles");
  if (!cv || cv._running) return;
  const ctx = cv.getContext("2d");
  let W = 0, H = 0, mouse = { x: -9999, y: -9999 };
  const P = [];
  const N = Math.min(90, Math.max(50, Math.floor(window.innerWidth / 18)));
  const resize = () => { W = cv.width = window.innerWidth; H = cv.height = window.innerHeight; };
  const spawn = () => {
    for (let i = 0; i < N; i++) {
      const tier = Math.random();
      const spd = (tier < 0.6 ? 0.04 + Math.random() * 0.05
                 : tier < 0.9 ? 0.09 + Math.random() * 0.10
                 : 0.20 + Math.random() * 0.18) * 0.55;
      P.push({
        x: Math.random() * W, y: Math.random() * H,
        vx: (Math.random() - .5) * spd * 2, vy: (Math.random() - .5) * spd * 2,
        r: tier < 0.6 ? Math.random() * 1.1 + .4 : tier < 0.9 ? Math.random() * 1.5 + .7 : Math.random() * 2 + 1.1,
        wobble: Math.random() * Math.PI * 2,
        wobbleSpd: (0.0012 + Math.random() * 0.004) * 0.55,
        alpha: tier < 0.6 ? .22 + Math.random() * .16 : tier < 0.9 ? .34 + Math.random() * .2 : .5 + Math.random() * .25
      });
    }
  };
  window.addEventListener("resize", resize);
  window.addEventListener("mousemove", (e) => { mouse.x = e.clientX; mouse.y = e.clientY; });
  window.addEventListener("mouseleave", () => { mouse.x = -9999; mouse.y = -9999; });
  resize(); spawn();
  const rgb = [255, 61, 87];
  (function frame() {
    ctx.clearRect(0, 0, W, H);
    for (const p of P) {
      const dx = mouse.x - p.x, dy = mouse.y - p.y;
      const d2 = dx * dx + dy * dy;
      if (d2 < 200 * 200 && d2 > 1) {
        const f = 0.025 / Math.sqrt(d2);
        p.vx += dx * f * 0.55; p.vy += dy * f * 0.55;
      }
      p.vx += Math.cos(p.wobble) * 0.0012;
      p.vy += Math.sin(p.wobble) * 0.0012;
      p.wobble += p.wobbleSpd;
      p.vx *= 0.995; p.vy *= 0.995;
      p.x += p.vx; p.y += p.vy;
      if (p.x < -10) p.x = W + 10; if (p.x > W + 10) p.x = -10;
      if (p.y < -10) p.y = H + 10; if (p.y > H + 10) p.y = -10;
      ctx.beginPath();
      ctx.arc(p.x, p.y, p.r, 0, Math.PI * 2);
      ctx.fillStyle = `rgba(${rgb[0]},${rgb[1]},${rgb[2]},${p.alpha})`;
      ctx.fill();
    }
    cv._raf = requestAnimationFrame(frame);
  })();
}

/* ================= toast ================= */
let toastTimer = null;
function toast(msg, kind = "info") {
  const el = $("#toast");
  el.textContent = msg;
  el.style.borderColor = kind === "ok" ? "rgba(61,214,140,.4)" : kind === "err" ? "rgba(255,61,87,.5)" : "";
  el.classList.add("visible");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => el.classList.remove("visible"), 2600);
}

/* ================= sparkline ================= */
function sparkline(canvas, data, color, max = 100) {
  const ctx = canvas.getContext("2d");
  const w = canvas.width = canvas.clientWidth * devicePixelRatio;
  const h = canvas.height = canvas.clientHeight * devicePixelRatio;
  ctx.clearRect(0, 0, w, h);
  if (!data.length) return;
  // grid
  ctx.strokeStyle = "rgba(255,255,255,.05)";
  ctx.lineWidth = 1;
  for (let i = 1; i < 4; i++) {
    ctx.beginPath(); ctx.moveTo(0, h * i / 4); ctx.lineTo(w, h * i / 4); ctx.stroke();
  }
  const pts = data.slice(-60);
  const step = w / Math.max(1, pts.length - 1);
  const y = (v) => h - (Math.min(v, max) / max) * (h - 6) - 3;
  // area
  ctx.beginPath();
  ctx.moveTo(0, h);
  pts.forEach((v, i) => ctx.lineTo(i * step, y(v)));
  ctx.lineTo((pts.length - 1) * step, h);
  ctx.closePath();
  const g = ctx.createLinearGradient(0, 0, 0, h);
  g.addColorStop(0, color.replace("ALPHA", ".25"));
  g.addColorStop(1, color.replace("ALPHA", "0"));
  ctx.fillStyle = g;
  ctx.fill();
  // line
  ctx.beginPath();
  pts.forEach((v, i) => i ? ctx.lineTo(i * step, y(v)) : ctx.moveTo(0, y(v)));
  ctx.strokeStyle = color.replace("ALPHA", ".95");
  ctx.lineWidth = 2 * devicePixelRatio;
  ctx.shadowColor = color.replace("ALPHA", ".6");
  ctx.shadowBlur = 8;
  ctx.stroke();
  ctx.shadowBlur = 0;
  // last point
  const lx = (pts.length - 1) * step, ly = y(pts[pts.length - 1]);
  ctx.beginPath(); ctx.arc(lx, ly, 3 * devicePixelRatio, 0, Math.PI * 2);
  ctx.fillStyle = color.replace("ALPHA", "1");
  ctx.fill();
}

/* ================= state ================= */
const S = { view: "dashboard", tweaks: [], sel: new Set(), admin: false, filter: "all" };

/* ================= views ================= */
async function loadState() {
  try {
    const s = await api.get("/api/state");
    S.admin = !!s.admin;
    $("#st-os").textContent = s.os;
    $("#st-cpu").textContent = `${s.cpu} (${s.threads} threads)`;
    $("#st-gpu").textContent = s.gpu || "—";
    $("#st-ram").textContent = `${fmtBytes(s.ramTotal)} · ${fmtBytes(s.ramAvail)} libre`;
    $("#st-plan").textContent = s.plan;
    $("#st-gm").textContent = s.gameMode ? "On" : "Off";
    $("#st-hags").textContent = s.hags ? "On" : "Off";
    $("#st-uptime").textContent = s.uptime;
    $("#user-name").textContent = s.user || "user";
    $("#avatar").textContent = (s.user || "U")[0].toUpperCase();
    const ad = $("#admin-pill");
    ad.textContent = s.admin ? "● Administrator" : "○ Standard user";
    ad.style.color = s.admin ? "var(--ok)" : "var(--warn)";
    const adSide = $("#admin-pill-side");
    adSide.textContent = s.admin ? "administrator" : "standard user";
    document.querySelectorAll(".adm-lock").forEach((e) => e.style.display = s.admin ? "none" : "inline-flex");
    if (s.gpuDriver) { const d = $("#st-gpu-d"); if (d) d.textContent = "Driver " + s.gpuDriver; }
    const gh = $("#st-gpu-h"); if (gh && s.gpu) gh.textContent = s.gpu;
  } catch { toast("API unreachable", "err"); }
}

/* ---- monitoring ---- */
const hist = { cpu: [], ram: [], gpu: [] };
let monBusy = false;
async function tickMonitor() {
  if (monBusy) return;
  monBusy = true;
  try {
    const m = await api.get("/api/monitor");
    hist.cpu = m.hist?.cpu || hist.cpu;
    hist.ram = m.hist?.ram || hist.ram;
    hist.gpu = m.hist?.gpu || hist.gpu;
    if (S.view === "dashboard") {
      $("#m-cpu-v").textContent = m.cpu.toFixed(0) + "%";
      $("#m-ram-v").textContent = m.ram.toFixed(0) + "%";
      $("#m-gpu-v").textContent = m.gpu.toFixed(0) + "%";
      $("#m-cpu-clock").textContent = m.cpuClock ? m.cpuClock.toFixed(0) + "% perf" : "";
      $("#m-ram-abs").textContent = fmtBytes(m.ramUsed) + " / " + fmtBytes(m.ramTotal);
      $("#m-disk-sub").textContent = `disk ${m.diskRead.toFixed(0)}/${m.diskWrite.toFixed(0)} MB/s`;
      $("#m-net-v").textContent = `↓${m.netDown < 1024 ? m.netDown.toFixed(0) + " KB/s" : (m.netDown / 1024).toFixed(1) + " MB/s"} · ↑${m.netUp < 1024 ? m.netUp.toFixed(0) + " KB/s" : (m.netUp / 1024).toFixed(1) + " MB/s"}`;
      $("#m-proc-v").textContent = `${m.procs} procs · ${m.threads} threads`;
      sparkline($("#m-cpu-g"), hist.cpu, "rgba(255,61,87,ALPHA)");
      sparkline($("#m-ram-g"), hist.ram, "rgba(245,165,36,ALPHA)");
      sparkline($("#m-gpu-g"), hist.gpu, "rgba(61,214,140,ALPHA)");
    }
  } catch {}
  monBusy = false;
}

async function tickProcs() {
  if (S.view !== "dashboard") return;
  try {
    const p = await api.get("/api/processes");
    const el = $("#proc-list");
    if (!p.length) { el.innerHTML = `<div class="conv-empty">…</div>`; return; }
    el.innerHTML = p.map((r) => `
      <div class="proc-row">
        <span class="proc-name" title="${esc(r.name)}">${esc(r.name)}</span>
        <span class="proc-pid mono">#${r.pid}</span>
        <span class="proc-cpu mono ${r.cpu > 25 ? "hot" : ""}">${r.cpu.toFixed(1)}%</span>
        <span class="proc-ram mono">${r.ramMB} MB</span>
      </div>`).join("");
  } catch {}
}

/* ---- tweaks ---- */
function tweakCard(t) {
  const badge = t.admin ? `<span class="badge adm">ADMIN</span>` : `<span class="badge usr">USER</span>`;
  const stars = "★".repeat(t.impact) + `<span class="st-off">${"★".repeat(3 - t.impact)}</span>`;
  return `
  <div class="mcard tweak ${S.sel.has(t.id) ? "sel" : ""} ${S.filter !== "all" && !matchFilter(t) ? "hide" : ""}" data-id="${t.id}">
    <div class="t-main">
      <div class="t-top"><b>${esc(t.name)}</b>${badge}<span class="impact">${stars}</span></div>
      <div class="t-desc">${esc(t.desc)}</div>
      <div class="t-src">source: ${esc(t.source)} · state: ${t.applied ? `<span class="ok-t">applied</span>` : `<span class="dim-t">default</span>`}</div>
    </div>
    <div class="t-actions">
      <button class="btn sm" data-act="restore">Restore</button>
      <button class="btn sm primary" data-act="apply">Apply</button>
    </div>
  </div>`;
}
function matchFilter(t) {
  if (S.filter === "gaming") return t.impact >= 2;
  if (S.filter === "privacy") return /telemetry|advertis|activity|bing|tailored|copilot|edge/.test(t.id);
  if (S.filter === "debloat") return /bloat|onedrive|xbox|sysmain|search_index/.test(t.id);
  return true;
}
async function loadTweaks() {
  S.tweaks = await api.get("/api/tweaks");
  const box = $("#tweaks-list");
  box.innerHTML = S.tweaks.map(tweakCard).join("");
  box.querySelectorAll(".mcard").forEach((card) => {
    const id = card.dataset.id;
    card.addEventListener("click", () => {
      S.sel.has(id) ? S.sel.delete(id) : S.sel.add(id);
      card.classList.toggle("sel");
    });
    card.querySelector('[data-act="apply"]').addEventListener("click", async (e) => {
      e.stopPropagation();
      const r = await api.post("/api/tweaks/apply", { [id]: true });
      r.errors?.length ? toast("Failed: " + r.errors[0], "err") : toast("Applied ✓", "ok");
      loadTweaks();
    });
    card.querySelector('[data-act="restore"]').addEventListener("click", async (e) => {
      e.stopPropagation();
      const r = await api.post("/api/tweaks/restore", [id]);
      r.errors?.length ? toast("Failed: " + r.errors[0], "err") : toast("Restored ✓", "ok");
      loadTweaks();
    });
  });
  updateCounts();
}
function updateCounts() {
  const sel = S.sel.size;
  $("#sel-count").textContent = `${sel} selected`;
  $("#btn-apply-sel").disabled = !sel;
  $("#btn-restore-sel").disabled = !sel;
}

/* ---- network ---- */
async function loadPings() {
  const box = $("#ping-list");
  box.innerHTML = `<div class="conv-empty">testing<span class="dots"><i></i><i></i><i></i></span></div>`;
  const rows = await api.get("/api/ping");
  box.innerHTML = rows.map((r) => {
    const cls = !r.ok ? "err" : r.ms < 30 ? "ok" : r.ms < 80 ? "warn" : "err";
    return `<div class="ping-row">
      <span class="p-name">${esc(r.name)}</span>
      <span class="p-host mono">${esc(r.host)}</span>
      <span class="p-ms mono ${cls}">${r.ok ? r.ms + " ms" : "timeout"}</span>
      <span class="p-dot ${cls}"></span>
    </div>`;
  }).join("");
}
async function quickPing() {
  const host = $("#ping-input").value.trim();
  if (!host) return;
  const out = $("#ping-quick-out");
  out.textContent = "pinging " + host + "…";
  try {
    const r = await api.post("/api/ping", { host });
    out.textContent = r.ok ? `${host} : ${r.ms} ms` : `${host} : timeout`;
    out.className = r.ok ? "mono ok-t" : "mono err-t";
  } catch { out.textContent = "error"; }
}

/* ---- logs ---- */
async function loadLogs() {
  const j = await api.get("/api/logs");
  const el = $("#log-box");
  const lines = (j.log || "").split("\n").slice(-400);
  el.innerHTML = lines.map((l) => {
    const cls = /\[OK\]/.test(l) ? "ok" : /\[FAIL\]/.test(l) ? "err" : /\[WARN\]/.test(l) ? "warn" : "";
    const m = l.match(/^(\[[\d\- :]+\])(.*)$/);
    return m ? `<div class="log-line ${cls}"><span class="log-t">${esc(m[1])}</span>${esc(m[2])}</div>`
             : `<div class="log-line ${cls}">${esc(l)}</div>`;
  }).join("") || `<div class="conv-empty">log empty</div>`;
  el.scrollTop = el.scrollHeight;
}

/* ---- actions ---- */
async function doProfile(name) {
  toast(`Running ${name} profile…`);
  const r = await api.post("/api/profile", { name });
  toast(r.summary || "done", r.failed ? "err" : "ok");
  loadState(); loadTweaks(); loadLogs();
}
async function doClean() {
  toast("Cleaning…");
  const r = await api.post("/api/clean");
  toast(`Freed ${fmtBytes(r.freedBytes)}`, "ok");
  loadLogs();
}
async function applySel() {
  if (!S.sel.size) return;
  const body = {};
  S.sel.forEach((id) => body[id] = true);
  const r = await api.post("/api/tweaks/apply", body);
  toast(`${r.applied} applied${r.errors?.length ? " · " + r.errors.length + " failed" : ""}`, r.errors?.length ? "err" : "ok");
  S.sel.clear(); loadTweaks(); loadLogs();
}
async function restoreSel() {
  if (!S.sel.size) return;
  const r = await api.post("/api/tweaks/restore", [...S.sel]);
  toast(`${r.restored} restored`, "ok");
  S.sel.clear(); loadTweaks(); loadLogs();
}

/* ================= navigation ================= */
const views = ["dashboard", "tweaks", "gaming", "privacy", "drivers", "network", "logs", "about"];
function showView(v) {
  S.view = v;
  views.forEach((x) => $("#view-" + x)?.classList.toggle("hidden", x !== v));
  $$(".nav-item").forEach((n) => n.classList.toggle("active", n.dataset.view === v));
  if (v === "tweaks") loadTweaks();
  if (v === "network") loadPings();
  if (v === "logs") loadLogs();
  if (v === "dashboard") { loadState(); tickMonitor(); tickProcs(); }
}

/* ================= init ================= */
window.addEventListener("DOMContentLoaded", () => {
  startParticles();
  $$(".nav-item").forEach((n) => n.addEventListener("click", () => showView(n.dataset.view)));
  const on = (sel, fn) => { const el = $(sel); if (el) el.addEventListener("click", fn); };
  on("#btn-quick-optimize", () => doProfile("gaming"));
  on("#btn-clean", doClean);
  on("#btn-privacy", () => doProfile("privacy"));
  on("#btn-apply-sel", applySel);
  on("#btn-restore-sel", restoreSel);
  on("#btn-sel-all", () => {
    S.sel.clear();
    S.tweaks.filter(matchFilter).forEach((t) => S.sel.add(t.id));
    $$("#tweaks-list .mcard").forEach((c) => c.classList.add("sel"));
    updateCounts();
  });
  on("#btn-sel-none", () => {
    S.sel.clear();
    $$("#tweaks-list .mcard").forEach((c) => c.classList.remove("sel"));
    updateCounts();
  });
  $$(".chip[data-filter]").forEach((c) => c.addEventListener("click", () => {
    $$(".chip[data-filter]").forEach((x) => x.classList.remove("on"));
    c.classList.add("on");
    S.filter = c.dataset.filter;
    $$("#tweaks-list .mcard").forEach((card) => {
      const t = S.tweaks.find((x) => x.id === card.dataset.id);
      card.classList.toggle("hide", !matchFilter(t));
    });
  }));
  on("#btn-ping-go", quickPing);
  const pi = $("#ping-input"); if (pi) pi.addEventListener("keydown", (e) => { if (e.key === "Enter") quickPing(); });
  on("#btn-ping-refresh", loadPings);
  on("#btn-log-open", () => api.get("/api/open?what=log"));
  on("#btn-open-logfolder", () => api.get("/api/open?what=logfolder"));
  on("#btn-log-refresh", loadLogs);
  on("#btn-gpu-page", () => window.open(gpuVendorUrl, "_blank"));
  on("#btn-devmgmt", () => api.get("/api/open?what=devmgmt"));
  on("#btn-dxdiag", () => api.get("/api/open?what=dxdiag"));
  on("#btn-full-gaming", () => doProfile("gaming"));

  // quick tweak grids (gaming / privacy views)
  const gamingIds = [
    ["game_dvr_off", "Game DVR off", "no background capture"],
    ["network_gaming", "Gaming network", "no Nagle, no throttle"],
    ["timer_high", "Timer 0.5 ms", "global resolution"],
    ["hags_on", "HAGS", "GPU scheduling"],
    ["mpo_off", "MPO fix", "24H2 stutter"],
    ["power_ultimate", "Ultimate plan", "max power"],
    ["mouse_precision", "Raw mouse", "1:1, no accel"],
    ["fso_on", "Exclusive FS", "true fullscreen"]];
  const privacyIds = [
    ["telemetry_off", "Telemetry off", "DiagTrack & co"],
    ["advertising_off", "Ad ID off", "no ad tracking"],
    ["activity_history", "Activity off", "no timeline"],
    ["bing_search", "Bing out", "local search"],
    ["tailored_experiences", "Tailored off", "no targeted tips"],
    ["telemetry_tasks", "CEIP tasks off", "scheduled tasks"],
    ["windows_copilot", "Copilot off", "policy removed"],
    ["edge_bing_blocking", "Edge quiet", "no background"]];
  const mkQuick = (el, list) => {
    el.innerHTML = list.map(([id, t, d]) => `<button class="quick-t" data-id="${id}">${t}<small>${d}</small></button>`).join("");
    el.querySelectorAll(".quick-t").forEach((b) => b.addEventListener("click", async () => {
      toast("Applying " + b.dataset.id + "…");
      const r = await api.post("/api/tweaks/apply", { [b.dataset.id]: true });
      r.errors?.length ? toast(r.errors[0], "err") : toast("Applied ✓", "ok");
      loadLogs();
    }));
  };
  mkQuick($("#gaming-quick"), gamingIds);
  mkQuick($("#privacy-quick"), privacyIds);

  loadState();
  tickMonitor();
  setInterval(tickMonitor, 1000);
  setInterval(tickProcs, 2500);
  showView("dashboard");
});

let gpuVendorUrl = "https://www.nvidia.com/Download/index.aspx";
api.get("/api/state").then((s) => {
  const g = (s.gpu || "").toLowerCase();
  if (/radeon|amd/.test(g)) gpuVendorUrl = "https://www.amd.com/en/support";
  else if (/intel|iris|arc|uhd/.test(g)) gpuVendorUrl = "https://www.intel.com/content/www/us/en/download-center/home.html";
  $("#st-gpu").textContent = s.gpu;
});
})();
