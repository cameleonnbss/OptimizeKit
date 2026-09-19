// OptimizeKit - common helpers
#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

namespace ok {

using std::string;
using std::wstring;
using std::vector;

inline wstring widen(const string& s) {
    if (s.empty()) return wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    wstring r(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &r[0], n);
    return r;
}
inline string narrow(const wstring& s) {
    if (s.empty()) return string();
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    string r(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), &r[0], n, nullptr, nullptr);
    return r;
}

wstring fmtBytes(uint64_t b);        // "16.0 GB"
wstring fmtFloat(double v);          // "12.3" (1 decimal)
wstring fmtDuration(DWORD seconds);  // "1h 02m 03s"
bool isAdmin();                      // elevated token?
bool is64bit();
wstring userName();
wstring appDataDir();                // %LOCALAPPDATA%\OptimizeKit (created)
wstring configPath();                // ...\config.json
wstring logPath();                   // ...\OptimizeKit.log
wstring backupPath();                // ...\backup_YYYYMMDD_HHMMSS.reg
wstring exePath();                   // current module path
wstring exeDir();

// true = exit code 0. Captures combined stdout+stderr (UTF-8, re-encoded from console CP).
bool runCapture(const wstring& cmdLine, string& out, DWORD timeoutMs = 180000);
// fire-and-forget (opens URLs etc.)
void shellOpen(const wstring& url);
// write a UTF-8 text file (BOM-free)
bool writeTextFile(const wstring& path, const string& utf8);

// ---- logging used by every subsystem (append-only, thread-safe) ----
namespace log {
void init();                              // open/append, write header
void clear();                             // truncate + header (GUI start / script start)
void line(const wstring& msg);            // [2026-09-18 21:04:07] message
void section(const wstring& title);       // big separator header
void ok(const wstring& msg);
void fail(const wstring& msg);
void info(const wstring& msg);
wstring readAll();                        // whole file as text (for the GUI log tab)
wstring path();
}

} // namespace ok
