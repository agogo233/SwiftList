#include "pipe/pipe_server.h"

#include <cstring>
#include <stdexcept>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace swiftlist::pipe {

void PipeInstance::Reset() {
    if (pipe != INVALID_HANDLE_VALUE) {
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        pipe = INVALID_HANDLE_VALUE;
    }
    state = PipeInstanceState::WaitingForClient;
    readPos = 0;
    bytesRead = 0;
    readBuf.clear();
    writeBuf.clear();
    std::memset(&ov, 0, sizeof(ov));
}

bool PipeInstance::Connect(const std::wstring& name, HANDLE iocp,
                             ULONG_PTR completionKey) {
    pipe = CreateNamedPipeW(
        name.c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT |
            PIPE_REJECT_REMOTE_CLIENTS,
        PIPE_UNLIMITED_INSTANCES,
        4096, 4096, 0, nullptr);

    if (pipe == INVALID_HANDLE_VALUE) return false;

    CreateIoCompletionPort(pipe, iocp, completionKey, 0);

    std::memset(&ov, 0, sizeof(ov));
    BOOL pending = ConnectNamedPipe(pipe, &ov);
    DWORD err = GetLastError();

    if (pending) {
        state = PipeInstanceState::WaitingForClient;
        return true;
    }

    if (err == ERROR_IO_PENDING) {
        state = PipeInstanceState::WaitingForClient;
        return true;
    }

    if (err == ERROR_PIPE_CONNECTED) {
        state = PipeInstanceState::Reading;
        return StartRead();
    }

    CloseHandle(pipe);
    pipe = INVALID_HANDLE_VALUE;
    return false;
}

bool PipeInstance::StartRead() {
    state = PipeInstanceState::Reading;
    readBuf.resize(4096);
    std::memset(&ov, 0, sizeof(ov));
    DWORD bytesReadLocal = 0;
    BOOL ok = ReadFile(pipe, readBuf.data(), static_cast<DWORD>(readBuf.size()),
                       &bytesReadLocal, &ov);
    if (ok) {
        bytesRead = bytesReadLocal;
        return true;
    }
    DWORD err = GetLastError();
    if (err == ERROR_IO_PENDING) return true;

    if (err == ERROR_MORE_DATA) {
        bytesRead = bytesReadLocal;
        return true;
    }

    state = PipeInstanceState::Disconnected;
    return false;
}

bool PipeInstance::WriteAll(const uint8_t* data, DWORD len) {
    std::memset(&ov, 0, sizeof(ov));
    DWORD written = 0;
    while (written < len) {
        DWORD chunk = 0;
        BOOL ok = WriteFile(pipe, data + written, len - written, &chunk, &ov);
        if (!ok) {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING) {
                DWORD transferred = 0;
                if (!GetOverlappedResult(pipe, &ov, &transferred, TRUE)) {
                    state = PipeInstanceState::Disconnected;
                    return false;
                }
                written += transferred;
                std::memset(&ov, 0, sizeof(ov));
                continue;
            }
            state = PipeInstanceState::Disconnected;
            return false;
        }
        written += chunk;
    }
    return true;
}

PipeServer::PipeServer() = default;

PipeServer::~PipeServer() {
    Stop();
}

bool PipeServer::Start(const std::wstring& pipeName, int instanceCount,
                       HandleRequestFn handler) {
    if (running_) return false;

    pipeName_ = pipeName;
    instanceCount_ = instanceCount;
    handler_ = std::move(handler);

    iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0,
                                   instanceCount_);
    if (!iocp_) return false;

    stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!stopEvent_) {
        CloseHandle(iocp_);
        iocp_ = INVALID_HANDLE_VALUE;
        return false;
    }

    instances_.resize(instanceCount_);
    for (int i = 0; i < instanceCount_; ++i) {
        if (!instances_[i].Connect(pipeName_, iocp_,
                                   reinterpret_cast<ULONG_PTR>(&instances_[i]))) {
            DWORD err = GetLastError();
            if (err != ERROR_IO_PENDING && err != ERROR_PIPE_CONNECTED) {
                Stop();
                return false;
            }
        }
    }

    running_ = true;
    for (int i = 0; i < instanceCount_; ++i) {
        HANDLE h = CreateThread(nullptr, 0, WorkerThread, this, 0, nullptr);
        if (!h) {
            Stop();
            return false;
        }
        workers_.push_back(h);
    }

    return true;
}

void PipeServer::Stop() {
    if (!running_) return;
    running_ = false;

    if (stopEvent_) SetEvent(stopEvent_);

    for (auto& inst : instances_) {
        if (inst.pipe != INVALID_HANDLE_VALUE) {
            CancelIo(inst.pipe);
        }
    }

    for (auto h : workers_) {
        WaitForSingleObject(h, 5000);
        CloseHandle(h);
    }
    workers_.clear();

    for (auto& inst : instances_) {
        inst.Reset();
    }
    instances_.clear();

    if (iocp_ != INVALID_HANDLE_VALUE) {
        CloseHandle(iocp_);
        iocp_ = INVALID_HANDLE_VALUE;
    }
    if (stopEvent_ != INVALID_HANDLE_VALUE) {
        CloseHandle(stopEvent_);
        stopEvent_ = INVALID_HANDLE_VALUE;
    }
}

DWORD WINAPI PipeServer::WorkerThread(LPVOID param) {
    auto* self = static_cast<PipeServer*>(param);
    self->WorkerLoop();
    return 0;
}

void PipeServer::WorkerLoop() {
    DWORD bytesTransferred = 0;
    ULONG_PTR key = 0;
    LPOVERLAPPED ov = nullptr;

    while (running_) {
        BOOL ok = GetQueuedCompletionStatus(iocp_, &bytesTransferred, &key, &ov, 1000);
        if (!ok) {
            if (!ov) continue;

            auto* inst = FindInstanceByOverlapped(ov);
            if (inst) {
                std::wstring name = pipeName_;
                inst->Reset();
                inst->Connect(name, iocp_, reinterpret_cast<ULONG_PTR>(inst));
            }
            continue;
        }

        if (bytesTransferred == 0 && key == 0 && ov == nullptr) {
            break;
        }

        auto* inst = reinterpret_cast<PipeInstance*>(key);
        if (!inst) continue;

        switch (inst->state) {
        case PipeInstanceState::WaitingForClient:
            inst->state = PipeInstanceState::Reading;
            inst->StartRead();
            break;

        case PipeInstanceState::Reading: {
            inst->bytesRead = bytesTransferred;
            std::vector<uint8_t> response;
            bool keepAlive = handler_(*inst, response);
            std::wstring name = pipeName_;
            if (!keepAlive || response.empty()) {
                inst->Reset();
                inst->Connect(name, iocp_, reinterpret_cast<ULONG_PTR>(inst));
                break;
            }
            inst->writeBuf = std::move(response);
            inst->state = PipeInstanceState::Writing;
            if (!inst->WriteAll(inst->writeBuf.data(),
                                static_cast<DWORD>(inst->writeBuf.size()))) {
                inst->Reset();
                inst->Connect(name, iocp_, reinterpret_cast<ULONG_PTR>(inst));
                break;
            }
            FlushFileBuffers(inst->pipe);
            DisconnectNamedPipe(inst->pipe);
            inst->Reset();
            inst->Connect(name, iocp_, reinterpret_cast<ULONG_PTR>(inst));
            break;
        }

        case PipeInstanceState::Writing:
        case PipeInstanceState::Disconnected:
        default: {
            std::wstring name = pipeName_;
            inst->Reset();
            inst->Connect(name, iocp_, reinterpret_cast<ULONG_PTR>(inst));
            break;
        }
        }
    }
}

PipeInstance* PipeServer::FindInstanceByOverlapped(LPOVERLAPPED ov) {
    for (auto& inst : instances_) {
        if (&inst.ov == ov) return &inst;
    }
    return nullptr;
}

} // namespace swiftlist::pipe
