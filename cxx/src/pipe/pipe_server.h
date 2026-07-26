#pragma once

#include <Windows.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace swiftlist::pipe {

enum class PipeInstanceState : uint8_t {
    WaitingForClient,
    Reading,
    Writing,
    Disconnected,
};

struct PipeInstance;

using HandleRequestFn = std::function<bool(PipeInstance& inst,
                                            std::vector<uint8_t>& response)>;

struct PipeInstance {
    OVERLAPPED ov{};
    HANDLE pipe = INVALID_HANDLE_VALUE;
    PipeInstanceState state = PipeInstanceState::WaitingForClient;
    std::vector<uint8_t> readBuf;
    std::vector<uint8_t> writeBuf;
    DWORD readPos = 0;
    DWORD bytesRead = 0;

    void Reset();
    bool Connect(const std::wstring& name, HANDLE iocp, ULONG_PTR completionKey);
    bool StartRead();
    bool WriteAll(const uint8_t* data, DWORD len);
};

class PipeServer {
public:
    PipeServer();
    ~PipeServer();

    PipeServer(const PipeServer&) = delete;
    PipeServer& operator=(const PipeServer&) = delete;

    bool Start(const std::wstring& pipeName, int instanceCount,
               HandleRequestFn handler);
    void Stop();
    bool IsRunning() const { return running_; }

private:
    static DWORD WINAPI WorkerThread(LPVOID param);
    void WorkerLoop();
    PipeInstance* FindInstanceByOverlapped(LPOVERLAPPED ov);

    std::wstring pipeName_;
    int instanceCount_ = 4;
    HandleRequestFn handler_;
    HANDLE iocp_ = INVALID_HANDLE_VALUE;
    HANDLE stopEvent_ = INVALID_HANDLE_VALUE;
    std::atomic<bool> running_{false};
    std::vector<PipeInstance> instances_;
    std::vector<HANDLE> workers_;
};

} // namespace swiftlist::pipe
