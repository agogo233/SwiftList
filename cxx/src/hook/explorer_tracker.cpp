#include "hook/explorer_tracker.h"

#include <shlobj.h>

// ShellWindowConstants and ShellWindowFindWindowOptions from <exdisp.h>.
// Defined here to avoid pulling in the full exdisp.h (heavy COM overhead).
#ifndef SWC_DESKTOP
#define SWC_DESKTOP 0x08
#endif
#ifndef SWFO_NEEDDISPLAY
#define SWFO_NEEDDISPLAY 0x01
#endif

namespace swiftlist::hook {

ExplorerTracker::ExplorerTracker() {
    instance_ = this;
}

ExplorerTracker::~ExplorerTracker() {
    Stop();
    instance_ = nullptr;
}

bool ExplorerTracker::Start() {
    if (hook_) return true;

    hook_ = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        nullptr, WinEventProc,
        0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    return hook_ != nullptr;
}

void ExplorerTracker::Stop() {
    if (hook_) {
        UnhookWinEvent(hook_);
        hook_ = nullptr;
    }
}

void ExplorerTracker::SetCallback(ChangeFn cb) {
    callback_ = std::move(cb);
}

void CALLBACK ExplorerTracker::WinEventProc(HWINEVENTHOOK /*hook*/, DWORD event,
                                              HWND hwnd, LONG /*idObject*/,
                                              LONG /*idChild*/,
                                              DWORD /*eventThread*/,
                                              DWORD /*eventTime*/) {
    if (instance_ && event == EVENT_SYSTEM_FOREGROUND) {
        instance_->HandleForeground(hwnd);
    }
}

void ExplorerTracker::HandleForeground(HWND hwnd) {
    WCHAR className[256] = {};
    GetClassNameW(hwnd, className, 256);

    bool isExplorer = (wcscmp(className, L"CabinetWClass") == 0);
    bool isDesktop = (wcscmp(className, L"Progman") == 0) ||
                     (wcscmp(className, L"WorkerW") == 0);

    if (!isExplorer && !isDesktop) {
        return;
    }

    current_.Handle = hwnd;
    current_.IsDesktop = isDesktop;

    GetWindowTextW(hwnd, current_.Caption.data(),
                    static_cast<int>(current_.Caption.size()));

    if (isDesktop) {
        current_.Path = L"";
    } else {
        IShellWindows* shellWindows = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_ShellWindows, nullptr,
                                        CLSCTX_ALL,
                                        IID_IShellWindows,
                                        reinterpret_cast<void**>(&shellWindows)))) {
            VARIANT v;
            VariantInit(&v);
            v.vt = VT_I4;
            v.lVal = 0;

            IDispatch* disp = nullptr;
            if (SUCCEEDED(shellWindows->FindWindowSW(&v, &v, SWC_DESKTOP,
                                                      reinterpret_cast<long*>(&hwnd),
                                                      SWFO_NEEDDISPLAY,
                                                      &disp)) && disp) {
                IServiceProvider* sp = nullptr;
                if (SUCCEEDED(disp->QueryInterface(IID_IServiceProvider,
                                                     reinterpret_cast<void**>(&sp)))) {
                    IShellBrowser* browser = nullptr;
                    if (SUCCEEDED(sp->QueryService(SID_STopLevelBrowser,
                                                     IID_IShellBrowser,
                                                     reinterpret_cast<void**>(&browser)))) {
                        IShellView* view = nullptr;
                        if (SUCCEEDED(browser->QueryActiveShellView(&view))) {
                            IFolderView2* folderView = nullptr;
                            if (SUCCEEDED(view->QueryInterface(IID_IFolderView2,
                                                                 reinterpret_cast<void**>(&folderView)))) {
                                IPersistFolder2* persist = nullptr;
                                if (SUCCEEDED(folderView->GetFolder(IID_IPersistFolder2,
                                                                      reinterpret_cast<void**>(&persist)))) {
                                    ITEMIDLIST* pidl = nullptr;
                                    if (SUCCEEDED(persist->GetCurFolder(&pidl))) {
                                        WCHAR path[MAX_PATH] = {};
                                        if (SHGetPathFromIDListW(pidl, path)) {
                                            current_.Path = path;
                                        }
                                        CoTaskMemFree(pidl);
                                    }
                                    persist->Release();
                                }
                                folderView->Release();
                            }
                            view->Release();
                        }
                        browser->Release();
                    }
                    sp->Release();
                }
                disp->Release();
            }
            shellWindows->Release();
        }
    }

    if (callback_) {
        callback_(current_);
    }
}

BOOL CALLBACK ExplorerTracker::EnumChildProc(HWND /*hwnd*/, LPARAM /*lParam*/) {
    return TRUE;
}

} // namespace swiftlist::hook
