#include "app/app.h"

#include <cstdio>

int wmain(int argc, wchar_t* argv[]) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) return 1;

    auto& app = swiftlist::app::Application::Instance();

    int result = 0;
    if (!app.Initialize()) {
        result = 0;
    } else {
        result = app.Run();
        app.Shutdown();
    }

    CoUninitialize();
    return result;
}
