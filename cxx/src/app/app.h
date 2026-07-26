#pragma once

#include "app/quick_search_window.h"

namespace swiftlist::app {

class Application {
public:
    static Application& Instance();

    bool Initialize();
    void Shutdown();
    int Run();

    void ToggleQuickSearch();
    void ShowSettings();

    QuickSearchWindow& QuickSearch() { return quickSearch_; }

private:
    Application() = default;

    bool EnsureSingleInstance();

    HANDLE singleInstanceMutex_ = nullptr;
    QuickSearchWindow quickSearch_;
    bool initialized_ = false;
};

} // namespace swiftlist::app
