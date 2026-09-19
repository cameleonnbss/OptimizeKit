// OptimizeKit - game library implementation.
// Icon extraction: SHDefExtractIconW (dynamic, MinGW header lacks it) -> GetDIBits -> GDI+ PNG cache.
// Launcher detection: registry uninstall keys + launcher library manifests + known dirs.
#include "games.h"
#include "engine.h"
#include "gameboost.h"
#include "tweaks.h"
#include "sysinfo.h"
#include <shellapi.h>
#include <shlobj.h>
#include <gdiplus.h>
#include <filesystem>
#include <fstream>
#include <algorithm>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")

namespace fs = std::filesystem;

namespace ok::games {

using json = nlohmann::json;

// ------------------------------------------------------------------ utils
static wstring lower(wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), ::towlower);
    return s;
}
static wstring niceName(const wstring& exe) {
    // "C:\\games\\Cyberpunk2077.exe" -> "Cyberpunk2077" (basename, no extension)
    wstring n = exe;
    size_t p = n.find_last_of(L"\\/");
    if (p != wstring::npos) n = n.substr(p + 1);
    p = n.rfind(L'.');
    if (p != wstring::npos && p > 0) n = n.substr(0, p);
    return n;
}

// helper/infra executables that must never be picked as "the game"
static bool helperExe(const wstring& fileName) {
    static const wchar_t* bad[] = {
        L"crash", L"unins", L"setup", L"redist", L"dxsetup", L"vcredist",
        L"eac", L"easyanticheat", L"battleye", L"unity", L"updater", L"patcher",
    };
    wstring l = lower(fileName);
    for (auto* b : bad) if (l.find(b) != wstring::npos) return true;
    return false;
}
static wstring iconCacheDir() {
    wstring d = appDataDir() + L"\\game-icons";
    CreateDirectoryW(d.c_str(), nullptr);
    return d;
}

// flat cache filename for a game id (defensive: strip anything not filename-safe)
wstring safeIconId(const wstring& id) {
    wstring s;
    for (wchar_t c : id) {
        if (c == L'\\' || c == L'/' || c == L':' || c == L'*' || c == L'?' ||
            c == L'"' || c == L'<' || c == L'>' || c == L'|') s += L'_';
        else s += c;
    }
    return s;
}

// ------------------------------------------------------------------ icons
// ------------------------------------------------------------------ icons
typedef HRESULT (WINAPI *SHDefExtractIconW_t)(LPCWSTR, int, UINT, HICON*, HICON*, UINT);
static SHDefExtractIconW_t shdefIcon() {
    static SHDefExtractIconW_t f = nullptr;
    if (!f) {
        HMODULE m = GetModuleHandleW(L"shell32.dll");
        if (!m) m = LoadLibraryW(L"shell32.dll");
        f = m ? (SHDefExtractIconW_t)GetProcAddress(m, "SHDefExtractIconW") : nullptr;
    }
    return f;
}

// HICON -> 32bpp ARGB PNG. Reads the color bitmap raw (alpha when present) and
// combines it with the AND mask for legacy icons. No GDI+ drawing involved.
static bool hiconToPng(HICON icon, const wstring& cache) {
    ICONINFO ii{};
    if (!GetIconInfo(icon, &ii)) return false;
    bool ok = false;
    BITMAP cbm{}, mbm{};
    if (ii.hbmColor && ii.hbmMask &&
        GetObject(ii.hbmColor, sizeof(cbm), &cbm) && GetObject(ii.hbmMask, sizeof(mbm), &mbm)) {
        int w = cbm.bmWidth, h = cbm.bmHeight > 0 ? cbm.bmHeight : -cbm.bmHeight;
        if (w > 0 && h > 0) {
            BITMAPINFO bi{};
            bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth = w;
            bi.bmiHeader.biHeight = -h; // top-down
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            vector<unsigned char> col((size_t)w * h * 4);
            HDC mem = CreateCompatibleDC(nullptr);
            if (mem) {
                if (GetDIBits(mem, ii.hbmColor, 0, h, col.data(), &bi, DIB_RGB_COLORS)) {
                    int maskStride = ((w + 31) / 32) * 4;
                    vector<unsigned char> msk((size_t)maskStride * h);
                    BITMAPINFO bmi{};
                    bmi.bmiHeader = bi.bmiHeader;
                    bmi.bmiHeader.biBitCount = 1;
                    bool hasMask = GetDIBits(mem, ii.hbmMask, 0, h, msk.data(), &bmi, DIB_RGB_COLORS) != 0;
                    bool colorHasAlpha = cbm.bmBitsPixel >= 32;
                    for (int y = 0; y < h; ++y) {
                        for (int x = 0; x < w; ++x) {
                            unsigned char* px = &col[((size_t)y * w + x) * 4];
                            unsigned char a;
                            if (colorHasAlpha) {
                                a = px[3];
                            } else {
                                unsigned char bit = hasMask ? (msk[(size_t)y * maskStride + x / 8] & (0x80 >> (x % 8))) : 0;
                                a = bit ? 0 : 255; // AND mask: 1 = transparent
                            }
                            px[3] = a;
                        }
                    }
                    Gdiplus::GdiplusStartupInput si;
                    ULONG_PTR tok = 0;
                    if (Gdiplus::GdiplusStartup(&tok, &si, nullptr) == Gdiplus::Ok) {
                        {
                            // IMPORTANT: Bitmap must be destroyed BEFORE GdiplusShutdown
                            Gdiplus::Bitmap bmp(w, h, w * 4, PixelFormat32bppARGB, col.data());
                            CLSID clsid;
                            if (CLSIDFromString(L"{557cf406-1a04-11d3-9a73-0000f81ef32e}", (LPCLSID)&clsid) == S_OK)
                                ok = bmp.Save(cache.c_str(), &clsid, nullptr) == Gdiplus::Ok;
                        }
                        Gdiplus::GdiplusShutdown(tok);
                    }
                }
                DeleteDC(mem);
            }
        }
    }
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    return ok;
}

wstring extractIcon(const Game& g) {
    wstring cache = iconCacheDir() + L"\\" + safeIconId(g.id) + L".png";
    if (fs::exists(cache)) return cache;

    HICON icon = nullptr;
    if (auto f = shdefIcon()) {
        for (int sz : {96, 48, 32}) {
            HICON h = nullptr;
            if (SUCCEEDED(f(g.exePath.c_str(), 0, 0, &h, nullptr, MAKELONG(sz, 0))) && h) { icon = h; break; }
        }
    }
    if (!icon) {
        if (ExtractIconExW(g.exePath.c_str(), 0, &icon, nullptr, 1) == 0 || !icon) return L"";
    }
    bool ok = hiconToPng(icon, cache);
    DestroyIcon(icon);
    return ok && fs::exists(cache) ? cache : L"";
}

// ------------------------------------------------------------- detection
static void addGame(vector<Game>& out, const Game& g) {
    for (auto& e : out) if (e.id == g.id) return;
    out.push_back(g);
}

static void scanSteam(vector<Game>& out) {
    // Steam library folders -> appmanifest_*_.acf -> name + install dir; icon: steam/cache or exe
    wchar_t* p = nullptr;
    if (SHGetKnownFolderPath(FOLDERID_ProgramFilesX86, 0, nullptr, &p) != S_OK) return;
    wstring steamRoot = wstring(p) + L"\\Steam";
    CoTaskMemFree(p);

    wstring libFile = steamRoot + L"\\steamapps\\libraryfolders.vdf";
    std::ifstream f(libFile.c_str());
    vector<wstring> libs;
    if (f) {
        // cheap VDF: collect "path" values (UTF-8 file)
        std::string line;
        while (std::getline(f, line)) {
            if (line.find("\"path\"") != string::npos) {
                auto q1 = line.find('"', line.find("\"path\"") + 6);
                auto q2 = line.find('"', q1 + 1);
                if (q1 != string::npos && q2 != string::npos)
                    libs.push_back(widen(line.substr(q1 + 1, q2 - q1 - 1)));
            }
        }
    }
    if (libs.empty()) libs.push_back(steamRoot);

    for (auto& lib : libs) {
        wstring dir = lib + L"\\steamapps";
        if (!fs::exists(dir)) continue;
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW((dir + L"\\appmanifest_*.acf").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            std::ifstream acf((dir + L"\\" + fd.cFileName).c_str());
            if (!acf) continue;
            std::string content((std::istreambuf_iterator<char>(acf)), std::istreambuf_iterator<char>());
            // "name"      "Game Name"
            auto nk = content.find("\"name\"");
            if (nk == string::npos) continue;
            auto q1 = content.find('"', nk + 6); auto q2 = content.find('"', q1 + 1);
            if (q1 == string::npos || q2 == string::npos) continue;
            wstring name = widen(content.substr(q1 + 1, q2 - q1 - 1));
            auto sk = content.find("\"installdir\"");
            wstring inst;            if (sk != string::npos) {
                auto s1 = content.find('"', sk + 12); auto s2 = content.find('"', s1 + 1);
                if (s1 != string::npos && s2 != string::npos) inst = widen(content.substr(s1 + 1, s2 - s1 - 1));
            }
            if (inst.empty()) continue;
            wstring fullDir = lib + L"\\steamapps\\common\\" + inst;
            // find the primary exe = biggest exe that is not a helper tool
            wstring exe, exeAny;
            uint64_t best = 0, bestAny = 0;
            WIN32_FIND_DATAW fd2;
            HANDLE h2 = FindFirstFileW((fullDir + L"\\*.exe").c_str(), &fd2);
            if (h2 != INVALID_HANDLE_VALUE) {
                do {
                    if (fd2.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                    ULARGE_INTEGER sz{ fd2.nFileSizeLow, fd2.nFileSizeHigh };
                    if (sz.QuadPart > bestAny) { bestAny = sz.QuadPart; exeAny = fullDir + L"\\" + fd2.cFileName; }
                    if (!helperExe(fd2.cFileName) && sz.QuadPart > best) { best = sz.QuadPart; exe = fullDir + L"\\" + fd2.cFileName; }
                } while (FindNextFileW(h2, &fd2));
                FindClose(h2);
            }
            if (exe.empty()) exe = exeAny;   // all helpers? fall back to biggest
            if (exe.empty()) continue;
            Game g;
            g.name = name;
            g.exePath = exe;
            g.launcher = L"Steam";
            g.id = lower(niceName(exe));   // id = exe name without path/extension
            addGame(out, g);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
}

static void scanEpic(vector<Game>& out) {
    // Epic manifests: %PROGRAMDATA%\Epic\EpicGamesLauncher\Data\Manifests\*.item (JSON)
    wchar_t* pd = nullptr;
    if (SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &pd) != S_OK) return;
    wstring dir = wstring(pd) + L"\\Epic\\EpicGamesLauncher\\Data\\Manifests";
    CoTaskMemFree(pd);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*.item").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        std::ifstream f((dir + L"\\" + fd.cFileName).c_str());
        if (!f) continue;
        try {
            json j = json::parse(f, nullptr, false);
            if (j.is_discarded()) continue;
            if (j.value("bIsIncompleteInstall", false)) continue;
            wstring name = widen(j.value("DisplayName", string()));
            wstring exeRel = widen(j.value("LaunchExecutable", string()));
            wstring inst  = widen(j.value("InstallLocation", string()));
            if (name.empty() || exeRel.empty() || inst.empty()) continue;
            Game g;
            g.name = name;
            g.exePath = inst + L"\\" + exeRel;
            g.launcher = L"Epic";
            g.id = lower(niceName(exeRel));
            addGame(out, g);
        } catch (...) {}
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static void scanRiot(vector<Game>& out) {
    // Riot client registers its games under HKLM\SOFTWARE\Riot Games, Inc.\<game>
    const HKEY roots[] = { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
    const wchar_t* base = L"SOFTWARE\\Riot Games, Inc.";
    for (auto root : roots) {
        HKEY k;
        if (RegOpenKeyExW(root, base, 0, KEY_READ, &k) != ERROR_SUCCESS) continue;
        DWORD idx = 0; wchar_t sub[128]; DWORD subLen = 128;
        while (RegEnumKeyExW(k, idx++, sub, &subLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            subLen = 128;
            wchar_t loc[512] = L""; DWORD sz = sizeof(loc);
            if (RegGetValueW(root, (wstring(base) + L"\\" + sub).c_str(), L"InstallLocation", RRF_RT_REG_SZ, nullptr, loc, &sz) != ERROR_SUCCESS)
                continue;
            wstring installDir(loc);
            // find the biggest non-helper exe in the game dir
            wstring exe, exeAny; uint64_t best = 0, bestAny = 0;
            WIN32_FIND_DATAW fd;
            HANDLE h = FindFirstFileW((installDir + L"\\*.exe").c_str(), &fd);
            if (h == INVALID_HANDLE_VALUE) continue;
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                ULARGE_INTEGER sz2{ fd.nFileSizeLow, fd.nFileSizeHigh };
                if (sz2.QuadPart > bestAny) { bestAny = sz2.QuadPart; exeAny = installDir + L"\\" + fd.cFileName; }
                if (!helperExe(fd.cFileName) && sz2.QuadPart > best) { best = sz2.QuadPart; exe = installDir + L"\\" + fd.cFileName; }
            } while (FindNextFileW(h, &fd));
            FindClose(h);
            if (exe.empty()) exe = exeAny;
            if (exe.empty()) continue;
            wstring nm = sub;
            if (lower(nm).find(L"riot") != wstring::npos) continue;   // skip the client itself
            Game g;
            g.name = nm;
            g.exePath = exe;
            g.launcher = L"Riot";
            g.id = lower(niceName(exe));
            addGame(out, g);
        }
        RegCloseKey(k);
    }
}

static void scanGog(vector<Game>& out) {
    // GOG registers every game under HKLM\SOFTWARE\WOW6432Node\GOG.com\Games\<id>
    HKEY k;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\GOG.com\\Games", 0, KEY_READ, &k) != ERROR_SUCCESS) return;
    DWORD idx = 0; wchar_t sub[128]; DWORD subLen = 128;
    while (RegEnumKeyExW(k, idx++, sub, &subLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        subLen = 128;
        wstring keyPath = wstring(L"SOFTWARE\\WOW6432Node\\GOG.com\\Games\\") + sub;
        wchar_t nm[256] = L"", exe[512] = L"";
        DWORD sz = sizeof(nm);
        if (RegGetValueW(HKEY_LOCAL_MACHINE, keyPath.c_str(), L"GameName", RRF_RT_REG_SZ, nullptr, nm, &sz) != ERROR_SUCCESS) continue;
        sz = sizeof(exe);
        RegGetValueW(HKEY_LOCAL_MACHINE, keyPath.c_str(), L"EXE", RRF_RT_REG_SZ, nullptr, exe, &sz);
        if (!exe[0]) continue;
        Game g;
        g.name = nm;
        g.exePath = exe;
        g.launcher = L"GOG";
        g.id = lower(niceName(exe));
        addGame(out, g);
    }
    RegCloseKey(k);
}

static void scanRegistryApps(vector<Game>& out) {
    // Uninstall keys: DisplayName + InstallLocation + DisplayIcon -> real games with icons
    const wchar_t* keys[] = {
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
    };
    const wchar_t* launchers[] = {
        L"Epic Games", L"Riot Games", L"GOG.com", L"Battle.net", L"Ubisoft Connect",
        L"EA Games", L"Xbox Games", L"Rockstar Games", L"Bethesda", L"Paradox Interactive",
    };
    for (auto& root : keys) {
        HKEY k;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, root, 0, KEY_READ, &k) != ERROR_SUCCESS) continue;
        DWORD idx = 0; wchar_t sub[256]; DWORD subLen = 256;
        while (RegEnumKeyExW(k, idx++, sub, &subLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            subLen = 256;
            wchar_t name[256] = L"", loc[512] = L"", icon[512] = L"";
            DWORD sz = sizeof(name);
            if (RegGetValueW(HKEY_LOCAL_MACHINE, (wstring(root) + L"\\" + sub).c_str(), L"DisplayName", RRF_RT_REG_SZ, nullptr, name, &sz) != ERROR_SUCCESS)
                continue;
            sz = sizeof(loc);
            RegGetValueW(HKEY_LOCAL_MACHINE, (wstring(root) + L"\\" + sub).c_str(), L"InstallLocation", RRF_RT_REG_SZ, nullptr, loc, &sz);
            sz = sizeof(icon);
            RegGetValueW(HKEY_LOCAL_MACHINE, (wstring(root) + L"\\" + sub).c_str(), L"DisplayIcon", RRF_RT_REG_SZ, nullptr, icon, &sz);

            // keep only entries that live under a known launcher dir (avoids catching every app)
            wstring l = lower(loc);
            wstring launcher = L"Standalone";
            for (auto& ln : launchers) {
                if (l.find(lower(ln)) != wstring::npos) { launcher = ln; break; }
            }
            if (launcher == L"Standalone") continue;
            if (lower(name).find(L"launcher") != wstring::npos) continue;   // launcher app, not a game

            // exe: from DisplayIcon, else the biggest exe in InstallLocation
            wstring exe = icon;
            size_t comma = exe.rfind(L',');                     // DisplayIcon may end with ",0"
            if (comma != wstring::npos) exe = exe.substr(0, comma);
            if (!exe.empty() && exe.front() == L'"') exe = exe.substr(1);   // DisplayIcon is often quoted
            if (!exe.empty() && exe.back() == L'"') exe.pop_back();
            if (exe.empty() || exe.rfind(L".exe") == wstring::npos) {
                // search InstallLocation
                exe.clear();
                if (!loc[0]) continue;
                WIN32_FIND_DATAW fd;
                HANDLE h2 = FindFirstFileW((wstring(loc) + L"\\*.exe").c_str(), &fd);
                uint64_t best = 0;
                if (h2 != INVALID_HANDLE_VALUE) {
                    do {
                        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                        ULARGE_INTEGER s2{ fd.nFileSizeLow, fd.nFileSizeHigh };
                        if (s2.QuadPart > best) { best = s2.QuadPart; exe = wstring(loc) + L"\\" + fd.cFileName; }
                    } while (FindNextFileW(h2, &fd));
                    FindClose(h2);
                }
            }
            if (exe.empty()) continue;
            Game g;
            g.name = name;
            g.exePath = exe;
            g.launcher = launcher;
            g.id = lower(niceName(exe.substr(exe.rfind(L'\\') + 1)));
            addGame(out, g);
        }
        RegCloseKey(k);
    }
}

vector<Game> detect() {
    vector<Game> out;
    scanSteam(out);
    scanEpic(out);
    scanRiot(out);
    scanGog(out);
    scanRegistryApps(out);

    // running? compare against process list
    auto running = gameboost::listProcesses();
    for (auto& g : out) {
        wstring exeName = lower(g.exePath.substr(g.exePath.rfind(L'\\') + 1));
        for (auto& r : running)
            if (lower(r.name) == exeName) { g.running = true; break; }
        // cached icon? (flat filename derived from id)
        wstring cache = iconCacheDir() + L"\\" + safeIconId(g.id) + L".png";
        if (fs::exists(cache)) g.iconPath = safeIconId(g.id);
    }
    return out;
}

// ---------------------------------------------------------------- profiles
json getProfile(const wstring& gameId) {
    json cfg = engine::loadConfig();
    if (cfg.contains("game_profiles") && cfg["game_profiles"].contains(narrow(gameId)))
        return cfg["game_profiles"][narrow(gameId)];
    return json::object();
}

bool saveProfile(const wstring& gameId, const json& p) {
    json cfg = engine::loadConfig();
    cfg["game_profiles"][narrow(gameId)] = p;
    log::ok(L"Saved profile for game: " + gameId);
    return engine::saveConfig(cfg);
}

bool applyProfile(const wstring& gameId, wstring& err) {
    json p = getProfile(gameId);
    if (p.empty()) { err = L"No profile for " + gameId; return false; }

    if (p.value("priority", "") == "high") {
        // persistent via IFEO PerfOptions - applies whenever this exe launches
        wstring exeName = gameId + L".exe";
        if (!gameboost::setPersistentPriority(exeName, 5, err)) return false;
        log::ok(L"Persistent HIGH priority for " + exeName);
    }
    if (p.value("disable_fso", false)) {
        tweaks::apply("fso_on", err);
        log::ok(L"Fullscreen optimizations disabled (global)");
    }
    log::ok(L"Game profile applied: " + gameId);
    return true;
}

bool clearProfile(const wstring& gameId, wstring& err) {
    if (!gameboost::setPersistentPriority(gameId + L".exe", 3, err)) return false;
    log::ok(L"Game profile cleared: " + gameId);
    return true;
}

// ------------------------------------------------------------- gaming mode
static GamingSnapshot g_snap;
static std::vector<DWORD> g_boosted;

bool gamingModeActive() { return g_snap.active; }

bool gamingModeEnter(const wstring& gameId, wstring& err) {
    if (g_snap.active) { err = L"Gaming mode already active"; return false; }
    log::section(L"GAMING MODE ENTER");

    // snapshot power plan
    g_snap.powerPlanBefore = sysinfo::activePowerPlanGuid();
    // apply ultimate plan
    tweaks::apply("power_ultimate", err);

    // boost the game process if it is running
    auto procs = gameboost::listProcesses();
    wstring want = lower(gameId);
    for (auto& pr : procs) {
        if (want.empty() || lower(pr.name).find(want) != wstring::npos) {
            if (gameboost::setPriority(pr.pid, 5, err)) {
                g_boosted.push_back(pr.pid);
                log::ok(L"Boosted priority: " + pr.name + L" (pid " + std::to_wstring(pr.pid) + L")");
            }
        }
    }

    // end background apps the user allowed killing (config: gaming_kill_list, array of exe names)
    json cfg = engine::loadConfig();
    if (cfg.contains("gaming_kill_list") && cfg["gaming_kill_list"].is_array()) {
        for (auto& item : cfg["gaming_kill_list"]) {
            wstring exe = widen(item.get<string>());
            for (auto& pr : procs) {
                if (lower(pr.name) == lower(exe)) {
                    if (gameboost::killProcess(pr.pid, err))
                        log::ok(L"Killed (user-approved list): " + pr.name);
                }
            }
        }
    }

    g_snap.active = true;
    log::ok(L"Gaming mode active" + (gameId.empty() ? L"" : (L" for " + gameId)));
    return true;
}

bool gamingModeExit(wstring& err) {
    if (!g_snap.active) return true;
    log::section(L"GAMING MODE EXIT — restoring");
    // restore power plan
    if (!g_snap.powerPlanBefore.empty()) {
        wstring cmd = L"powercfg /setactive " + g_snap.powerPlanBefore;
        string o;
        if (runCapture(cmd, o, 15000)) log::ok(L"Power plan restored");
        else log::fail(L"Power plan restore failed");
    }
    // priorities fall back when the game exits; clear any lingering boost
    for (DWORD pid : g_boosted) {
        wstring e2;
        gameboost::setPriority(pid, 3, e2);
    }
    g_boosted.clear();
    g_snap.active = false;
    log::ok(L"Gaming mode ended — everything restored");
    return true;
}

} // namespace ok::games
