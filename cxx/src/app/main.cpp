#include "app/app.h"

#include <cstdio>

int wmain(int argc, wchar_t* argv[]) {
    auto& app = swiftlist::app::Application::Instance();

    if (!app.Initialize()) {
        return 0;
    }

    int result = app.Run();
    app.Shutdown();
    return result;
}
