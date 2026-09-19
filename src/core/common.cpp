#include "common.h"
#include <shellapi.h>
#include <shlobj.h>
#include <userenv.h>
#include <fstream>
#include <sstream>
#include <ctime>
#include <cstdio>
#include <mutex>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "userenv.lib")

namespace ok {

wstring fmtBytes(uint64_t b) {
    wchar_t buf[64];
    double gb = (double)b / (1024.0 * 1024.0 * 1024.0);
    if (gb >= 1.0) { swprintf(buf, 64, L"%.1f GB", gb); return buf; }
    double mb = (double)b / (1024.0 * 1024.0);
    if (mb >= 1.0) { swprintf(buf, 64, L"%.0f MB", mb); return buf; }
    swprintf(buf, 64, L"%llu KB", b / 1024ull);
    return buf;
}

wstring fmtFloat(double v) {
    wchar_t buf[32];
    swprintf(buf, 32, L"%.1f", v);
    return buf;
}

wstring fmtDuration(DWORD sec) {
    wchar_t buf[64];
    DWORD h = sec / 3600, m = (sec % 3600) / 60, s = sec % 60;
    if (h) swprintf(buf, 64, L"%02uh %02um %02us", h, m, s);
    else   swprintf(buf, 64, L"%02u:%02u:%02u", m, s);
    return buf;
}

bool isAdmin() {
    BOOL elevated = FALSE;
    HANDLE tok = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
        TOKEN_ELEVATION e{};
        DWORD cb = 0;
        if (GetTokenInformation(tok, TokenElevation, &e, sizeof(e), &cb)) elevated = e.TokenIsElevated;
        CloseHandle(tok);
    }
    return elevated != FALSE;
}

bool is64bit() {
#if defined(_WIN64)
    return true;
#else
    BOOL wow = FALSE;
    IsWow64Process(GetCurrentProcess(), &wow);
    return wow != FALSE;
#endif
}

wstring userName() {
    wchar_t buf[256]; DWORD n = 256;
    if (GetUserNameW(buf, &n)) return buf;
    return L"user";
}

static void ensureDir(const wstring& dir) {
    SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr); // OK if it already exists
}

wstring appDataDir() {
    wchar_t* p = nullptr;
    wstring base;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &p))) {
        base = wstring(p) + L"\\OptimizeKit";
        CoTaskMemFree(p);
    } else {
        base = L"C:\\ProgramData\\OptimizeKit";
    }
    ensureDir(base);
    return base;
}

wstring configPath() { return appDataDir() + L"\\config.json"; }
wstring logPath()    { return appDataDir() + L"\\OptimizeKit.log"; }

wstring backupPath() {
    time_t t = time(nullptr);
    tm lt{};
    localtime_s(&lt, &t);
    wchar_t buf[64];
    swprintf(buf, 64, L"\\backup_%04d%02d%02d_%02d%02d%02d.reg",
             lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);
    return appDataDir() + buf;
}

wstring exePath() {
    wchar_t buf[MAX_PATH + 2] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH + 1);
    return buf;
}
wstring exeDir() {
    wstring p = exePath();
    size_t i = p.find_last_of(L"\\/");
    return (i == wstring::npos) ? L"." : p.substr(0, i);
}

bool runCapture(const wstring& cmdLine, string& out, DWORD timeoutMs) {
    out.clear();
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return false;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = wr;
    si.hStdError  = wr;

    wstring cmd = cmdLine; // CreateProcess needs mutable buffer
    PROCESS_INFORMATION pi{};
    BOOL created = CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, TRUE,
                                  CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(wr);
    if (!created) { CloseHandle(rd); return false; }

    const DWORD CHUNK = 8192;
    std::vector<char> acc;
    char tmp[8192];
    DWORD read = 0, waitResult = 0, elapsed = 0;
    bool timedOut = false;
    for (;;) {
        waitResult = WaitForSingleObject(pi.hProcess, 200);
        for (;;) {
            if (!PeekNamedPipe(rd, nullptr, 0, nullptr, &read, nullptr) || read == 0) break;
            DWORD got = 0;
            if (ReadFile(rd, tmp, CHUNK, &got, nullptr) && got) acc.insert(acc.end(), tmp, tmp + got);
            else break;
        }
        if (waitResult == WAIT_OBJECT_0) {
            // drain what's left
            for (;;) {
                DWORD got = 0;
                if (!ReadFile(rd, tmp, CHUNK, &got, nullptr) || !got) break;
                acc.insert(acc.end(), tmp, tmp + got);
            }
            break;
        }
        if (waitResult == WAIT_TIMEOUT && timeoutMs != INFINITE) {
            elapsed += 200;
            if (elapsed >= timeoutMs) { TerminateProcess(pi.hProcess, 1); timedOut = true; break; }
        }
    }
    CloseHandle(rd);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    // console output is usually in the OEM/ANSI CP; convert to UTF-8
    UINT cp = GetOEMCP();
    int wn = MultiByteToWideChar(cp, 0, acc.data(), (int)acc.size(), nullptr, 0);
    wstring wide(wn, L'\0');
    MultiByteToWideChar(cp, 0, acc.data(), (int)acc.size(), &wide[0], wn);
    out = narrow(wide);
    return !timedOut;
}

void shellOpen(const wstring& url) {
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

bool writeTextFile(const wstring& path, const string& utf8) {
    std::ofstream f(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(utf8.data(), (std::streamsize)utf8.size());
    return (bool)f;
}

// ============================ logging ============================
namespace log {

static std::mutex g_mtx;

static FILE* openFile(const wchar_t* mode) {
    FILE* f = nullptr;
    _wfopen_s(&f, logPath().c_str(), mode);
    return f;
}

static void writeRaw(const wstring& s) {
    std::lock_guard<std::mutex> g(g_mtx);
    FILE* f = openFile(L"a, ccs=UTF-8");
    if (!f) return;
    fwprintf(f, L"%s\n", s.c_str());
    fclose(f);
}

static wstring now() {
    time_t t = time(nullptr);
    tm lt{};
    localtime_s(&lt, &t);
    wchar_t buf[40];
    swprintf(buf, 40, L"[%04d-%02d-%02d %02d:%02d:%02d]",
             lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);
    return buf;
}

void init()   { writeRaw(now() + L" OptimizeKit session started (admin=" + (isAdmin() ? L"yes" : L"no") + L")"); }
void clear() {
    FILE* f = openFile(L"w, ccs=UTF-8");
    if (f) fclose(f);
    init();
}
void line(const wstring& msg)    { writeRaw(now() + L" " + msg); }
void section(const wstring& t)   { writeRaw(L"\n==================================================\n  " + t + L"\n=================================================="); }
void ok(const wstring& msg)      { line(L"[OK]   " + msg); }
void fail(const wstring& msg)    { line(L"[FAIL] " + msg); }
void info(const wstring& msg)    { line(L"[INFO] " + msg); }

wstring readAll() {
    std::lock_guard<std::mutex> g(g_mtx);
    FILE* f = openFile(L"r, ccs=UTF-8");
    if (!f) return L"(log not created yet)";
    wstring all;
    wchar_t buf[2048];
    while (fgetws(buf, 2048, f)) all += buf;
    fclose(f);
    return all;
}
wstring path() { return logPath(); }

} // namespace log
} // namespace ok
