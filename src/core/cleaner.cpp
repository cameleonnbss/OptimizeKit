#include "cleaner.h"
#include <shellapi.h>
#include <filesystem>

#pragma comment(lib, "shell32.lib")

namespace fs = std::filesystem;

namespace ok::cleaner {

static vector<wstring> junkDirs() {
    vector<wstring> v;
    auto add = [&](const wchar_t* env, const wchar_t* sub) {
        wchar_t buf[MAX_PATH] = {};
        if (GetEnvironmentVariableW(env, buf, MAX_PATH)) {
            wstring p = buf;
            if (sub && *sub) p += wstring(L"\\") + sub;
            v.push_back(p);
        }
    };
    add(L"TEMP", nullptr);
    add(L"TMP", nullptr);
    add(L"LOCALAPPDATA", L"Temp");
    add(L"LOCALAPPDATA", L"Microsoft\\Windows\\INetCache");
    add(L"LOCALAPPDATA", L"Microsoft\\Windows\\Explorer");         // thumbcache
    add(L"LOCALAPPDATA", L"CrashDumps");
    add(L"LOCALAPPDATA", L"NVIDIA\\DXCache");
    add(L"LOCALAPPDATA", L"NVIDIA\\GLCache");
    add(L"LOCALAPPDATA", L"AMD\\DxCache");
    add(L"LOCALAPPDATA", L"AMD\\DxcCache");
    add(L"LOCALAPPDATA", L"D3DSCache");
    add(L"LOCALAPPDATA", L"Microsoft\\Windows\\WER");              // windows error reporting
    add(L"ProgramData", L"Microsoft\\Windows\\WER");
    add(L"WINDIR", L"Temp");
    return v;
}

static uint64_t sizeOfTree(const wstring& dir) {
    uint64_t bytes = 0;
    std::error_code ec;
    for (auto& p : fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec))
        if (p.is_regular_file(ec)) bytes += p.file_size(ec);
    return bytes;
}

Report measure() {
    Report r;
    for (auto& d : junkDirs()) {
        Entry e;
        e.path = d;
        size_t pos = d.find_last_of(L"\\/");
        e.label = (pos == wstring::npos) ? d : d.substr(pos + 1);
        std::error_code ec;
        e.bytes = fs::exists(d, ec) ? sizeOfTree(d) : 0;
        r.entries.push_back(e);
        r.total += e.bytes;
    }
    return r;
}

static uint64_t purgeTop(const wstring& dir) {
    std::error_code ec;
    if (!fs::exists(dir, ec)) return 0;
    uint64_t before = sizeOfTree(dir);
    for (auto& p : fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec)) {
        std::error_code ec2;
        // never follow symlinks/junctions out of the junk dir
        if (fs::is_symlink(fs::symlink_status(p.path(), ec2))) continue;
        fs::remove_all(p.path(), ec2);
    }
    return before - sizeOfTree(dir);
}

uint64_t purge() {
    uint64_t freed = 0;
    for (auto& d : junkDirs()) {
        freed += purgeTop(d);
    }
    log::ok(L"cleaner freed " + fmtBytes(freed));
    return freed;
}

void emptyRecycleBin() {
    SHEmptyRecycleBinW(nullptr, nullptr,
        SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
    log::ok(L"recycle bin emptied");
}

bool componentCleanup(wstring& err) {
    if (!isAdmin()) { err = L"administrator required"; return false; }
    string out;
    runCapture(L"Dism.exe /Online /Cleanup-Image /StartComponentCleanup", out, 600000);
    log::ok(L"DISM component cleanup done");
    return true;
}

} // namespace ok::cleaner
