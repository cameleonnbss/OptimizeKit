// OptimizeKit - liquid glass dashboard (Direct2D + DirectWrite, no other deps)
// Visual style inspired by WormGPT-desktop (cameleonnbss): dark glass, accent gradients.
#include "ui.h"
#include "engine.h"
#include <d2d1.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <map>
#include <thread>
#include <mutex>
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
enum Tab { T_DASH, T_TWEAKS, T_GAMING, T_PRIVACY, T_DRIVERS, T_PING, T_LOGS, T_ABOUT, T_COUNT };

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
        // sort by pid, drop system pid 0/4
        p.erase(std::remove_if(p.begin(), p.end(), [](auto& x) { return x.pid <= 4; }), p.end());
        {
            // no mutex needed: vector swap under GIL-ish single UI thread read after flag
            g.procs = p;
            g.procsBusy = false;
        }
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
    // top highlight gradient
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
    if (accent) {
        strokeRR(rr, D2D1_COLOR_F{ 1,1,1, 0.25f });
    } else {
        strokeRR(rr, D2D1_COLOR_F{ 1,1,1, hover ? 0.30f : 0.14f });
    }
    D2D1_COLOR_F tc = enabled ? (accent ? D2D1_COLOR_F{ 0.02f,0.05f,0.12f,1 } : C_TEXT)
                              : D2D1_COLOR_F{ 1,1,1, 0.25f };
    auto fmt = f ? f : g_fBody;
    D2D1_RECT_F tr = r; tr.left += 12; tr.right -= 12;
    drawText(label, fmt, tr, tc);
    bool hit = enabled && clickedIn(r);
    return hit;
}

static bool checkboxRow(D2D1_RECT_F& r, const wstring& label, bool& checked, bool adminReq) {
    bool hover = inRect(r, g.mouse);
    if (hover) fillRect(r, D2D1_COLOR_F{ 1,1,1,0.04f });
    // box
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
    bool hit = clickedIn(r);
    return hit;
}

// ------------------------------------------------------------------ background
static void drawBackground(float t) {
    g_rt->Clear(C_BASE);
    // animated blobs
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
        { T_DASH,    L"Dashboard"   },
        { T_TWEAKS,  L"Tweaks"      },
        { T_GAMING,  L"Gaming"      },
        { T_PRIVACY, L"Privacy"     },
        { T_DRIVERS, L"Drivers"     },
        { T_PING,    L"Network"     },
        { T_LOGS,    L"Logs"        },
        { T_ABOUT,   L"About"       },
    };
    float x = S(16), y = S(72), w = sidebarX() - S(28);
    for (int i = 0; i < T_COUNT; ++i) {
        D2D1_RECT_F r = Rc(x, y, w, S(38));
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
        y += S(44);
    }
    // admin badge
    D2D1_RECT_F r = Rc(S(16), g_sz.height - S(56), w, S(40));
    glassPanel(r, 10);
    drawText(isAdmin() ? L"● Elevated (admin)" : L"○ Standard user",
             g_fSmall, { r.left + S(10), r.top + S(10), r.right, r.bottom },
             isAdmin() ? C_OK : C_DIM);
}

static void drawTitlebar() {
    // drag area handled in WM_NCHITTEST; here draw title
    drawText(L"OPTIMIZEKIT", g_fTitle, { S(20), S(14), sidebarX(), S(52) }, C_TEXT);
    D2D1_COLOR_F acc = C_ACC;
    drawDot(S(20) - S(10), S(30) * 1.0f, S(5), acc);

    // window buttons
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

// ---- dashboard
static void drawDashboard() {
    auto cr = contentRect();
    if (g.sysDirty) actRefreshSys();
    float x = cr.left, y = cr.top + S(6);

    // hero
    D2D1_RECT_F hero = Rc(x, y, cr.right - cr.left, S(120));
    glassPanel(hero, 16);
    drawText(L"Optimize your PC.", g_fH1, { x + S(22), y + S(16), x + S(500), y + S(60) }, C_TEXT);
    drawText(L"Gaming · Privacy · Debloat — one kit, fully logged.", g_fBody,
             { x + S(22), y + S(62), x + S(600), y + S(90) }, C_DIM);
    D2D1_RECT_F b1 = Rc(x + S(620), y + S(34), S(160), S(40));
    D2D1_RECT_F b2 = Rc(x + S(795), y + S(34), S(140), S(40));
    if (button(b1, L"⚡ Quick Optimize", true)) actProfile("gaming");
    if (button(b2, L"🧹 Clean junk")) actClean();
    y += S(140);

    // system grid (2 cols)
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
    float rowH = S(46);
    for (size_t i = 0; i < rows.size(); ++i) {
        float cx = x + (i % 2) * (colW + S(12));
        float cy = y + (i / 2) * (rowH + S(8));
        D2D1_RECT_F r = Rc(cx, cy, colW, rowH);
        glassPanel(r, 10);
        drawText(rows[i].k, g_fSmall, { cx + S(14), cy + S(6), cx + colW, cy + S(22) }, C_FAINT);
        drawText(rows[i].v, g_fBody, { cx + S(14), cy + S(20), cx + colW - S(10), cy + rowH }, C_TEXT);
    }
    y += (rows.size() / 2 + (rows.size() % 2)) * (rowH + S(8)) + S(16);

    // quick actions
    D2D1_RECT_F r = Rc(x, y, cr.right - cr.left, S(150));
    glassPanel(r, 14);
    drawText(L"Quick actions", g_fH2, { x + S(18), y + S(10), x + S(400), y + S(40) }, C_TEXT);
    D2D1_RECT_F q1 = Rc(x + S(18),  y + S(52), S(190), S(38));
    D2D1_RECT_F q2 = Rc(x + S(220), y + S(52), S(190), S(38));
    D2D1_RECT_F q3 = Rc(x + S(422), y + S(52), S(190), S(38));
    D2D1_RECT_F q4 = Rc(x + S(624), y + S(52), S(190), S(38));
    if (button(q1, L"Apply GAMING profile", true)) actProfile("gaming");
    if (button(q2, L"Apply PRIVACY profile")) actProfile("privacy");
    if (button(q3, L"Apply FULL kit")) actProfile("full");
    if (button(q4, L"Open log folder")) shellOpen(appDataDir());
    drawText(L"Everything the kit does is written to OptimizeKit.log — open the Logs tab anytime.",
             g_fSmall, { x + S(18), y + S(104), x + S(700), y + S(130) }, C_FAINT);
}

// ---- tweaks
static void drawTweaks() {
    auto cr = contentRect();
    float x = cr.left, y = cr.top + S(6);
    float w = cr.right - cr.left;

    drawText(L"Tweaks", g_fH1, { x, y, x + S(300), y + S(34) }, C_TEXT);
    y += S(40);

    // filter pills
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

    // action bar
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

    // list
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
                || t.id.find("edge") != string::npos))
            || (g.tweakFilter == 3 && (t.id.find("bloat") != string::npos || t.id.find("onedrive") != string::npos
                || t.id.find("xbox") != string::npos || t.id.find("sysmain") != string::npos));
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
        // badges
        wstring badge = t.admin ? L"ADMIN" : L"USER";
        D2D1_COLOR_F bc = t.admin ? C_WARN : C_OK;
        D2D1_RECT_F brr = { r.right - S(170), r.top + S(10), r.right - S(96), r.top + S(28) };
        fillRR(RR(brr, 9), D2D1_COLOR_F{ bc.r, bc.g, bc.b, 0.18f });
        drawText(badge, g_fSmall, brr, bc);
        // impact stars
        wstring stars;
        for (int i = 0; i < t.impact; ++i) stars += L"★";
        drawText(stars, g_fSmall, { r.right - S(88), r.top + S(8), r.right - S(10), r.top + S(28) }, C_ACC2);
        // source + click-to-apply hint
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

    // individual quick tweaks
    struct Quick { const char* id; const wchar_t* label; };
    static const Quick quicks[] = {
        { "game_dvr_off",   L"Game DVR off" },
        { "network_gaming", L"Gaming network" },
        { "timer_high",     L"Timer 0.5ms" },
        { "hags_on",        L"HAGS" },
        { "mpo_off",        L"MPO fix" },
        { "power_ultimate", L"Ultimate plan" },
        { "mouse_precision",L"Raw mouse" },
        { "fso_on",         L"Exclusive FS" },
    };
    float bw = (w - S(8) * 3) / 4;
    for (int i = 0; i < 8; ++i) {
        float bx = x + (i % 4) * (bw + S(8));
        float by = y + (i / 4) * S(48);
        D2D1_RECT_F r = Rc(bx, by, bw, S(40));
        if (button(r, quicks[i].label)) actApplyTweak(quicks[i].id);
    }
    y += S(120);

    // process priority
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
    int shown = 0;
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
        py += lh; shown++;
    }
    g_rt->PopAxisAlignedClip();
    g.scroll["procs"] = scroll;
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
        { "edge_bing_blocking", L"Edge background off" },
        { "bloat_uninstall",  L"Store bloat removal" },
        { "onedrive_off",     L"OneDrive removal" },
    };
    float bw = (w - S(8) * 2) / 3;
    for (int i = 0; i < 10; ++i) {
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
    y += S(44);

    D2D1_RECT_F p = Rc(x, y, w, S(120));
    glassPanel(p, 14);
    drawText(g.gpuName.empty() ? L"GPU: (unknown)" : L"GPU: " + g.gpuName, g_fH2,
             { x + S(18), y + S(14), x + w - S(20), y + S(44) }, C_TEXT);
    drawText(g.gpuDrv.empty() ? L"Driver: unknown" : L"Driver: " + g.gpuDrv + L"   ·   " + g.gpuDate,
             g_fBody, { x + S(18), y + S(48), x + w - S(20), y + S(76) }, C_DIM);
    D2D1_RECT_F b1 = Rc(x + S(18), y + S(74), S(200), S(36));
    if (button(b1, L"Open vendor page", true)) drivers::openVendorPage();
    y += S(134);

    struct Q { const wchar_t* label; std::function<void()> fn; };
    static const Q qs[] = {
        { L"DirectX diagnostics (dxdiag)",  [] { drivers::openDxdiag(); } },
        { L"Scan Windows Update for drivers", [] { drivers::scanWindowsUpdateDrivers(); } },
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

    // table header
    D2D1_RECT_F hd = Rc(x, y + S(40), w, S(30));
    fillRR(RR(hd, 8), D2D1_COLOR_F{ 1,1,1,0.08f });
    drawText(L"NAME", g_fSmall, { x + S(14), y + S(46), x + S(220), y + S(66) }, C_FAINT);
    drawText(L"HOST", g_fSmall, { x + S(220), y + S(46), x + S(430), y + S(66) }, C_FAINT);
    drawText(L"LATENCY", g_fSmall, { x + S(430), y + S(46), x + S(560), y + S(66) }, C_FAINT);
    drawText(L"", g_fSmall, hd, C_FAINT);

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

    // add target input
    float iy = ry + S(10);
    D2D1_RECT_F in = Rc(x, iy, w - S(150), S(38));
    glassPanel(in, 10);
    D2D1_COLOR_F bc = g.pingHostFocused ? C_ACC : C_GLASSBR;
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
    // show the tail (last ~2000 chars)
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
    drawText(L"OptimizeKit v1.0", g_fH2, { x + S(20), y + S(14), x + S(400), y + S(44) }, C_TEXT);
    drawText(L"Windows optimization kit — native C++ dashboard + PowerShell/Batch engines.",
             g_fBody, { x + S(20), y + S(46), x + w - S(20), y + S(72) }, C_DIM);
    drawText(L"Tweaks curated from Chris Titus Tech's WinUtil (MIT), Valve/Microsoft docs and the",
             g_fSmall, { x + S(20), y + S(76), x + w - S(20), y + S(96) }, C_FAINT);
    drawText(L"PC gaming community. Every tweak can be restored to Windows defaults.",
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
        case T_DASH:    drawDashboard(); break;
        case T_TWEAKS:  drawTweaks(); break;
        case T_GAMING:  drawGaming(); break;
        case T_PRIVACY: drawPrivacy(); break;
        case T_DRIVERS: drawDrivers(); break;
        case T_PING:    drawPing(); break;
        case T_LOGS:    drawLogs(); break;
        case T_ABOUT:   drawAbout(); break;
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
            // custom title bar drag area (everything above content, left of buttons)
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
    log::info(L"dashboard started");

    WNDCLASSEXW wc{ sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
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

    // D2D
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
