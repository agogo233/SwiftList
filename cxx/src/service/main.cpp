#include "service/usn_service.h"
#include "service/service_installer.h"
#include "service/hook_mode_launcher.h"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

using namespace swiftlist::service;

static std::atomic<bool> g_running{true};
static WinService* g_service = nullptr;

static void SignalHandler(int /*sig*/) {
    g_running = false;
    if (g_service) g_service->Stop();
}

static void PrintUsage(const wchar_t* exe) {
    fwprintf(stderr,
             L"Usage: %s [--service | --install | --uninstall | --hook]\n"
             L"  --service    Run as Windows Service (invoked by SCM)\n"
             L"  --install    Install and start the service\n"
             L"  --uninstall  Stop and remove the service\n"
             L"  --hook       Run in hook mode (global hotkeys)\n"
             L"  (no args)    Debug console mode\n",
             exe);
}

static int RunAsDebugConsole() {
    fwprintf(stdout, L"SwiftList Service running in debug console mode. Press Ctrl+C to exit.\n");

    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    fwprintf(stdout, L"Service stopped.\n");
    return 0;
}

static int RunAsService(WinService& service) {
    g_service = &service;

    auto handler = [](DWORD control) {
        if (control == SERVICE_CONTROL_STOP || control == SERVICE_CONTROL_SHUTDOWN) {
            g_running = false;
        }
    };

    if (!service.Run(L"SwiftListService", handler)) {
        fwprintf(stderr, L"Failed to start service dispatcher. Error: %lu\n", GetLastError());
        return 1;
    }

    return 0;
}

static int DoInstall(const std::wstring& exePath) {
    ServiceInstallConfig config;
    config.ServiceName = L"SwiftListService";
    config.DisplayName = L"SwiftList Background Service";
    config.BinPath = L"\"" + exePath + L"\" --service";
    config.Description = L"Provides fast file search indexing and IPC for SwiftList App.";
    config.StartType = SERVICE_AUTO_START;
    config.SecurityDescriptor =
        L"D:(A;;CCLCSWRPWPDTLOCRRC;;;SY)"
        L"(A;;CCDCLCSWRPWPDTLOCRSDRCWDWO;;;BA)"
        L"(A;;CCLCSWLOCRRC;;;IU)"
        L"(A;;CCLCSWLOCRRC;;;SU)"
        L"(A;;CCLCSWRPWPLORC;;;AU)"
        L"S:(AU;FA;CCDCLCSWRPWPDTLOCRSDRCWDWO;;;WD)";

    fwprintf(stdout, L"Installing service: %s\n", config.ServiceName.c_str());

    if (!ServiceInstall(config)) {
        fwprintf(stderr, L"Service install failed. Error: %lu\n", GetLastError());
        return 1;
    }

    fwprintf(stdout, L"Service installed successfully. Starting...\n");

    if (!ServiceStart(config.ServiceName)) {
        fwprintf(stderr, L"Service start failed. Error: %lu\n", GetLastError());
        return 1;
    }

    fwprintf(stdout, L"Service installed and started.\n");
    return 0;
}

static int DoUninstall() {
    fwprintf(stdout, L"Stopping service...\n");
    ServiceStop(L"SwiftListService");

    fwprintf(stdout, L"Removing service...\n");
    if (!ServiceUninstall(L"SwiftListService")) {
        fwprintf(stderr, L"Service uninstall failed. Error: %lu\n", GetLastError());
        return 1;
    }

    fwprintf(stdout, L"Service uninstalled.\n");
    return 0;
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc > 1) {
        std::wstring arg = argv[1];

        if (arg == L"--service") {
            WinService service;
            return RunAsService(service);
        }

        if (arg == L"--install" || arg == L"-i") {
            if (argc < 3) {
                PrintUsage(argv[0]);
                return 1;
            }
            return DoInstall(argv[2]);
        }

        if (arg == L"--uninstall" || arg == L"-u") {
            return DoUninstall();
        }

        if (arg == L"--hook") {
            fwprintf(stdout, L"Use 'hook.exe' directly for hook mode.\n");
            return 0;
        }

        PrintUsage(argv[0]);
        return 1;
    }

    return RunAsDebugConsole();
}
