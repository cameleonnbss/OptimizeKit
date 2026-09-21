// OptimizeKit v2.6 - DiskScope-inspired storage implementation.
// Ported from github.com/cameleonnbss/DiskScope-CLI (scanner.py / cleanup.py / files.py)
// to plain Win32. Same philosophy: measure only, never delete, bounded memory & time.
#include "diskscope.h"
#include <filesystem>
#include <fstream>
#include <functional>
#include <algorithm>
#include <bcrypt.h>
#include <shellapi.h>
#include <shlobj.h>

namespace fs = std::filesystem;

namespace ok::diskscope {

using json = nlohmann::json;

static wstring lower(wstring s) { std::transform(s.begin(), s.end(), s.begin(), ::towlower); return s; }

// ------------------------------------------------------------------ helpers
static uint64_t dirSizeQuick(const wstring& dir, int depth, uint64_t& files, int& budget) {
    if (depth <= 0 || budget <= 0) return 0;
    uint64_t total = 0;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (fd.cFileName[0] == L'.') continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;   // junction-safe
            --budget;
            total += dirSizeQuick(dir + L"\\" + fd.cFileName, depth - 1, files, budget);
        } else {
            ULARGE_INTEGER sz{ fd.nFileSizeLow, fd.nFileSizeHigh };
            total += sz.QuadPart;
            ++files;
        }
        if (budget <= 0) break;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return total;
}

// ------------------------------------------------------------------ drives + folders
json driveAndFolders(const wstring& drive, int topN, int budgetMs) {
    using clock = std::chrono::steady_clock;
    auto t0 = clock::now();
    auto elapsed = [&]() { return (int)std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - t0).count(); };

    json out;
    json drives = json::array();
    DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (!(mask & (1u << i))) continue;
        wstring L = wstring(1, (wchar_t)(L'A' + i)) + L":";
        ULARGE_INTEGER freeB{}, totalB{};
        if (!GetDiskFreeSpaceExW((L + L"\\").c_str(), nullptr, &totalB, &freeB)) continue;
        int type = GetDriveTypeW((L + L"\\").c_str());
        drives.push_back({ {"letter", narrow(L)}, {"total", (long long)totalB.QuadPart},
                           {"free", (long long)freeB.QuadPart},
                           {"bus", type == DRIVE_FIXED ? "fixed" : type == DRIVE_REMOVABLE ? "removable" : "other"} });
    }
    out["drives"] = drives;

    // first-level folders of the requested drive, with a bounded recursive size each
    json folders = json::array();
    wstring root = drive;
    if (root.size() == 2) root += L"\\";
    int globalBudget = 2600;   // folder visits, shared across the walk
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((root + L"*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        struct Row { wstring name; uint64_t bytes; uint64_t files; };
        vector<Row> rows;
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            if (fd.cFileName[0] == L'.') continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
            if (elapsed() > budgetMs) break;
            int budget = 900;                        // per-folder sub-visit budget
            uint64_t files = 0;
            uint64_t bytes = dirSizeQuick(root + fd.cFileName, 6, files, budget);
            globalBudget -= 900 - budget;
            rows.push_back({ fd.cFileName, bytes, files });
        } while (FindNextFileW(h, &fd));
        FindClose(h);
        std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.bytes > b.bytes; });
        if ((int)rows.size() > topN) rows.resize(topN);
        for (auto& r : rows)
            folders.push_back({ {"path", narrow(root + r.name)}, {"bytes", (long long)r.bytes}, {"files", (long long)r.files} });
    }
    out["folders"] = folders;
    out["truncated"] = elapsed() > budgetMs;
    return out;
}

// ------------------------------------------------------------------ SAFE cleanup targets
json cleanupTargets() {
    // Imported from DiskScope-CLI cleanup.py SAFE_TARGETS (same reasons, EN).
    wchar_t* la = nullptr; wstring local, roam;
    if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &la) == S_OK) { local = la; CoTaskMemFree(la); }
    wchar_t* ro = nullptr;
    if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &ro) == S_OK) { roam = ro; CoTaskMemFree(ro); }
    wchar_t winBuf[MAX_PATH] = L"C:\\Windows";
    GetWindowsDirectoryW(winBuf, MAX_PATH);
    wstring win = winBuf;

    struct T { wstring path; const wchar_t* reason; };
    vector<T> targets = {
        { local + L"\\Temp", L"user temp files" },
        { win + L"\\Temp", L"Windows temp files" },
        { local + L"\\Microsoft\\Windows\\Explorer", L"thumbnail caches" },
        { local + L"\\CrashDumps", L"crash dumps" },
        { local + L"\\D3DSCache", L"DirectX shader cache (regenerated)" },
        { local + L"\\NVIDIA\\DXCache", L"NVIDIA shader cache" },
        { local + L"\\NVIDIA\\GLCache", L"NVIDIA shader cache" },
        { local + L"\\AMD\\DxCache", L"AMD shader cache" },
        { local + L"\\AMD\\DxcCache", L"AMD shader cache" },
        { local + L"\\pip\\cache", L"pip cache" },
        { local + L"\\npm-cache", L"npm cache" },
        { local + L"\\Google\\Chrome\\User Data\\Default\\Cache", L"Chrome browser cache" },
        { local + L"\\Microsoft\\Edge\\User Data\\Default\\Cache", L"Edge browser cache" },
        { local + L"\\Packages", L"Store apps cache (only safe subfolders measured)" },
        { local + L"\\Steam\\htmlcache", L"Steam overlay HTML cache" },
        { local + L"\\Discord\\Cache", L"Discord cache" },
    };
    json arr = json::array();
    for (auto& t : targets) {
        int budget = 250; uint64_t files = 0;
        uint64_t bytes = dirSizeQuick(t.path, 4, files, budget);
        arr.push_back({ {"path", narrow(t.path)}, {"reason", narrow(t.reason)},
                        {"bytes", (long long)bytes}, {"files", (long long)files}, {"exists", bytes > 0} });
    }
    json out;
    out["targets"] = arr;
    uint64_t total = 0;
    for (auto& t : arr) total += t.value("bytes", (long long)0);
    out["total"] = (long long)total;
    return out;
}

// ------------------------------------------------------------------ duplicates
// bcrypt SHA-256, fetched lazily (MinGW has no secforwarder; bcrypt ships with Windows).
typedef NTSTATUS (WINAPI *BCryptHashData_t)(BCRYPT_HASH_HANDLE, PUCHAR, ULONG, ULONG);
typedef NTSTATUS (WINAPI *BCryptOpenAlg_t)(BCRYPT_ALG_HANDLE*, LPCWSTR, ULONG, ULONG);
typedef NTSTATUS (WINAPI *BCryptCloseAlg_t)(BCRYPT_ALG_HANDLE, ULONG);

static bool sha256Head(const wstring& path, unsigned char* out32, uint64_t headBytes, uint64_t skipBytes = 0) {
    static BCryptOpenAlg_t openAlg = nullptr;
    static BCryptHashData_t hashFn = nullptr;
    static BCryptCloseAlg_t closeAlg = nullptr;
    static BCRYPT_ALG_HANDLE alg = nullptr;
    if (!hashFn) {
        HMODULE b = LoadLibraryW(L"bcrypt.dll");
        if (!b) return false;
        openAlg = (BCryptOpenAlg_t)GetProcAddress(b, "BCryptOpenAlgorithmProvider");
        hashFn = (BCryptHashData_t)GetProcAddress(b, "BCryptHashData");
        // note: BCryptHashData signature matches our typedef for the hashing step
        closeAlg = (BCryptCloseAlg_t)GetProcAddress(b, "BCryptCloseAlgorithmProvider");
        if (!openAlg || !hashFn || openAlg(&alg, L"SHA256", 0, 0) != 0) return false;
    }
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) return false;
    if (skipBytes) f.seekg((std::streamoff)skipBytes);
    vector<char> buf((size_t)std::min<uint64_t>(headBytes, 1 << 20));
    f.read(buf.data(), (std::streamsize)buf.size());
    buf.resize((size_t)f.gcount());
    if (buf.empty()) return false;
    // BCryptHashData with a zero-length hash object buffer computes into out32 via BCryptFinishHash;
    // to keep the code tiny we hash with a full one-shot through BCryptHashData + Finish.
    typedef NTSTATUS (WINAPI *Finish_t)(BCRYPT_HASH_HANDLE, PUCHAR, ULONG, ULONG);
    HMODULE b = GetModuleHandleW(L"bcrypt.dll");
    auto finish = (Finish_t)GetProcAddress(b, "BCryptFinishHash");
    auto createHash = (NTSTATUS (WINAPI*)(BCRYPT_ALG_HANDLE, BCRYPT_HASH_HANDLE*, PUCHAR, ULONG, ULONG))GetProcAddress(b, "BCryptCreateHash");
    auto destroyHash = (NTSTATUS (WINAPI*)(BCRYPT_HASH_HANDLE, ULONG))GetProcAddress(b, "BCryptDestroyHash");
    if (!finish || !createHash || !destroyHash) return false;
    BCRYPT_HASH_HANDLE hh = nullptr;
    unsigned char obj[1024]; DWORD objLen = sizeof(obj); DWORD cbRes = 0;
    // query object length
    typedef NTSTATUS (WINAPI *Prop_t)(BCRYPT_ALG_HANDLE, LPCWSTR, PUCHAR, ULONG, ULONG*);
    auto prop = (Prop_t)GetProcAddress(b, "BCryptGetProperty");
    if (prop) prop(alg, L"ObjectLength", (PUCHAR)&objLen, sizeof(objLen), &cbRes);
    vector<unsigned char> objBuf(objLen);
    if (createHash(alg, &hh, objBuf.data(), objLen, 0) != 0) return false;
    bool ok = hashFn(hh, (PUCHAR)buf.data(), (ULONG)buf.size(), 0) == 0 && finish(hh, out32, 32, 0) == 0;
    destroyHash(hh, 0);
    return ok;
}

json duplicates(const wstring& rootW, int maxScan, int budgetMs) {
    using clock = std::chrono::steady_clock;
    auto t0 = clock::now();
    auto elapsed = [&]() { return (int)std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - t0).count(); };

    // 1) collect the biggest files, bounded like DiskScope's Collectors
    struct Cand { wstring path; uint64_t size; };
    vector<Cand> cands;
    cands.reserve(maxScan * 2);
    wstring root = rootW;
    if (root.size() == 2) root += L"\\";

    std::function<void(const wstring&, int, int&)> walk = [&](const wstring& dir, int depth, int& budget) {
        if (depth <= 0 || budget <= 0 || cands.size() >= (size_t)maxScan * 2) return;
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return;
        do {
            if (fd.cFileName[0] == L'.') continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
                --budget;
                walk(dir + L"\\" + fd.cFileName, depth - 1, budget);
            } else {
                ULARGE_INTEGER sz{ fd.nFileSizeLow, fd.nFileSizeHigh };
                if (sz.QuadPart > 32ull * 1024ull * 1024ull)            // only files > 32 MB are interesting
                    cands.push_back({ dir + L"\\" + fd.cFileName, sz.QuadPart });
            }
            if (elapsed() > budgetMs) break;
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    };
    int budget = 12000;
    walk(root, 8, budget);
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.size > b.size; });
    if ((int)cands.size() > maxScan) cands.resize(maxScan);

    // 2) group by size
    std::map<uint64_t, vector<const Cand*>> bySize;
    for (auto& c : cands) if (c.size > 0) bySize[c.size].push_back(&c);

    // 3) head-hash the size groups, full-hash the collisions
    json groups = json::array();
    uint64_t wasted = 0;
    int hashed = 0;
    for (auto& [size, list] : bySize) {
        if (list.size() < 2 || elapsed() > budgetMs) continue;
        std::map<string, vector<const Cand*>> byHead;
        for (auto* c : list) {
            unsigned char h32[32];
            if (!sha256Head(c->path, h32, 256ull * 1024ull)) continue;
            ++hashed;
            byHead[string((const char*)h32, 32)].push_back(c);
        }
        for (auto& [h, l2] : byHead) {
            if (l2.size() < 2) continue;
            // collision on the head: hash the tail (middle chunk) for confidence
            std::map<string, vector<const Cand*>> byTail;
            for (auto* c : l2) {
                unsigned char h32[32];
                uint64_t skip = c->size / 2;
                if (!sha256Head(c->path, h32, 1024ull * 1024ull, skip)) continue;
                byTail[string((const char*)h32, 32)].push_back(c);
            }
            for (auto& [h2, l3] : byTail) {
                if (l3.size() < 2) continue;
                json arr = json::array();
                for (auto* c : l3) arr.push_back(narrow(c->path));
                groups.push_back({ {"size", (long long)size}, {"count", (int)l3.size()},
                                   {"wasted", (long long)(size * (l3.size() - 1))}, {"paths", arr} });
                wasted += size * (l3.size() - 1);
                if (groups.size() >= 40) goto done;
            }
        }
    }
done:
    json out;
    out["groups"] = groups;
    out["wasted"] = (long long)wasted;
    out["hashed"] = hashed;
    out["scanned"] = (long long)cands.size();
    return out;
}

} // namespace ok::diskscope
