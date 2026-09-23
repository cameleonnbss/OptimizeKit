// OptimizeKit - liquid glass dashboard (Direct2D + DirectWrite, no other deps)
// Visual style inspired by WormGPT-desktop (cameleonnbss): dark glass, accent gradients.
// v2.7: native-first (this window IS the app - no Edge, no WebView2 by default),
// new tabs: Firmware, Storage, Startup, Settings; enriched Dashboard & Drivers.
#include "ui.h"
#include "engine.h"
#include "firmware.h"
#include "drvupdate.h"
#include "storage.h"
#include "ram.h"
#include "json.hpp"
#include <d2d1.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <map>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dwmapi.lib")

namespace ok::ui {

using ok::tweaks::catalog;
using ok::tweaks::Tweak;
using json = nlohmann::json;

// ------------------------------------------------------------------ theme
static const D2D1_COLOR_F C_BASE    = { 0.035f, 0.035f, 0.055f, 1.0f };
static const D2D1_COLOR_F C_TEXT    = { 0.93f, 0.94f, 0.98f, 1.0f };
static const D2D1_COLOR_F C_DIM     = { 0.62f, 0.64f, 0.72f, 1.0f };
static const D2D1_COLOR_F C_FAINT   = { 0.42f, 0.44f, 0.52f, 1.0f };
static const D2D1_COLOR_F C_ACC     = { 0.30f, 0.62f, 1.00f, 1.0f };   // accent blue
static const D2D1_COLOR_F C_ACC2    = { 0.72f, 0.35f, 1.00f, 1.0f };   // accent purple
static const D2D1_COLOR_F C_OK      = { 0.30f, 0.90f, 0.55f, 1.0f };
static const D2D1_COLOR_F C_WARN    = { 1.00f, 0.76f, 0.30f, 1.0f };
static const D2D1_COLOR_F C_ERR     = { 1.00f, 0.42f, 0.45f, 1.0f };
static const D2D1_COLOR_F C_GLASS   = { 1.0f, 1.0f, 1.0f, 0.055f };
static const D2D1_COLOR_F C_GLASSBR = { 1.0f, 1.0f, 1.0f, 0.14f };

// ------------------------------------------------------------------ state
enum Tab { T_DASH, T_TWEAKS, T_GAMING, T_FIRMWARE, T_STORAGE, T_STARTUP, T_SETTINGS,
           T_PRIVACY, T_DRIVERS, T_PING, T_LOGS, T_ABOUT, T_COUNT };

struct PingRow {
    wstring name, host;
    bool testing = true, ok = false;
    int  ms = -1;
};

struct State {
    Tab tab = T_DASH;
    POINT mouse{ -1,-1 };
    bool mouseDown = false;
    POINT clickAt{ -1,-1 };
    std::map<string, bool> sel;                 // tweak selection
    std::map<string, float> scroll;             // per-tab scroll
    std::vector<PingRow> pings;
    bool pingsBusy = false;
    bool pingsDirty = true;
    std::vector<gameboost::RunningProcess> procs;
    bool procsBusy = false;
    bool procsDirty = true;
    wstring status = L"Ready.";
    int    statusKind = 0;                      // 0 info 1 ok 2 err
    wstring pingHostInput;
    bool   pingHostFocused = false;
    int    tweakFilter = 0;                     // 0 all, 1 gaming, 2 privacy, 3 debloat
    wstring gpuName, gpuDrv, gpuDate;
    wstring planName;
    float  t0 = 0;                              // anim clock
    bool   sysDirty = true;
    sysinfo::Info si;

    // ---- v2.7 native modules ----
    // firmware
    json      fw;
    std::atomic<bool> fwBusy{ false };
    bool      fwDirty = true;
    // driver update
    json      drvRep;
    json      drvProblems;
    std::atomic<bool> drvBusy{ false };
    bool      drvDirty = true;
    // storage
    json      drives;
    bool      drivesDirty = true;
    json      largestFiles;
    wstring   largestRoot = L"C:\\";
    bool      largestBusy = false;
    // startup
    std::vector<startup::Entry> startupRows;
    bool      startupBusy = false;
    bool      startupDirty = true;
    // settings toggles (persisted in config.json)
    bool      setConfirm = true;
    bool      setAutoback = true;
    bool      setDnsManaged = false;
    // benchmark summary (dashboard tile)
    json      lastBench;
    bool      benchBusy = false;
} g;

static ID2D1Factory*          g_fac = nullptr;
static ID2D1HwndRenderTarget* g_rt  = nullptr;
static IDWriteFactory*        g_dw  = nullptr;
static IDWriteTextFormat*     g_fTitle = nullptr, * g_fH1 = nullptr, * g_fH2 = nullptr,
                          * g_fBody = nullptr, * g_fSmall = nullptr, * g_fMono = nullptr;
static HWND g_hwnd = nullptr;
static float g_dpiScale = 1.0f;
static D2D1_SIZE_F g_sz{ 1200, 760 };

struct Action { std::function<void()> fn; };
static std::vector<Action> g_actions;   // executed after paint
static std::mutex g_pingMtx;
static std::mutex g_dataMtx;            // guards fw/drv/startup/drives json swaps

static float S(float v) { return v * g_dpiScale; }

// ------------------------------------------------------------------ actions
static void actApplyTweak(const string& id) {
    wstring err;
    if (tweaks::apply(id, err)) { g.status = L"Applied: " + widen(id); g.statusKind = 1; }
    else { g.status = L"Failed: " + widen(id) + L" — " + err; g.statusKind = 2; }
    g.sysDirty = true;
}
static void actRestoreTweak(const string& id) {
    wstring err;
    if (tweaks::restore(id, err)) { g.status = L"Restored default: " + widen(id); g.statusKind = 1; }
    else { g.status = L"Restore failed: " + widen(id) + L" — " + err; g.statusKind = 2; }
    g.sysDirty = true;
}
static void actProfile(const string& name) {
    auto rep = engine::runProfile(name);
    g.status = L"Profile '" + widen(name) + L"': " + rep.summary;
    g.statusKind = rep.failed ? 2 : 1;
    g.sysDirty = true;
}
static void actClean() {
    uint64_t freed = cleaner::purge();
    cleaner::emptyRecycleBin();
    g.status = L"Cleanup done — " + fmtBytes(freed) + L" freed";
    g.statusKind = 1;
    g.drivesDirty = true;
}
static void actPings() {
    g.pingsBusy = true;
    std::thread([] {
        std::vector<PingRow> rows;
        json cfg = engine::loadConfig();
        for (auto& t : cfg["ping_targets"])
            rows.push_back({ widen(t.value("name", "?")), widen(t.value("host", "?")), true, false, -1 });
        {
            std::lock_guard<std::mutex> lk(g_pingMtx);
            g.pings = rows;
        }
        for (auto& r : rows) {
            auto res = ping::measure(r.host);
            r.testing = false; r.ok = res.ok; r.ms = res.ok ? (int)res.ms : -1;
            {
                std::lock_guard<std::mutex> lk(g_pingMtx);
                for (auto& gr : g.pings)
                    if (gr.host == r.host) { gr.testing = false; gr.ok = r.ok; gr.ms = r.ms; }
            }
        }
        g.pingsBusy = false;
    }).detach();
}
static void actAddTarget() {
    if (g.pingHostInput.empty()) return;
    if (engine::addPingTarget(g.pingHostInput, g.pingHostInput)) {
        g.status = L"Ping target added: " + g.pingHostInput; g.statusKind = 1;
        g.pingHostInput.clear();
        g.pingsDirty = true;
    } else { g.status = L"Could not add target (duplicate?)"; g.statusKind = 2; }
}
static void actRemoveTarget(const wstring& name) {
    engine::removePingTarget(name);
    g.status = L"Removed " + name; g.statusKind = 0;
    g.pingsDirty = true;
}
static void actRefreshProcs() {
    g.procsBusy = true;
    std::thread([] {
        auto p = gameboost::listProcesses();
        p.erase(std::remove_if(p.begin(), p.end(), [](auto& x) { return x.pid <= 4; }), p.end());
        g.procs = p;
        g.procsBusy = false;
    }).detach();
}
static void actRefreshSys() {
    g.si = sysinfo::collect();
    auto d = drivers::collect();
    g.gpuName = g.si.gpuName.empty() ? d.gpuName : g.si.gpuName;
    g.gpuDrv = d.driverVersion;
    g.gpuDate = d.driverDate;
    g.planName = sysinfo::activePowerPlanName();
    g.sysDirty = false;
}
static void actRefreshStartup() {
    g.startupBusy = true;
    std::thread([] {
        auto rows = startup::listEntries();
        { std::lock_guard<std::mutex> lk(g_dataMtx); g.startupRows = rows; }
        g.startupBusy = false;
    }).detach();
}
static void actToggleStartup(const startup::Entry& e, bool enable) {
    wstring err;
    if (startup::setEnabled(e.location, e.name, enable, err)) {
        g.status = (enable ? L"Enabled: " : L"Disabled: ") + e.name;
        g.statusKind = 1;
    } else {
        g.status = L"Startup change failed: " + err;
        g.statusKind = 2;
    }
    g.startupDirty = true;
}
static void actRefreshDrives() {
    { std::lock_guard<std::mutex> lk(g_dataMtx); g.drives = storage::drivesJson(); }
    g.drivesDirty = false;
}
static void actLargestFiles(const wstring& root) {
    g.largestBusy = true;
    g.largestRoot = root;
    std::thread([root] {
        json r = storage::largestFiles(root, 14);
        { std::lock_guard<std::mutex> lk(g_dataMtx); g.largestFiles = r; }
        g.largestBusy = false;
    }).detach();
}
static void actRefreshFw() {
    g.fwBusy = true;
    std::thread([] {
        firmware::FwInit();
        json j = firmware::inventory();
        { std::lock_guard<std::mutex> lk(g_dataMtx); g.fw = j; }
        g.fwBusy = false;
    }).detach();
}
static void actRefreshDrv() {
    g.drvBusy = true;
    std::thread([] {
        drvupdate::DrvInit();
        json rep = drvupdate::report();
        json probs = drvupdate::problemDevices();
        { std::lock_guard<std::mutex> lk(g_dataMtx); g.drvRep = rep; g.drvProblems = probs; }
        g.drvBusy = false;
    }).detach();
}
static void actWuDriverScan() {
    g.status = L"Windows Update driver scan triggered — open the WU window to confirm";
    g.statusKind = 1;
    std::thread([] { drvupdate::scanWindowsUpdate(true); }).detach();
}
static void actBenchQuick() {
    if (g.benchBusy) return;
    g.benchBusy = true;
    g.status = L"Benchmark running (~4 s)…"; g.statusKind = 0;
    std::thread([] {
        json b = storage::runBenchmarkAll();
        g.lastBench = b;
        g.benchBusy = false;
        g.status = L"Benchmark done — score " + widen(std::to_string(b.value("total", 0)));
        g.statusKind = 1;
    }).detach();
}
static void saveSettings() {
    json cfg = engine::loadConfig();
    cfg["ui_confirm_actions"] = g.setConfirm;
    cfg["ui_autobackup"] = g.setAutoback;
    cfg["dns_managed"] = g.setDnsManaged;
    engine::saveConfig(cfg);
    g.status = L"Settings saved to config.json"; g.statusKind = 1;
}
static void loadSettings() {
    json cfg = engine::loadConfig();
    g.setConfirm = cfg.value("ui_confirm_actions", true);
    g.setAutoback = cfg.value("ui_autobackup", true);
    g.setDnsManaged = cfg.value("dns_managed", false);
}

// ------------------------------------------------------------------ d2d helpers
static D2D1_ROUNDED_RECT RR(D2D1_RECT_F r, float rad) {
    return D2D1::RoundedRect(r, rad, rad);
}
static D2D1_RECT_F Rc(float x, float y, float w, float h) {
    return D2D1::RectF(x, y, x + w, y + h);
}
static bool inRect(const D2D1_RECT_F& r, POINT p) {
    return p.x >= r.left && p.x <= r.right && p.y >= r.top && p.y <= r.bottom;
}
static bool clickedIn(const D2D1_RECT_F& r) {
    return g.clickAt.x >= 0 && inRect(r, g.clickAt);
}

static void drawText(const wstring& s, IDWriteTextFormat* f, const D2D1_RECT_F& r, const D2D1_COLOR_F& c) {
    ID2D1SolidColorBrush* b = nullptr;
    g_rt->CreateSolidColorBrush(c, &b);
    if (b) {
        g_rt->DrawTextW(s.c_str(), (UINT32)s.size(), f, r, b);
        b->Release();
    }
}
static void fillRect(const D2D1_RECT_F& r, const D2D1_COLOR_F& c) {
    ID2D1SolidColorBrush* b = nullptr;
    g_rt->CreateSolidColorBrush(c, &b);
    if (b) { g_rt->FillRectangle(r, b); b->Release(); }
}
static void fillRR(const D2D1_ROUNDED_RECT& r, const D2D1_COLOR_F& c) {
    ID2D1SolidColorBrush* b = nullptr;
    g_rt->CreateSolidColorBrush(c, &b);
    if (b) { g_rt->FillRoundedRectangle(r, b); b->Release(); }
}
static void strokeRR(const D2D1_ROUNDED_RECT& r, const D2D1_COLOR_F& c, float w = 1.0f) {
    ID2D1SolidColorBrush* b = nullptr;
    g_rt->CreateSolidColorBrush(c, &b);
    if (b) { g_rt->DrawRoundedRectangle(r, b, w); b->Release(); }
}
static void glassPanel(const D2D1_RECT_F& r, float rad = 14.0f) {
    auto rr = RR(r, rad);
    fillRR(rr, C_GLASS);
    ID2D1GradientStopCollection* gs = nullptr;
    D2D1_GRADIENT_STOP stops[2] = {
        { 0.0f, { 1,1,1, 0.10f } },
        { 1.0f, { 1,1,1, 0.0f } },
    };
    if (SUCCEEDED(g_rt->CreateGradientStopCollection(stops, 2, &gs))) {
        ID2D1LinearGradientBrush* lb = nullptr;
        D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES lp{};
        lp.startPoint = { r.left, r.top };
        lp.endPoint   = { r.left, r.top + (r.bottom - r.top) * 0.5f };
        if (SUCCEEDED(g_rt->CreateLinearGradientBrush(lp, gs, &lb))) {
            g_rt->FillRoundedRectangle(rr, lb);
            lb->Release();
        }
        gs->Release();
    }
    strokeRR(rr, C_GLASSBR);
}
static void drawDot(float x, float y, float rad, const D2D1_COLOR_F& c) {
    ID2D1SolidColorBrush* b = nullptr;
    g_rt->CreateSolidColorBrush(c, &b);
    if (b) { g_rt->FillEllipse(D2D1::Ellipse({ x, y }, rad, rad), b); b->Release(); }
}

// glass button; returns true if clicked this frame
static bool button(const D2D1_RECT_F& r, const wstring& label, bool accent = false, bool enabled = true, IDWriteTextFormat* f = nullptr) {
    bool hover = inRect(r, g.mouse) && enabled;
    bool down  = hover && g.mouseDown;
    auto rr = RR(r, 10);
    D2D1_COLOR_F fill = accent ? D2D1_COLOR_F{ 0.30f, 0.62f, 1.0f, hover ? 0.95f : 0.80f }
                               : D2D1_COLOR_F{ 1,1,1, hover ? 0.14f : (down ? 0.10f : 0.07f) };
    fillRR(rr, fill);
    if (accent) strokeRR(rr, D2D1_COLOR_F{ 1,1,1, 0.25f });
    else        strokeRR(rr, D2D1_COLOR_F{ 1,1,1, hover ? 0.30f : 0.14f });
    D2D1_COLOR_F tc = enabled ? (accent ? D2D1_COLOR_F{ 0.02f,0.05f,0.12f,1 } : C_TEXT)
                              : D2D1_COLOR_F{ 1,1,1, 0.25f };
    auto fmt = f ? f : g_fBody;
    D2D1_RECT_F tr = r; tr.left += 12; tr.right -= 12;
    drawText(label, fmt, tr, tc);
    return enabled && clickedIn(r);
}

// small glass switch showing a boolean state; returns true when clicked
static bool switchRow(const D2D1_RECT_F& r, const wstring& label, const wstring& sub, bool on, bool adminReq = false) {
    bool hover = inRect(r, g.mouse);
    if (hover) fillRect(r, D2D1_COLOR_F{ 1,1,1,0.04f });
    float sw = S(34), sh = S(18);
    D2D1_RECT_F swr = Rc(r.right - sw - S(12), r.top + (r.bottom - r.top - sh) / 2, sw, sh);
    D2D1_COLOR_F onC = { 0.30f,0.62f,1.0f,0.9f };
    fillRR(RR(swr, sh / 2), on ? onC : D2D1_COLOR_F{ 1,1,1,0.10f });
    strokeRR(RR(swr, sh / 2), on ? D2D1_COLOR_F{ 1,1,1,0.35f } : C_GLASSBR);
    float knobR = sh * 0.42f;
    float kx = on ? swr.right - sh / 2 : swr.left + sh / 2;
    drawDot(kx, swr.top + sh / 2, knobR, on ? D2D1_COLOR_F{ 0.05f,0.08f,0.16f,1 } : C_DIM);
    D2D1_RECT_F tr = r; tr.right = swr.left - S(10);
    drawText(label, g_fBody, { tr.left + S(8), r.top + S(5), tr.right, r.top + S(24) }, adminReq ? C_TEXT : C_TEXT);
    if (!sub.empty())
        drawText(sub, g_fSmall, { tr.left + S(8), r.top + S(25), tr.right, r.bottom }, C_DIM);
    if (adminReq)
        drawText(L"ADMIN", g_fSmall, { r.right - sw - S(70), r.top + S(6), r.right - sw - S(16), r.top + S(22) }, C_WARN);
    return clickedIn(r);
}

static bool checkboxRow(D2D1_RECT_F& r, const wstring& label, bool& checked, bool adminReq) {
    bool hover = inRect(r, g.mouse);
    if (hover) fillRect(r, D2D1_COLOR_F{ 1,1,1,0.04f });
    float bs = S(18);
    D2D1_RECT_F box = Rc(r.left + S(6), r.top + (r.bottom - r.top - bs) / 2, bs, bs);
    fillRR(RR(box, 5), checked ? D2D1_COLOR_F{ 0.30f,0.62f,1.0f,0.9f } : D2D1_COLOR_F{ 1,1,1,0.08f });
    strokeRR(RR(box, 5), checked ? D2D1_COLOR_F{ 1,1,1,0.4f } : C_GLASSBR);
    if (checked) {
        ID2D1SolidColorBrush* b = nullptr;
        g_rt->CreateSolidColorBrush(D2D1_COLOR_F{ 0.05f,0.08f,0.16f,1 }, &b);
        if (b) {
            g_rt->DrawLine({ box.left + bs * 0.22f, box.top + bs * 0.52f },
                           { box.left + bs * 0.42f, box.top + bs * 0.75f }, b, S(2.2f));
            g_rt->DrawLine({ box.left + bs * 0.42f, box.top + bs * 0.75f },
                           { box.left + bs * 0.80f, box.top + bs * 0.25f }, b, S(2.2f));
            b->Release();
        }
    }
    D2D1_RECT_F tr = r;
    tr.left += bs + S(16);
    drawText(label, g_fBody, tr, adminReq ? C_TEXT : C_DIM);
    return clickedIn(r);
}

// ------------------------------------------------------------------ background
static void drawBackground(float t) {
    g_rt->Clear(C_BASE);
    struct Blob { float cx, cy, r; D2D1_COLOR_F c; float sx, sy, ph; };
    Blob blobs[3] = {
        { 0.25f, 0.30f, 0.55f, { 0.35f, 0.25f, 0.95f, 0.16f }, 0.11f, 0.07f, 0.0f },
        { 0.75f, 0.25f, 0.50f, { 0.90f, 0.25f, 0.60f, 0.11f }, 0.08f, 0.12f, 2.1f },
        { 0.55f, 0.85f, 0.60f, { 0.15f, 0.60f, 0.95f, 0.13f }, 0.09f, 0.06f, 4.2f },
    };
    for (auto& b : blobs) {
        float cx = (b.cx + b.sx * sinf(t * 0.35f + b.ph)) * g_sz.width;
        float cy = (b.cy + b.sy * cosf(t * 0.27f + b.ph)) * g_sz.height;
        float rad = b.r * std::min(g_sz.width, g_sz.height);
        ID2D1GradientStopCollection* gs = nullptr;
        D2D1_GRADIENT_STOP stops[2] = { { 0.0f, b.c }, { 1.0f, D2D1_COLOR_F{ 0,0,0,0 } } };
        if (SUCCEEDED(g_rt->CreateGradientStopCollection(stops, 2, &gs))) {
            ID2D1RadialGradientBrush* rb = nullptr;
            D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES rp{};
            rp.center = { cx, cy };
            rp.gradientOriginOffset = { 0, 0 };
            rp.radiusX = rad; rp.radiusY = rad;
            if (SUCCEEDED(g_rt->CreateRadialGradientBrush(rp, gs, &rb))) {
                g_rt->FillRectangle(D2D1::RectF(0, 0, g_sz.width, g_sz.height), rb);
                rb->Release();
            }
            gs->Release();
        }
    }
}

// ------------------------------------------------------------------ layout
static float sidebarX() { return S(210); }

static void drawSidebar() {
    static const struct { Tab t; const wchar_t* name; } tabs[T_COUNT] = {
        { T_DASH,     L"Dashboard"   },
        { T_TWEAKS,   L"Tweaks"      },
        { T_GAMING,   L"Gaming"      },
        { T_FIRMWARE, L"Firmware"    },
        { T_STORAGE,  L"Storage"     },
        { T_STARTUP,  L"Startup"     },
        { T_SETTINGS, L"Settings"    },
        { T_PRIVACY,  L"Privacy"     },
        { T_DRIVERS,  L"Drivers"     },
        { T_PING,     L"Network"     },
        { T_LOGS,     L"Logs"        },
        { T_ABOUT,    L"About"       },
    };
    float x = S(16), y = S(72), w = sidebarX() - S(28);
    for (int i = 0; i < T_COUNT; ++i) {
        D2D1_RECT_F r = Rc(x, y, w, S(34));
        bool active = g.tab == tabs[i].t;
        bool hover = inRect(r, g.mouse);
        if (active) {
            auto rr = RR(r, 10);
            fillRR(rr, D2D1_COLOR_F{ 0.30f, 0.62f, 1.0f, 0.22f });
            strokeRR(rr, D2D1_COLOR_F{ 0.30f, 0.62f, 1.0f, 0.55f });
        } else if (hover) {
            fillRR(RR(r, 10), D2D1_COLOR_F{ 1,1,1,0.05f });
        }
        D2D1_RECT_F tr = r; tr.left += S(14);
        drawText(tabs[i].name, g_fBody, tr, active ? C_TEXT : (hover ? C_TEXT : C_DIM));
        if (clickedIn(r)) g.tab = tabs[i].t;
        y += S(39);
    }
    D2D1_RECT_F r = Rc(S(16), g_sz.height - S(56), w, S(40));
    glassPanel(r, 10);
    drawText(isAdmin() ? L"● Elevated (admin)" : L"○ Standard user",
             g_fSmall, { r.left + S(10), r.top + S(10), r.right, r.bottom },
             isAdmin() ? C_OK : C_DIM);
}

static void drawTitlebar() {
    drawText(L"OPTIMIZEKIT", g_fTitle, { S(20), S(14), sidebarX(), S(52) }, C_TEXT);
    drawDot(S(20) - S(10), S(30) * 1.0f, S(5), C_ACC);

    float bw = S(40), bh = S(28);
    D2D1_RECT_F bmin = Rc(g_sz.width - bw * 2 - S(10), S(10), bw, bh);
    D2D1_RECT_F bcls = Rc(g_sz.width - bw - S(10), S(10), bw, bh);
    if (inRect(bmin, g.mouse)) fillRect(bmin, D2D1_COLOR_F{ 1,1,1,0.10f });
    if (inRect(bcls, g.mouse)) fillRect(bcls, D2D1_COLOR_F{ 1.0f,0.35f,0.40f,0.85f });
    drawText(L"—", g_fSmall, bmin, C_DIM);
    drawText(L"✕", g_fSmall, bcls, inRect(bcls, g.mouse) ? D2D1_COLOR_F{ 1,1,1,1 } : C_DIM);
    if (clickedIn(bmin)) PostMessageW(g_hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
    if (clickedIn(bcls)) PostMessageW(g_hwnd, WM_CLOSE, 0, 0);
}

static D2D1_RECT_F contentRect() {
    return { sidebarX() + S(14), S(64), g_sz.width - S(18), g_sz.height - S(46) };
}

static void drawStatusbar() {
    D2D1_RECT_F r = Rc(sidebarX() + S(14), g_sz.height - S(40), g_sz.width - sidebarX() - S(32), S(28));
    D2D1_COLOR_F c = g.statusKind == 1 ? C_OK : g.statusKind == 2 ? C_ERR : C_DIM;
    fillRR(RR(r, 8), D2D1_COLOR_F{ 1,1,1,0.05f });
    drawText(g.status, g_fSmall, { r.left + S(10), r.top + S(5), r.right, r.bottom }, c);
}

// shared: one key/value card
static void kvCard(const D2D1_RECT_F& r, const wstring& k, const wstring& v, const D2D1_COLOR_F& vc = C_TEXT) {
    glassPanel(r, 10);
    drawText(k, g_fSmall, { r.left + S(14), r.top + S(6), r.right - S(10), r.top + S(22) }, C_FAINT);
    drawText(v, g_fBody, { r.left + S(14), r.top + S(20), r.right - S(10), r.bottom }, vc);
}

// ---- dashboard
static void drawDashboard() {
    auto cr = contentRect();
    if (g.sysDirty) actRefreshSys();
    float x = cr.left, y = cr.top + S(6);

    D2D1_RECT_F hero = Rc(x, y, cr.right - cr.left, S(110));
    glassPanel(hero, 16);
    drawText(L"Optimize your PC — natively.", g_fH1, { x + S(22), y + S(14), x + S(560), y + S(58) }, C_TEXT);
    drawText(L"Gaming · Privacy · Firmware · Storage — one exe, zero browser, fully logged.",
             g_fBody, { x + S(22), y + S(58), x + S(640), y + S(86) }, C_DIM);
    D2D1_RECT_F b1 = Rc(x + S(660), y + S(32), S(160), S(40));
    D2D1_RECT_F b2 = Rc(x + S(830), y + S(32), S(140), S(40));
    if (button(b1, L"⚡ Quick Optimize", true)) actProfile("gaming");
    if (button(b2, L"🧹 Clean junk")) actClean();
    y += S(126);

    struct Row { wstring k, v; };
    vector<Row> rows = {
        { L"OS",           g.si.osName + L" build " + g.si.osBuild + L" (" + g.si.osArch + L")" },
        { L"CPU",          g.si.cpuName + L"  ·  " + std::to_wstring(g.si.cpuCores) + L" threads" },
        { L"GPU",          g.gpuName + (g.gpuDrv.empty() ? L"" : L"  ·  driver " + g.gpuDrv) },
        { L"RAM",          fmtBytes(g.si.ramTotal) + L" total · " + fmtBytes(g.si.ramAvail) + L" available" },
        { L"Power plan",   g.planName },
        { L"Game Mode",    g.si.gameModeOn ? L"On" : L"Off" },
        { L"GPU scheduling", g.si.hagsOn ? L"On" : L"Off" },
        { L"Uptime",       g.si.uptime },
    };
    float colW = (cr.right - cr.left - S(12)) / 2;
    float rowH = S(44);
    for (size_t i = 0; i < rows.size(); ++i) {
        float cx = x + (i % 2) * (colW + S(12));
        float cy = y + (i / 2) * (rowH + S(8));
        kvCard(Rc(cx, cy, colW, rowH), rows[i].k, rows[i].v);
    }
    y += (rows.size() / 2) * (rowH + S(8)) + S(14);

    // benchmark + quick actions row
    {
        D2D1_RECT_F r = Rc(x, y, cr.right - cr.left, S(96));
        glassPanel(r, 14);
        drawText(L"Benchmark & actions", g_fH2, { x + S(18), y + S(10), x + S(420), y + S(38) }, C_TEXT);
        wstring bench = g.benchBusy ? L"measuring…"
            : (g.lastBench.contains("total") ? (L"last score: " + widen(std::to_string(g.lastBench["total"].get<int>()))) : L"not measured yet");
        drawText(bench, g_fSmall, { x + S(18), y + S(40), x + S(300), y + S(60) }, C_ACC);
        D2D1_RECT_F q1 = Rc(x + S(320), y + S(28), S(170), S(38));
        D2D1_RECT_F q2 = Rc(x + S(500), y + S(28), S(190), S(38));
        D2D1_RECT_F q3 = Rc(x + S(700), y + S(28), S(190), S(38));
        D2D1_RECT_F q4 = Rc(x + S(900), y + S(28), S(170), S(38));
        if (button(q1, L"▲ Run benchmark", true, !g.benchBusy)) actBenchQuick();
        if (button(q2, L"⚡ Apply GAMING profile")) actProfile("gaming");
        if (button(q3, L"🛡 Apply PRIVACY profile")) actProfile("privacy");
        if (button(q4, L"Open log folder")) shellOpen(appDataDir());
        y += S(110);
    }
}

// ---- tweaks
static void drawTweaks() {
    auto cr = contentRect();
    float x = cr.left, y = cr.top + S(6);
    float w = cr.right - cr.left;

    drawText(L"Tweaks", g_fH1, { x, y, x + S(300), y + S(34) }, C_TEXT);
    y += S(40);

    static const wchar_t* pills[] = { L"All", L"Gaming", L"Privacy", L"Debloat" };
    float px = x;
    for (int i = 0; i < 4; ++i) {
        D2D1_RECT_F r = Rc(px, y, S(92), S(30));
        bool active = g.tweakFilter == i;
        if (active) fillRR(RR(r, 15), D2D1_COLOR_F{ 0.30f,0.62f,1.0f,0.85f });
        else fillRR(RR(r, 15), D2D1_COLOR_F{ 1,1,1,0.07f });
        strokeRR(RR(r, 15), active ? D2D1_COLOR_F{ 1,1,1,0.3f } : C_GLASSBR);
        drawText(pills[i], g_fSmall, r, active ? D2D1_COLOR_F{ 0.02f,0.05f,0.12f,1 } : C_DIM);
        if (clickedIn(r)) g.tweakFilter = i;
        px += S(100);
    }
    y += S(42);

    D2D1_RECT_F ba = Rc(x, y, S(200), S(36));
    D2D1_RECT_F br = Rc(x + S(210), y, S(220), S(36));
    D2D1_RECT_F bs = Rc(x + w - S(190), y, S(190), S(36));
    if (button(ba, L"✔ Apply selected", true)) {
        json sel = json::object();
        for (auto& t : catalog()) if (g.sel[t.id]) sel[t.id] = true;
        int n = 0; vector<wstring> errs;
        tweaks::backupAll();
        n = tweaks::applyMany(sel, errs);
        g.status = n ? (L"Applied " + std::to_wstring(n) + L" tweaks") : L"Nothing selected";
        g.statusKind = n ? 1 : 0;
        g.sysDirty = true;
    }
    if (button(br, L"↺ Restore selected")) {
        int n = 0;
        for (auto& t : catalog()) {
            if (g.sel[t.id]) { wstring e; if (tweaks::restore(t.id, e)) n++; }
        }
        g.status = L"Restored " + std::to_wstring(n) + L" defaults"; g.statusKind = 1;
        g.sysDirty = true;
    }
    if (button(bs, L"Select all / none")) {
        bool any = false;
        for (auto& t : catalog()) if (g.sel[t.id]) any = true;
        for (auto& t : catalog()) g.sel[t.id] = !any;
    }
    y += S(48);

    float listTop = y, listBottom = cr.bottom - S(8);
    float scroll = g.scroll["tweaks"];
    float cy = listTop - scroll;

    const auto& cat = catalog();
    float rowH = S(56);
    for (auto& t : cat) {
        bool pass = g.tweakFilter == 0
            || (g.tweakFilter == 1 && t.impact >= 2)
            || (g.tweakFilter == 2 && (t.id.find("telemetry") != string::npos || t.id.find("advertising") != string::npos
                || t.id.find("activity") != string::npos || t.id.find("bing") != string::npos
                || t.id.find("tailored") != string::npos || t.id.find("copilot") != string::npos
                || t.id.find("edge") != string::npos || t.id.find("brave") != string::npos
                || t.id.find("location") != string::npos))
            || (g.tweakFilter == 3 && (t.id.find("bloat") != string::npos || t.id.find("onedrive") != string::npos
                || t.id.find("xbox") != string::npos || t.id.find("sysmain") != string::npos
                || t.id.find("widgets") != string::npos || t.id.find("consumer") != string::npos
                || t.id.find("razer") != string::npos));
        if (!pass) continue;

        if (cy + rowH < listTop || cy > listBottom) { cy += rowH + S(8); continue; }
        D2D1_RECT_F r = Rc(x, cy, w, rowH);
        glassPanel(r, 10);
        D2D1_RECT_F boxRect = { r.left + S(10), r.top, r.left + S(30), r.bottom };
        bool hit = checkboxRow(boxRect, L"", g.sel[t.id], t.admin);
        if (hit) g.sel[t.id] = !g.sel[t.id];
        D2D1_RECT_F tr = { r.left + S(56), r.top + S(8), r.right - S(180), r.bottom };
        drawText(t.name, g_fBody, { tr.left, tr.top, tr.right, tr.top + S(22) }, C_TEXT);
        drawText(t.desc, g_fSmall, { tr.left, tr.top + S(24), tr.right, tr.bottom }, C_DIM);
        wstring badge = t.admin ? L"ADMIN" : L"USER";
        D2D1_COLOR_F bc = t.admin ? C_WARN : C_OK;
        D2D1_RECT_F brr = { r.right - S(170), r.top + S(10), r.right - S(96), r.top + S(28) };
        fillRR(RR(brr, 9), D2D1_COLOR_F{ bc.r, bc.g, bc.b, 0.18f });
        drawText(badge, g_fSmall, brr, bc);
        wstring stars;
        for (int i = 0; i < t.impact; ++i) stars += L"★";
        drawText(stars, g_fSmall, { r.right - S(88), r.top + S(8), r.right - S(10), r.top + S(28) }, C_ACC2);
        D2D1_RECT_F applyB = { r.right - S(88), r.top + S(30), r.right - S(10), r.top + S(50) };
        if (button(applyB, L"apply", false, true, g_fSmall)) actApplyTweak(t.id);

        cy += rowH + S(8);
    }
    g.scroll["tweaks"] = scroll;
}

// ---- gaming
static void drawGaming() {
    auto cr = contentRect();
    float x = cr.left, y = cr.top + S(6);
    float w = cr.right - cr.left;

    drawText(L"Gaming", g_fH1, { x, y, x + S(300), y + S(34) }, C_TEXT);
    y += S(44);

    D2D1_RECT_F p = Rc(x, y, w, S(190));
    glassPanel(p, 14);
    drawText(L"One-click gaming profile", g_fH2, { x + S(18), y + S(12), x + S(500), y + S(42) }, C_TEXT);
    drawText(L"Game DVR off · network stack · timer 0.5ms · HAGS · MPO fix · Ultimate power plan.",
             g_fSmall, { x + S(18), y + S(44), x + w - S(20), y + S(66) }, C_DIM);
    D2D1_RECT_F b1 = Rc(x + S(18), y + S(78), S(220), S(42));
    if (button(b1, L"⚡ Apply GAMING profile", true)) actProfile("gaming");
    drawText(L"Frees FPS and cuts latency. Some tweaks need admin and a reboot.",
             g_fSmall, { x + S(18), y + S(134), x + w - S(20), y + S(160) }, C_FAINT);
    y += S(204);

    struct Quick { const char* id; const wchar_t* label; };
    static const Quick quicks[] = {
        { "game_dvr_off",   L"Game DVR off" },
        { "network_gaming", L"Gaming network" },
        { "timer_high",     L"Timer 0.5ms" },
        { "hags_on",        L"HAGS" },
        { "mpo_off",        L"MPO fix" },
        { "power_ultimate", L"Ultimate plan" },
        { "mouse_precision",L"Raw mouse" },
        { "end_task_menu",  L"End-task menu" },
        { "sticky_keys_off",L"Sticky keys off" },
        { "notifications_off", L"No notifications" },
        { "game_bar_off",   L"Game Bar off" },
        { "xbox_live_off",  L"Xbox services off" },
    };
    float bw = (w - S(8) * 3) / 4;
    for (int i = 0; i < 12; ++i) {
        float bx = x + (i % 4) * (bw + S(8));
        float by = y + (i / 4) * S(48);
        D2D1_RECT_F r = Rc(bx, by, bw, S(40));
        if (button(r, quicks[i].label)) actApplyTweak(quicks[i].id);
    }
    y += S(168);

    D2D1_RECT_F pr = Rc(x, y, w, cr.bottom - y - S(4));
    glassPanel(pr, 14);
    drawText(L"Running processes — boost priority or kill", g_fH2, { x + S(18), y + S(12), x + S(600), y + S(42) }, C_TEXT);
    D2D1_RECT_F rb = Rc(x + w - S(130), y + S(12), S(110), S(32));
    if (button(rb, L"↻ Refresh")) { g.procsDirty = true; actRefreshProcs(); }
    if (g.procsDirty && !g.procsBusy) { g.procsDirty = false; actRefreshProcs(); }

    float ly = y + S(56);
    float lh = S(30);
    float scroll = g.scroll["procs"];
    auto clip = pr;
    g_rt->PushAxisAlignedClip(clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    float py = ly - scroll;
    for (auto& pr2 : g.procs) {
        if (py > pr.bottom) break;
        if (py + lh > pr.top + S(50)) {
            D2D1_RECT_F row = { pr.left + S(10), py, pr.right - S(10), py + lh };
            if (inRect(row, g.mouse)) fillRect(row, D2D1_COLOR_F{ 1,1,1,0.05f });
            wchar_t pid[16]; swprintf(pid, 16, L"%lu", pr2.pid);
            drawText(pr2.name + L"   (" + pid + L")", g_fSmall,
                     { row.left + S(8), py + S(5), row.right - S(260), py + lh }, C_TEXT);
            D2D1_RECT_F hb = { row.right - S(250), py + S(3), row.right - S(160), py + lh - S(3) };
            D2D1_RECT_F kb = { row.right - S(150), py + S(3), row.right - S(70), py + lh - S(3) };
            if (button(hb, L"High", false, true, g_fSmall)) {
                wstring e;
                if (!gameboost::setPriority(pr2.pid, 5, e)) { g.status = e; g.statusKind = 2; }
                else { g.status = L"High priority set for " + pr2.name; g.statusKind = 1; }
            }
            if (button(kb, L"Kill", false, true, g_fSmall)) {
                wstring e;
                if (!gameboost::killProcess(pr2.pid, e)) { g.status = e; g.statusKind = 2; }
                g.procsDirty = true;
            }
        }
        py += lh;
    }
    g_rt->PopAxisAlignedClip();
    g.scroll["procs"] = scroll;
}

// ---- firmware
static void drawFirmware() {
    auto cr = contentRect();
    float x = cr.left, y = cr.top + S(6);
    float w = cr.right - cr.left;

    drawText(L"Firmware & platform", g_fH1, { x, y, x + S(460), y + S(34) }, C_TEXT);
    D2D1_RECT_F rb = Rc(x + w - S(150), y + S(2), S(150), S(32));
    if (button(rb, g.fwBusy ? L"Reading…" : L"↻ Read platform", !g.fwBusy) && !g.fwBusy) actRefreshFw();
    if (g.fwDirty && !g.fwBusy) { g.fwDirty = false; actRefreshFw(); }
    y += S(44);

    if (g.fwBusy && !g.fw.contains("secureBoot")) {
        drawText(L"Reading BIOS, SecureBoot, TPM and kernel options…", g_fBody,
                 { x + S(8), y + S(8), x + w, y + S(36) }, C_DIM);
        return;
    }
    if (!g.fw.contains("secureBoot")) return;

    json& f = g.fw;
    auto sv = [&](const char* k, const wstring& def = L"?") -> wstring {
        return f.contains(k) && !f[k].is_null() ? widen(f[k].get<std::string>()) : def;
    };
    auto stateColor = [](const wstring& s, const wstring& good) {
        return s.find(good) != wstring::npos ? C_OK : C_WARN;
    };

    // row 1: identity
    {
        float cw = (w - S(24)) / 3;
        kvCard(Rc(x, y, cw, S(64)), L"BIOS", sv("biosVendor") + L"  " + sv("biosVersion") + L"  (" + sv("biosDate") + L")");
        kvCard(Rc(x + cw + S(12), y, cw, S(64)), L"Motherboard", sv("motherboard"));
        wstring boot = sv("bootMode") + L" · SecureBoot " + sv("secureBoot");
        kvCard(Rc(x + (cw + S(12)) * 2, y, cw, S(64)), L"Boot", boot,
               stateColor(sv("secureBoot"), L"on"));
        y += S(76);
    }
    // row 2: security
    {
        float cw = (w - S(24)) / 3;
        json& tpm = f["tpm"];
        wstring tpmS = tpm.value("present", false)
            ? (L"TPM " + widen(tpm.value("version", std::string("2.0")))) : L"absent";
        kvCard(Rc(x, y, cw, S(64)), L"TPM", tpmS, tpm.value("present", false) ? C_OK : C_ERR);
        wstring vt = sv("virtualization") + (f.value("hypervisorRunning", false) ? L" · hypervisor on" : L"");
        kvCard(Rc(x + cw + S(12), y, cw, S(64)), L"Virtualization", vt,
               stateColor(sv("virtualization"), L"on"));
        wstring standby = sv("modernStandby") + L" · " + sv("deviceType");
        kvCard(Rc(x + (cw + S(12)) * 2, y, cw, S(64)), L"Power profile", standby);
        y += S(76);
    }
    // row 3: kernel options (what our tweaks changed, read live)
    {
        float cw = (w - S(24)) / 4;
        kvCard(Rc(x, y, cw, S(58)), L"HPET", sv("hpet"), stateColor(sv("hpet"), L"on"));
        kvCard(Rc(x + cw + S(8), y, cw, S(58)), L"WPBT", sv("wpbt"));
        kvCard(Rc(x + (cw + S(8)) * 2, y, cw, S(58)), L"Dynamic tick", sv("dynamicTick"));
        kvCard(Rc(x + (cw + S(8)) * 3, y, cw, S(58)), L"Platform tick", sv("useplatformtick"));
        y += S(70);
    }
    // note panel
    {
        D2D1_RECT_F r = Rc(x, y, w, S(88));
        glassPanel(r, 12);
        drawText(L"This panel is read-only — OptimizeKit never writes to your firmware.", g_fBody,
                 { x + S(18), y + S(12), x + w - S(20), y + S(38) }, C_TEXT);
        drawText(L"The BIOS guide (web dashboard or About) explains XMP/EXPO, ReBAR, Above-4G, C-states and fan curves,",
                 g_fSmall, { x + S(18), y + S(40), x + w - S(20), y + S(60) }, C_DIM);
        drawText(L"and every change is done by you, in the vendor setup, at the brand logo (Del / F2).",
                 g_fSmall, { x + S(18), y + S(58), x + w - S(20), y + S(78) }, C_DIM);
    }
}

// ---- storage
static void drawStorage() {
    auto cr = contentRect();
    float x = cr.left, y = cr.top + S(6);
    float w = cr.right - cr.left;

    drawText(L"Storage", g_fH1, { x, y, x + S(300), y + S(34) }, C_TEXT);
    D2D1_RECT_F rf = Rc(x + w - S(130), y + S(2), S(130), S(32));
    if (button(rf, L"↻ Refresh")) g.drivesDirty = true;
    y += S(44);

    if (g.drivesDirty) actRefreshDrives();

    // drives
    {
        std::lock_guard<std::mutex> lk(g_dataMtx);
        if (g.drives.is_array()) {
            int n = (int)g.drives.size();
            float cw = (std::min(n, 4) > 0) ? (w - S(12) * (std::min(n, 4) - 1)) / std::min(n, 4) : w;
            int i = 0;
            for (auto& d : g.drives) {
                if (i >= 4) break;
                wstring letter = widen(d.value("letter", std::string("?")));
                uint64_t total = d.value("total", (uint64_t)0), freeq = d.value("free", (uint64_t)0);
                uint64_t used = total > freeq ? total - freeq : 0;
                float pct = total ? (float)((double)used / (double)total * 100.0) : 0.f;
                D2D1_RECT_F r = Rc(x + i * (cw + S(12)), y, cw, S(86));
                glassPanel(r, 12);
                drawText(letter + L"  " + widen(d.value("label", std::string(""))), g_fBody,
                         { r.left + S(12), r.top + S(8), r.right - S(10), r.top + S(30) }, C_TEXT);
                drawText(fmtBytes(used) + L" / " + fmtBytes(total), g_fSmall,
                         { r.left + S(12), r.top + S(32), r.right - S(10), r.top + S(50) }, C_DIM);
                // usage bar
                D2D1_RECT_F bar = { r.left + S(12), r.top + S(58), r.right - S(12), r.top + S(68) };
                fillRR(RR(bar, 4), D2D1_COLOR_F{ 1,1,1,0.08f });
                D2D1_RECT_F fillB = bar; fillB.right = bar.left + (bar.right - bar.left) * (pct / 100.0f);
                D2D1_COLOR_F c = pct > 90 ? C_ERR : pct > 75 ? C_WARN : C_OK;
                if (fillB.right > fillB.left) fillRR(RR(fillB, 4), c);
                drawText(widen(d.value("bus", std::string(""))), g_fSmall,
                         { r.left + S(12), r.top + S(68), r.right - S(10), r.top + S(86) }, C_FAINT);
                ++i;
            }
            y += S(100);
        }
    }

    // actions
    D2D1_RECT_F b1 = Rc(x, y, S(190), S(38));
    D2D1_RECT_F b2 = Rc(x + S(200), y, S(190), S(38));
    D2D1_RECT_F b3 = Rc(x + S(400), y, S(230), S(38));
    if (button(b1, L"🧹 Clean junk now", true)) actClean();
    if (button(b2, L"⏬ Temp files tweak")) actApplyTweak("temp_files");
    if (button(b3, L"🧰 Disk cleanup + WinSxS")) actApplyTweak("disk_cleanup");
    y += S(52);

    // largest files
    D2D1_RECT_F p = Rc(x, y, w, cr.bottom - y - S(4));
    glassPanel(p, 14);
    drawText(L"Largest files on " + g.largestRoot, g_fH2, { x + S(18), y + S(12), x + S(420), y + S(40) }, C_TEXT);
    D2D1_RECT_F cb = Rc(x + w - S(150), y + S(12), S(130), S(30));
    if (button(cb, g.largestBusy ? L"Scanning…" : L"Scan " + g.largestRoot, false, !g.largestBusy, g_fSmall) && !g.largestBusy)
        actLargestFiles(g.largestRoot);
    D2D1_RECT_F swC = Rc(x + w - S(300), y + S(12), S(140), S(30));
    if (button(swC, g.largestRoot == L"C:\\" ? L"Switch to D:\\" : L"Switch to C:\\", false, true, g_fSmall))
        actLargestFiles(g.largestRoot == L"C:\\" ? L"D:\\" : L"C:\\");
    g_rt->PushAxisAlignedClip(p, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    float ry = y + S(52) - g.scroll["storage"];
    {
        std::lock_guard<std::mutex> lk(g_dataMtx);
        if (g.largestFiles.is_array()) {
            for (auto& f2 : g.largestFiles) {
                if (ry > p.bottom) break;
                if (ry + S(26) > p.top + S(44)) {
                    uint64_t szb = f2.value("bytes", (uint64_t)0);
                    wstring path = widen(f2.value("path", std::string("")));
                    drawText(fmtBytes(szb), g_fMono, { p.left + S(16), ry, p.left + S(120), ry + S(22) }, C_ACC);
                    drawText(path, g_fSmall, { p.left + S(130), ry, p.right - S(16), ry + S(22) }, C_DIM);
                }
                ry += S(26);
            }
        } else {
            drawText(L"Press Scan to walk the drive (bounded, read-only).", g_fSmall,
                     { p.left + S(16), ry, p.right - S(16), ry + S(22) }, C_FAINT);
        }
    }
    g_rt->PopAxisAlignedClip();
}

// ---- startup
static void drawStartup() {
    auto cr = contentRect();
    float x = cr.left, y = cr.top + S(6);
    float w = cr.right - cr.left;

    drawText(L"Startup apps", g_fH1, { x, y, x + S(300), y + S(34) }, C_TEXT);
    D2D1_RECT_F rb = Rc(x + w - S(130), y + S(2), S(130), S(32));
    if (button(rb, g.startupBusy ? L"Reading…" : L"↻ Refresh", !g.startupBusy) && !g.startupBusy)
        actRefreshStartup();
    if (g.startupDirty && !g.startupBusy) { g.startupDirty = false; actRefreshStartup(); }
    y += S(44);

    D2D1_RECT_F listP = Rc(x, y, w, cr.bottom - y - S(4));
    glassPanel(listP, 14);
    g_rt->PushAxisAlignedClip(listP, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    float ry = listP.top + S(12) - g.scroll["startup"];
    std::lock_guard<std::mutex> lk(g_dataMtx);
    for (auto& e : g.startupRows) {
        if (ry > listP.bottom) break;
        if (ry + S(34) > listP.top + S(6)) {
            D2D1_RECT_F row = { listP.left + S(10), ry, listP.right - S(10), ry + S(32) };
            if (inRect(row, g.mouse)) fillRect(row, D2D1_COLOR_F{ 1,1,1,0.05f });
            drawText(e.name, g_fBody, { row.left + S(8), ry + S(5), row.left + S(320), ry + S(28) }, C_TEXT);
            drawText(e.command, g_fSmall, { row.left + S(330), ry + S(6), row.right - S(220), ry + S(26) }, C_FAINT);
            drawText(e.location, g_fSmall, { row.left + S(8), ry + S(24), row.left + S(330), ry + S(40) }, C_FAINT);
            D2D1_RECT_F tb = { row.right - S(110), ry + S(3), row.right - S(10), ry + S(29) };
            wstring tl = e.enabled ? L"Disable" : L"Enable";
            if (button(tb, tl, !e.enabled, true, g_fSmall)) actToggleStartup(e, !e.enabled);
        }
        ry += S(36);
    }
    g_rt->PopAxisAlignedClip();
}

// ---- settings
static void drawSettings() {
    auto cr = contentRect();
    float x = cr.left, y = cr.top + S(6);
    float w = cr.right - cr.left;

    drawText(L"Settings", g_fH1, { x, y, x + S(300), y + S(34) }, C_TEXT);
    y += S(48);

    // behavior
    {
        D2D1_RECT_F r = Rc(x, y, w, S(128));
        glassPanel(r, 14);
        drawText(L"Behavior", g_fH2, { x + S(18), y + S(10), x + S(400), y + S(38) }, C_TEXT);
        D2D1_RECT_F c1 = { x + S(18), y + S(44), x + S(520), y + S(68) };
        if (checkboxRow(c1, L"Confirm destructive actions before running them", g.setConfirm, false)) g.setConfirm = !g.setConfirm;
        D2D1_RECT_F c2 = { x + S(18), y + S(70), x + S(520), y + S(94) };
        if (checkboxRow(c2, L"Automatic registry backup before every tweak (recommended)", g.setAutoback, false)) g.setAutoback = !g.setAutoback;
        D2D1_RECT_F c3 = { x + S(18), y + S(96), x + S(520), y + S(120) };
        if (checkboxRow(c3, L"Allow gaming network profiles to set DNS (1.1.1.1 / 8.8.8.8)", g.setDnsManaged, false)) g.setDnsManaged = !g.setDnsManaged;
        y += S(142);
    }
    // backups + about data
    {
        D2D1_RECT_F r = Rc(x, y, w, S(96));
        glassPanel(r, 14);
        drawText(L"Backups & data", g_fH2, { x + S(18), y + S(10), x + S(400), y + S(38) }, C_TEXT);
        drawText(L"Registry snapshots and logs live in:", g_fSmall, { x + S(18), y + S(42), x + w - S(20), y + S(62) }, C_DIM);
        drawText(appDataDir(), g_fMono, { x + S(18), y + S(62), x + w - S(20), y + S(86) }, C_ACC);
        D2D1_RECT_F ob = Rc(x + w - S(190), y + S(40), S(170), S(34));
        if (button(ob, L"Open folder")) shellOpen(appDataDir());
        y += S(110);
    }
    // save + gaming extras
    {
        D2D1_RECT_F sb = Rc(x, y, S(220), S(42));
        if (button(sb, L"✔ Save settings", true)) saveSettings();
        D2D1_RECT_F sb2 = Rc(x + S(232), y, S(220), S(42));
        if (button(sb2, L"↺ Restore ALL defaults")) {
            auto rep = engine::runProfile("restore");
            g.status = rep.summary; g.statusKind = rep.failed ? 2 : 1;
            g.sysDirty = true;
        }
    }
}

// ---- privacy
static void drawPrivacy() {
    auto cr = contentRect();
    float x = cr.left, y = cr.top + S(6);
    float w = cr.right - cr.left;
    drawText(L"Privacy & Telemetry", g_fH1, { x, y, x + S(500), y + S(34) }, C_TEXT);
    y += S(44);

    D2D1_RECT_F p = Rc(x, y, w, S(110));
    glassPanel(p, 14);
    drawText(L"Kill telemetry, ads and tracking in one click.", g_fBody,
             { x + S(18), y + S(14), x + w - S(20), y + S(44) }, C_TEXT);
    D2D1_RECT_F b1 = Rc(x + S(18), y + S(52), S(220), S(40));
    if (button(b1, L"🛡 Apply PRIVACY profile", true)) actProfile("privacy");
    y += S(124);

    struct Quick { const char* id; const wchar_t* label; };
    static const Quick quicks[] = {
        { "telemetry_off",    L"Telemetry off" },
        { "advertising_off",  L"Ad ID off" },
        { "activity_history", L"Activity history off" },
        { "bing_search",      L"Bing out of search" },
        { "tailored_experiences", L"Tailored ads off" },
        { "telemetry_tasks",  L"CEIP tasks off" },
        { "windows_copilot",  L"Copilot off" },
        { "edge_debloat",     L"Edge debloat" },
        { "brave_debloat",    L"Brave debloat" },
        { "location_off",     L"Location tracking off" },
        { "consumer_features", L"Store suggestions off" },
        { "wpbt_off",         L"WPBT execution off" },
    };
    float bw = (w - S(8) * 2) / 3;
    for (int i = 0; i < 12; ++i) {
        float bx = x + (i % 3) * (bw + S(8));
        float by = y + (i / 3) * S(48);
        D2D1_RECT_F r = Rc(bx, by, bw, S(40));
        if (button(r, quicks[i].label)) actApplyTweak(quicks[i].id);
    }
}

// ---- drivers
static void drawDrivers() {
    auto cr = contentRect();
    float x = cr.left, y = cr.top + S(6);
    float w = cr.right - cr.left;
    if (g.sysDirty) actRefreshSys();
    drawText(L"Drivers", g_fH1, { x, y, x + S(300), y + S(34) }, C_TEXT);
    D2D1_RECT_F rs = Rc(x + w - S(150), y + S(2), S(150), S(32));
    if (button(rs, g.drvBusy ? L"Reading…" : L"↻ Driver report", !g.drvBusy) && !g.drvBusy) actRefreshDrv();
    if (g.drvDirty && !g.drvBusy) { g.drvDirty = false; actRefreshDrv(); }
    y += S(44);

    D2D1_RECT_F p = Rc(x, y, w, S(110));
    glassPanel(p, 14);
    drawText(g.gpuName.empty() ? L"GPU: (unknown)" : L"GPU: " + g.gpuName, g_fH2,
             { x + S(18), y + S(14), x + w - S(20), y + S(44) }, C_TEXT);
    drawText(g.gpuDrv.empty() ? L"Driver: unknown" : L"Driver: " + g.gpuDrv + L"   ·   " + g.gpuDate,
             g_fBody, { x + S(18), y + S(48), x + w - S(20), y + S(76) }, C_DIM);
    D2D1_RECT_F b1 = Rc(x + S(18), y + S(70), S(200), S(32));
    D2D1_RECT_F b2 = Rc(x + S(228), y + S(70), S(240), S(32));
    if (button(b1, L"Open vendor page", true)) drivers::openVendorPage();
    if (button(b2, L"⤓ Scan Windows Update for drivers")) actWuDriverScan();
    y += S(124);

    // live driver report
    if (g.drvRep.contains("gpu")) {
        D2D1_RECT_F rp = Rc(x, y, w, S(150));
        glassPanel(rp, 12);
        drawText(L"Driver store — measured ages", g_fH2, { x + S(16), y + S(8), x + S(420), y + S(34) }, C_TEXT);
        float ry = y + S(40);
        auto line = [&](const json& d) {
            wstring l = L"  " + widen(d.value("name", std::string("?")))
                + L"   v" + widen(d.value("version", std::string("?")))
                + L"   [" + widen(d.value("date", std::string("?"))) + L"]";
            wstring age = d.contains("ageDays") && !d["ageDays"].is_null()
                ? (std::to_wstring(d["ageDays"].get<long long>()) + L" days — " + widen(d.value("age", std::string("ok"))))
                : L"";
            drawText(l, g_fSmall, { x + S(16), ry, x + w - S(150), ry + S(18) }, C_DIM);
            D2D1_COLOR_F ac = widen(d.value("age", std::string())).find(L"stale") != wstring::npos ? C_ERR
                            : widen(d.value("age", std::string())).find(L"old") != wstring::npos ? C_WARN : C_OK;
            drawText(age, g_fSmall, { x + w - S(150), ry, x + w - S(16), ry + S(18) }, ac);
            ry += S(19);
        };
        if (g.drvRep["gpu"].is_object()) line(g.drvRep["gpu"]);
        int shown = 0;
        for (auto& d : g.drvRep["net"])   { if (shown++ > 2) break; line(d); }
        for (auto& d : g.drvRep["audio"]) { if (shown++ > 4) break; line(d); }
        y += S(160);
    }
    // problem devices
    if (g.drvProblems.is_array() && !g.drvProblems.empty()) {
        D2D1_RECT_F pp = Rc(x, y, w, S(34) * (float)std::min<size_t>(g.drvProblems.size() + 1, 5));
        glassPanel(pp, 12);
        drawText(L"Devices with a problem", g_fH2, { x + S(16), y + S(8), x + S(420), y + S(32) }, C_WARN);
        float ry = y + S(36);
        int i = 0;
        for (auto& d : g.drvProblems) {
            if (i++ >= 3) break;
            wchar_t code[24]; swprintf(code, 24, L" (code %lld)", d.value("problem", (long long)0));
            drawText(L"[!] " + widen(d.value("name", std::string("?"))) + code, g_fSmall,
                     { x + S(16), ry, x + w - S(16), ry + S(18) }, C_ERR);
            ry += S(19);
        }
        y += pp.bottom - pp.top + S(10);
    }

    struct Q { const wchar_t* label; std::function<void()> fn; };
    static const Q qs[] = {
        { L"DirectX diagnostics (dxdiag)",  [] { drivers::openDxdiag(); } },
        { L"Windows Update window (drivers)", [] { drvupdate::openWindowsUpdateUi(); } },
        { L"NVIDIA driver page",            [] { shellOpen(L"https://www.nvidia.com/Download/index.aspx"); } },
        { L"AMD driver page",               [] { shellOpen(L"https://www.amd.com/en/support"); } },
        { L"Intel driver page",             [] { shellOpen(L"https://www.intel.com/content/www/us/en/download-center/home.html"); } },
        { L"Device Manager",                [] { shellOpen(L"devmgmt.msc"); } },
    };
    float bw = (w - S(8)) / 2;
    for (int i = 0; i < 6; ++i) {
        float bx = x + (i % 2) * (bw + S(8));
        float by = y + (i / 2) * S(50);
        D2D1_RECT_F r = Rc(bx, by, bw, S(42));
        if (button(r, qs[i].label)) qs[i].fn();
    }
    y += S(170);

    D2D1_RECT_F tip = Rc(x, y, w, S(70));
    glassPanel(tip, 12);
    drawText(L"Tip — clean install: use DDU (Display Driver Uninstaller) in safe mode before installing",
             g_fBody, { x + S(18), y + S(10), x + w - S(20), y + S(38) }, C_TEXT);
    drawText(L"a fresh GPU driver, then pick 'NVIDIA app / AMD Adrenalin' minimal install.",
             g_fBody, { x + S(18), y + S(36), x + w - S(20), y + S(64) }, C_DIM);
}

// ---- ping
static void drawPing() {
    auto cr = contentRect();
    float x = cr.left, y = cr.top + S(6);
    float w = cr.right - cr.left;
    drawText(L"Network latency", g_fH1, { x, y, x + S(400), y + S(34) }, C_TEXT);
    y += S(44);

    D2D1_RECT_F rb = Rc(x + w - S(130), y - S(6), S(130), S(36));
    if (button(rb, g.pingsBusy ? L"Testing…" : L"↻ Test all", !g.pingsBusy) && !g.pingsBusy) actPings();
    if (g.pingsDirty && !g.pingsBusy) { g.pingsDirty = false; actPings(); }

    D2D1_RECT_F hd = Rc(x, y + S(40), w, S(30));
    fillRR(RR(hd, 8), D2D1_COLOR_F{ 1,1,1,0.08f });
    drawText(L"NAME", g_fSmall, { x + S(14), y + S(46), x + S(220), y + S(66) }, C_FAINT);
    drawText(L"HOST", g_fSmall, { x + S(220), y + S(46), x + S(430), y + S(66) }, C_FAINT);
    drawText(L"LATENCY", g_fSmall, { x + S(430), y + S(46), x + S(560), y + S(66) }, C_FAINT);

    float ry = y + S(78);
    {
        std::lock_guard<std::mutex> lk(g_pingMtx);
        for (auto& r : g.pings) {
            D2D1_RECT_F row = Rc(x, ry, w, S(34));
            glassPanel(row, 8);
            drawText(r.name, g_fBody, { x + S(14), ry + S(6), x + S(220), ry + S(30) }, C_TEXT);
            drawText(r.host, g_fBody, { x + S(220), ry + S(6), x + S(430), ry + S(30) }, C_DIM);
            if (r.testing) drawText(L"…", g_fBody, { x + S(430), ry + S(6), x + S(560), ry + S(30) }, C_FAINT);
            else if (r.ok) {
                wstring ms = std::to_wstring(r.ms) + L" ms";
                D2D1_COLOR_F c = r.ms < 30 ? C_OK : r.ms < 80 ? C_WARN : C_ERR;
                drawDot(x + S(438), ry + S(17), S(4), c);
                drawText(ms, g_fBody, { x + S(450), ry + S(6), x + S(560), ry + S(30) }, c);
            } else drawText(L"timeout", g_fBody, { x + S(430), ry + S(6), x + S(560), ry + S(30) }, C_ERR);
            D2D1_RECT_F del = { x + w - S(60), ry + S(4), x + w - S(12), ry + S(30) };
            if (button(del, L"✕", false, true, g_fSmall)) actRemoveTarget(r.name);
            ry += S(42);
        }
    }

    float iy = ry + S(10);
    D2D1_RECT_F in = Rc(x, iy, w - S(150), S(38));
    glassPanel(in, 10);
    strokeRR(RR(in, 10), g.pingHostFocused ? D2D1_COLOR_F{ 0.30f,0.62f,1.0f,0.9f } : C_GLASSBR);
    wstring shown = g.pingHostInput + (g.pingHostFocused && ((int)(g.t0 * 2) % 2) ? L"|" : L"");
    drawText(g.pingHostInput.empty() && !g.pingHostFocused ? L"Add host (IP or domain)…" : shown,
             g_fBody, { in.left + S(12), in.top + S(7), in.right, in.bottom }, C_DIM);
    if (clickedIn(in)) g.pingHostFocused = true;
    if (g.mouseDown && !clickedIn(in) && !inRect(in, g.mouse)) g.pingHostFocused = false;
    D2D1_RECT_F add = Rc(in.right + S(10), iy, S(130), S(38));
    if (button(add, L"+ Add target", true)) actAddTarget();
}

// ---- logs
static void drawLogs() {
    auto cr = contentRect();
    float x = cr.left, y = cr.top + S(6);
    float w = cr.right - cr.left, h = cr.bottom - cr.top;
    drawText(L"Activity log", g_fH1, { x, y, x + S(300), y + S(34) }, C_TEXT);
    D2D1_RECT_F b1 = Rc(x + w - S(130), y + S(2), S(130), S(32));
    if (button(b1, L"Open .log file")) shellOpen(logPath());
    y += S(44);

    D2D1_RECT_F p = Rc(x, y, w, h - (y - cr.top) - S(2));
    glassPanel(p, 12);
    wstring all = log::readAll();
    if (all.size() > 2000) all = all.substr(all.size() - 2000);
    float scroll = g.scroll["logs"];
    g_rt->PushAxisAlignedClip(p, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    drawText(all, g_fMono, { p.left + S(12), p.top + S(10) - scroll, p.right - S(12), p.bottom - S(10) }, C_DIM);
    g_rt->PopAxisAlignedClip();
}

// ---- about
static void drawAbout() {
    auto cr = contentRect();
    float x = cr.left, y = cr.top + S(6);
    float w = cr.right - cr.left;
    drawText(L"About", g_fH1, { x, y, x + S(300), y + S(34) }, C_TEXT);
    y += S(48);
    D2D1_RECT_F p = Rc(x, y, w, S(210));
    glassPanel(p, 14);
    drawText(L"OptimizeKit v2.8", g_fH2, { x + S(20), y + S(14), x + S(400), y + S(44) }, C_TEXT);
    drawText(L"Native C++ dashboard (Direct2D) — no browser, no Electron, no injection.",
             g_fBody, { x + S(20), y + S(46), x + w - S(20), y + S(72) }, C_DIM);
    drawText(L"Tweaks curated from Chris Titus Tech's WinUtil (MIT), Microsoft docs and the PC",
             g_fSmall, { x + S(20), y + S(76), x + w - S(20), y + S(96) }, C_FAINT);
    drawText(L"gaming community. Every tweak can be restored to Windows defaults.",
             g_fSmall, { x + S(20), y + S(94), x + w - S(20), y + S(114) }, C_FAINT);
    D2D1_RECT_F b1 = Rc(x + S(20), y + S(130), S(220), S(38));
    D2D1_RECT_F b2 = Rc(x + S(254), y + S(130), S(220), S(38));
    if (button(b1, L"GitHub repository")) shellOpen(L"https://github.com/cameleonnbss/OptimizeKit");
    if (button(b2, L"WinUtil (Chris Titus)")) shellOpen(L"https://github.com/ChrisTitusTech/winutil");
    y += S(230);
    D2D1_RECT_F w2 = Rc(x, y, w, S(110));
    glassPanel(w2, 14);
    drawText(L"⚠ Disclaimer", g_fH2, { x + S(20), y + S(12), x + S(300), y + S(40) }, C_WARN);
    drawText(L"Registry tweaks change your system. Restore points and the automatic .reg backups live in:",
             g_fSmall, { x + S(20), y + S(44), x + w - S(20), y + S(66) }, C_DIM);
    drawText(appDataDir(), g_fMono, { x + S(20), y + S(68), x + w - S(20), y + S(94) }, C_ACC);
}

// ------------------------------------------------------------------ frame
static void render() {
    if (!g_rt) return;
    g_rt->BeginDraw();
    float t = (float)GetTickCount64() / 1000.0f;
    g.t0 = t;
    drawBackground(t);

    drawSidebar();
    drawTitlebar();
    switch (g.tab) {
        case T_DASH:     drawDashboard(); break;
        case T_TWEAKS:   drawTweaks(); break;
        case T_GAMING:   drawGaming(); break;
        case T_FIRMWARE: drawFirmware(); break;
        case T_STORAGE:  drawStorage(); break;
        case T_STARTUP:  drawStartup(); break;
        case T_SETTINGS: drawSettings(); break;
        case T_PRIVACY:  drawPrivacy(); break;
        case T_DRIVERS:  drawDrivers(); break;
        case T_PING:     drawPing(); break;
        case T_LOGS:     drawLogs(); break;
        case T_ABOUT:    drawAbout(); break;
        default: break;
    }
    drawStatusbar();
    g_rt->EndDraw();
    g.clickAt = { -1,-1 };  // consume click
}

static void runActions() {
    auto copy = g_actions;
    g_actions.clear();
    for (auto& a : copy) a.fn();
}

// ------------------------------------------------------------------ wndproc
static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
        case WM_CREATE: {
            SetTimer(h, 1, 16, nullptr);
            loadSettings();
            return 0;
        }
        case WM_TIMER: InvalidateRect(h, nullptr, FALSE); return 0;
        case WM_SIZE: {
            if (g_rt && wp != SIZE_MINIMIZED) {
                UINT w = LOWORD(lp), hh = HIWORD(lp);
                g_rt->Resize(D2D1::SizeU(w, hh));
                g_sz = { (float)w, (float)hh };
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            POINTS pts = MAKEPOINTS(lp);
            g.mouse = { pts.x, pts.y };
            return 0;
        }
        case WM_LBUTTONDOWN: {
            POINTS pts = MAKEPOINTS(lp);
            g.mouse = { pts.x, pts.y };
            g.mouseDown = true;
            g.clickAt = g.mouse;
            return 0;
        }
        case WM_LBUTTONUP:   g.mouseDown = false; return 0;
        case WM_MOUSEWHEEL: {
            float d = (float)GET_WHEEL_DELTA_WPARAM(wp);
            const char* key = "other";
            if (g.tab == T_TWEAKS) key = "tweaks";
            else if (g.tab == T_GAMING) key = "procs";
            else if (g.tab == T_LOGS) key = "logs";
            else if (g.tab == T_STARTUP) key = "startup";
            else if (g.tab == T_STORAGE) key = "storage";
            float& s = g.scroll[key];
            s = std::max(0.0f, s - d * 0.5f * g_dpiScale);
            return 0;
        }
        case WM_CHAR: {
            wchar_t c = (wchar_t)wp;
            if (g.pingHostFocused && g.tab == T_PING) {
                if (c == L'\b') { if (!g.pingHostInput.empty()) g.pingHostInput.pop_back(); }
                else if (c == L'\r') actAddTarget();
                else if (c >= 32 && c < 127) g.pingHostInput += c;
            }
            return 0;
        }
        case WM_PAINT: {
            ValidateRect(h, nullptr);
            render();
            runActions();
            return 0;
        }
        case WM_NCHITTEST: {
            POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ScreenToClient(h, &pt);
            if (pt.y < S(56) && pt.x > S(140) && pt.x < g_sz.width - S(100))
                return HTCAPTION;
            break;
        }
        case WM_ERASEBKGND: return 1;
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

int runDashboard() {
    log::clear();
    log::info(L"native dashboard started (no browser components)");

    WNDCLASSEXW wc{ sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(1));
    wc.hIconSm = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(1));
    wc.lpszClassName = L"OptimizeKitWnd";
    RegisterClassExW(&wc);

    int w = 1220, hh = 780;
    RECT wa{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int x = (wa.right - w) / 2, y = (wa.bottom - hh) / 2;

    g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"OptimizeKit — Windows Optimization Suite",
        WS_OVERLAPPEDWINDOW, x, y, w, hh, nullptr, nullptr, wc.hInstance, nullptr);

    UINT dpi = GetDpiForWindow(g_hwnd);
    g_dpiScale = dpi / 96.0f;

    D2D1_FACTORY_OPTIONS fo{};
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), &fo, (void**)&g_fac);
    RECT rc; GetClientRect(g_hwnd, &rc);
    g_fac->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(g_hwnd, D2D1::SizeU(rc.right, rc.bottom)),
        &g_rt);
    g_sz = { (float)rc.right, (float)rc.bottom };

    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&g_dw);
    auto mkFmt = [&](float size, const wchar_t* fam = L"Segoe UI", bool bold = false) {
        IDWriteTextFormat* f = nullptr;
        DWRITE_FONT_WEIGHT wgt = bold ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
        g_dw->CreateTextFormat(fam, nullptr, wgt, DWRITE_FONT_STYLE_NORMAL,
                               DWRITE_FONT_STRETCH_NORMAL, S(size), L"en-us", &f);
        if (f) f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        return f;
    };
    g_fTitle = mkFmt(17, L"Segoe UI", true);
    g_fH1    = mkFmt(24, L"Segoe UI", true);
    g_fH2    = mkFmt(17, L"Segoe UI", true);
    g_fBody  = mkFmt(14);
    g_fSmall = mkFmt(12);
    g_fMono  = mkFmt(12, L"Consolas");

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

} // namespace ok::ui
