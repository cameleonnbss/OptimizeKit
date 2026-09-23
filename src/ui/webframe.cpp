// OptimizeKit - real desktop window hosting the dashboard through WebView2.
// Ships WebView2Loader.dll next to the exe; the Evergreen runtime is
// preinstalled on Windows 10/11. No injection, no game hooking - the web
// dashboard renders inside our own frame with the app icon and a dark border.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <shlobj.h>
#include "webframe.h"
#include "webview2.h"
#include "logging2.h"
#include <string>
#include <atomic>

// MinGW's <rpcndr.h> defines MIDL_INTERFACE(x) as a bare `struct`, dropping the
// __declspec(uuid(...)) that MSVC attaches - so __uuidof(ICoreWebView2*) has no
// definition and the link fails. Re-declare the IIDs we actually query.
__CRT_UUID_DECL(ICoreWebView2Controller2, 0xc979903e, 0xd4ca, 0x4228, 0x92, 0xeb, 0x47, 0xee, 0x3f, 0xa9, 0x6e, 0xab)
__CRT_UUID_DECL(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
                0x6c4819f3, 0xc9b7, 0x4260, 0x81, 0x27, 0xc9, 0xf5, 0xbd, 0xe7, 0xf6, 0x8c)
__CRT_UUID_DECL(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
                0x4e8a3389, 0xc9d8, 0x4bd2, 0xb6, 0xb5, 0x12, 0x4f, 0xee, 0x6c, 0xc1, 0x4d)
__CRT_UUID_DECL(ICoreWebView2DocumentTitleChangedEventHandler,
                0xf50f11d9, 0x8ea0, 0x4ba0, 0xa3, 0xf1, 0xa3, 0x97, 0x15, 0xd4, 0x6b, 0x3e)
__CRT_UUID_DECL(ICoreWebView2_12,
                0xb48663da, 0x09da, 0x4aa9, 0xb9, 0xf9, 0xd0, 0x35, 0x5c, 0x27, 0xc3, 0x9d)

namespace ok::webframe {

// WebView2.h is plain COM (no WRL dependency) - interfaces are used directly.

typedef HRESULT(WINAPI* PFN_CreateCoreWebView2EnvironmentWithOptions)(
    PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions*,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);
typedef HRESULT(WINAPI* PFN_GetAvailableCoreWebView2BrowserVersionString)(PCWSTR, LPWSTR*);

static PFN_CreateCoreWebView2EnvironmentWithOptions pCreateEnv = nullptr;
static PFN_GetAvailableCoreWebView2BrowserVersionString pGetVersion = nullptr;
static bool g_loaderTried = false;

static bool loadLoader() {
    if (g_loaderTried) return pCreateEnv != nullptr;
    g_loaderTried = true;
    const wstring local = exeDir() + L"\\WebView2Loader.dll";
    HMODULE h = LoadLibraryW(local.c_str());
    if (!h) h = LoadLibraryW(L"WebView2Loader.dll");
    if (!h) return false;
    pCreateEnv = reinterpret_cast<PFN_CreateCoreWebView2EnvironmentWithOptions>(
        reinterpret_cast<void*>(GetProcAddress(h, "CreateCoreWebView2EnvironmentWithOptions")));
    pGetVersion = reinterpret_cast<PFN_GetAvailableCoreWebView2BrowserVersionString>(
        reinterpret_cast<void*>(GetProcAddress(h, "GetAvailableCoreWebView2BrowserVersionString")));
    return pCreateEnv != nullptr;
}

bool available() {
    if (!loadLoader()) return false;
    if (!pGetVersion) return false;
    LPWSTR ver = nullptr;
    const HRESULT hr = pGetVersion(nullptr, &ver);
    if (FAILED(hr) || !ver) return false;
    CoTaskMemFree(ver);
    return true;
}

// ---------------------------------------------------------------- state
struct WebState {
    HWND hwnd = nullptr;
    ICoreWebView2* wv = nullptr;
    ICoreWebView2Controller* ctrl = nullptr;
    wstring url;
    std::atomic<bool> failed{false};
};

static WebState g_st;

static void applyBounds() {
    if (!g_st.ctrl) return;
    RECT rc;
    if (!GetClientRect(g_st.hwnd, &rc)) return;
    g_st.ctrl->put_Bounds({0, 0, rc.right, rc.bottom});
}

// ------------------------------------------------------------ COM handlers
class EnvHandler : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
public:
    explicit EnvHandler(LONG refs = 1) : refs_(refs) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (riid == __uuidof(IUnknown) ||
            riid == __uuidof(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)) {
            *ppv = static_cast<IUnknown*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&refs_); }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG r = InterlockedDecrement(&refs_);
        if (!r) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Environment* env) override {
        if (FAILED(errorCode) || !env) {
            g_st.failed = true;
            PostQuitMessage(3);
            return S_OK;
        }
        env->CreateCoreWebView2Controller(g_st.hwnd, new CtrlHandler());
        return S_OK;
    }

private:
    class CtrlHandler : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
    public:
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
            if (!ppv) return E_POINTER;
            *ppv = nullptr;
            if (riid == __uuidof(IUnknown) ||
                riid == __uuidof(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)) {
                *ppv = static_cast<IUnknown*>(this);
                AddRef();
                return S_OK;
            }
            return E_NOINTERFACE;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&refs_); }
        ULONG STDMETHODCALLTYPE Release() override {
            const ULONG r = InterlockedDecrement(&refs_);
            if (!r) delete this;
            return r;
        }
        HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Controller* ctrl) override {
            if (FAILED(errorCode) || !ctrl) {
                g_st.failed = true;
                PostQuitMessage(4);
                return S_OK;
            }
            g_st.ctrl = ctrl;
            ctrl->get_CoreWebView2(&g_st.wv);
            if (g_st.wv) {
                ICoreWebView2Settings* st = nullptr;
                if (SUCCEEDED(g_st.wv->get_Settings(&st)) && st) {
                    st->put_AreDevToolsEnabled(FALSE);
                    st->put_IsStatusBarEnabled(FALSE);
                    st->put_IsZoomControlEnabled(FALSE);
                    // context menu stays ON: users copy ping values / tweak ids from it
                    st->Release();
                }
                // dynamic window title -> the tab header the dashboard shows (v2.3+)
                ICoreWebView2_12* wv12 = nullptr;
                if (SUCCEEDED(g_st.wv->QueryInterface(__uuidof(ICoreWebView2_12),
                                                      reinterpret_cast<void**>(&wv12))) &&
                    wv12) {
                    class TitleHandler
                        : public ICoreWebView2DocumentTitleChangedEventHandler {
                    public:
                        explicit TitleHandler(LONG refs = 1) : refs_(refs) {}
                        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
                            if (!ppv) return E_POINTER;
                            *ppv = nullptr;
                            if (riid == __uuidof(IUnknown) ||
                                riid == __uuidof(ICoreWebView2DocumentTitleChangedEventHandler)) {
                                *ppv = static_cast<IUnknown*>(this);
                                AddRef();
                                return S_OK;
                            }
                            return E_NOINTERFACE;
                        }
                        ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&refs_); }
                        ULONG STDMETHODCALLTYPE Release() override {
                            const ULONG r = InterlockedDecrement(&refs_);
                            if (!r) delete this;
                            return r;
                        }
                        HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, IUnknown*) override {
                            LPWSTR t = nullptr;
                            if (SUCCEEDED(sender->get_DocumentTitle(&t)) && t) {
                                wchar_t full[256];
                                swprintf(full, 256, L"%ls - OptimizeKit", t);
                                SetWindowTextW(GetForegroundWindow(), full);
                                CoTaskMemFree(t);
                            }
                            return S_OK;
                        }
                        LONG refs_;
                    };
                    EventRegistrationToken titleTok;
                    wv12->add_DocumentTitleChanged(new TitleHandler(), &titleTok);
                    wv12->Release();
                }
                g_st.wv->Navigate(g_st.url.c_str());
            }
            // opaque dark page background - no white flash during load
            ICoreWebView2Controller2* ctrl2 = nullptr;
            if (SUCCEEDED(ctrl->QueryInterface(__uuidof(ICoreWebView2Controller2),
                                               reinterpret_cast<void**>(&ctrl2))) &&
                ctrl2) {
                ctrl2->put_DefaultBackgroundColor({255, 9, 10, 15});
                ctrl2->Release();
            }
            applyBounds();
            ctrl->put_IsVisible(TRUE);
            ShowWindow(g_st.hwnd, SW_SHOW);
            return S_OK;
        }
        LONG refs_ = 1;
    };

    LONG refs_;
};

// --------------------------------------------------------------- window
static LRESULT CALLBACK frameProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SIZE:
            if (wp != SIZE_MINIMIZED) applyBounds();
            return 0;
        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
            mmi->ptMinTrackSize = {980, 620};
            return 0;
        }
        case WM_ACTIVATE:
            // keep browser shortcuts alive inside the frame (F5, Ctrl+R, Ctrl+F, Ctrl+P)
            if (LOWORD(wp) != WA_INACTIVE && g_st.ctrl) {
                g_st.ctrl->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

int runWindow(const wstring& url) {
    if (!available()) return -1;

    // per-monitor DPI v2 - crisp text on scaled displays
    if (HMODULE u32 = GetModuleHandleW(L"user32.dll")) {
        using SetCtx_t = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        auto setCtx = reinterpret_cast<SetCtx_t>(
            reinterpret_cast<void*>(GetProcAddress(u32, "SetProcessDpiAwarenessContext")));
        if (setCtx) setCtx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }

    WNDCLASSW wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = frameProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hIcon = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(1));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(9, 10, 15));
    wc.lpszClassName = L"OptimizeKitFrame";
    RegisterClassW(&wc);

    RECT wa{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    const int w = 1320, h = 880;
    const int x = wa.left + ((wa.right - wa.left) - w) / 2;
    const int y = wa.top + ((wa.bottom - wa.top) - h) / 2;

    g_st.url = url;
    g_st.hwnd = CreateWindowExW(
        0, wc.lpszClassName,
        L"OptimizeKit - Windows Gaming & Performance Control Center",
        WS_OVERLAPPEDWINDOW, x, y, w, h, nullptr, nullptr, wc.hInstance, nullptr);
    if (!g_st.hwnd) return -2;

    // dark title bar on Windows 10/11
    BOOL dark = TRUE;
    DwmSetWindowAttribute(g_st.hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

    // webview data lives in our own appdata - nothing in the install dir
    wchar_t* localRaw = nullptr;
    wstring userData = (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localRaw) == S_OK &&
                        localRaw)
                           ? (wstring(localRaw) + L"\\OptimizeKit\\WebView2")
                           : (exeDir() + L"\\webviewdata");
    if (localRaw) CoTaskMemFree(localRaw);

    ShowWindow(g_st.hwnd, SW_HIDE);
    UpdateWindow(g_st.hwnd);

    const HRESULT hr =
        pCreateEnv(nullptr, userData.c_str(), nullptr, new EnvHandler());
    if (FAILED(hr)) {
        log2::error(L"WEBFRAME", L"WebView2 environment creation failed (0x" +
                                     widen(std::to_string((unsigned long)hr)) + L")");
        g_st.failed = true;
    }

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }

    if (g_st.ctrl) { g_st.ctrl->Close(); g_st.ctrl->Release(); g_st.ctrl = nullptr; }
    if (g_st.wv) { g_st.wv->Release(); g_st.wv = nullptr; }

    const int code = g_st.failed ? -3 : 0;
    if (g_st.failed)
        log2::warn(L"WEBFRAME", L"native window did not initialize - falling back to the browser");
    return code;
}

} // namespace ok::webframe
