#include "hook/keyboard_hook.h"
#include "hook/mouse_hook.h"
#include "hook/explorer_tracker.h"
#include "hook/inline_search_manager.h"

#include <csignal>
#include <cstdio>
#include <string>

using namespace swiftlist::hook;

static KeyboardHook* g_keyboard = nullptr;
static MouseHook* g_mouse = nullptr;
static ExplorerTracker* g_explorer = nullptr;
static InlineSearchManager* g_inline = nullptr;
static std::atomic<bool> g_running{true};

static void SignalHandler(int /*sig*/) {
    g_running = false;
}

static void OnKeyEvent(const KeyEvent& event) {
    switch (event.Type) {
    case KeyEventType::CtrlDoublePress:
        fwprintf(stdout, L"[Hook] Double Ctrl detected\n");
        break;
    case KeyEventType::Escape:
        fwprintf(stdout, L"[Hook] Escape\n");
        break;
    case KeyEventType::Enter:
        fwprintf(stdout, L"[Hook] Enter\n");
        break;
    case KeyEventType::Char:
        fwprintf(stdout, L"[Hook] Char: %c\n", event.Char);
        break;
    default:
        break;
    }
}

static void OnMouseEvent(const MouseEvent& event) {
    if (event.LeftButton) {
        fwprintf(stdout, L"[Hook] Left click at (%d, %d)%s\n",
                 event.X, event.Y,
                 event.DoubleClick ? L" (double)" : L"");
    }
}

static void OnExplorerChanged(const ExplorerInfo& info) {
    fwprintf(stdout, L"[Hook] Explorer: %s (desktop: %d)\n",
             info.Path.c_str(), info.IsDesktop ? 1 : 0);
}

int wmain(int argc, wchar_t* argv[]) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return 1;

    fwprintf(stdout, L"SwiftList Hook Process starting...\n");

    g_keyboard = new KeyboardHook();
    g_mouse = new MouseHook();
    g_explorer = new ExplorerTracker();
    g_inline = new InlineSearchManager();

    if (!g_keyboard->Install()) {
        fwprintf(stderr, L"Failed to install keyboard hook\n");
    }
    g_keyboard->SetCallback(OnKeyEvent);

    if (!g_mouse->Install()) {
        fwprintf(stderr, L"Failed to install mouse hook\n");
    }
    g_mouse->SetCallback(OnMouseEvent);

    if (!g_explorer->Start()) {
        fwprintf(stderr, L"Failed to start explorer tracker\n");
    }
    g_explorer->SetCallback(OnExplorerChanged);

    g_inline->SetAppName(L"User");
    g_inline->SetSessionId(WTSGetActiveConsoleSessionId());

    signal(SIGINT, SignalHandler);

    fwprintf(stdout, L"Hooks installed. Running message loop...\n");

    MSG msg = {};
    while (g_running && GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    delete g_keyboard;
    g_keyboard = nullptr;
    delete g_mouse;
    g_mouse = nullptr;
    delete g_explorer;
    g_explorer = nullptr;
    delete g_inline;
    g_inline = nullptr;

    CoUninitialize();
    return 0;
}
