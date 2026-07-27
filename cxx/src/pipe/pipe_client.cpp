#include "pipe/pipe_client.h"

#include <cstring>

namespace swiftlist::pipe {

PipeClient::PipeClient() = default;

PipeClient::~PipeClient() {
    Disconnect();
}

bool PipeClient::Connect(const std::wstring& pipeName, DWORD timeoutMs) {
    if (!WaitNamedPipeW(pipeName.c_str(), timeoutMs)) {
        return false;
    }

    pipe_ = CreateFileW(
        pipeName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);

    if (pipe_ == INVALID_HANDLE_VALUE) return false;

    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(pipe_, &mode, nullptr, nullptr)) {
        Disconnect();
        return false;
    }

    return true;
}

void PipeClient::Disconnect() {
    if (pipe_ != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe_);
        pipe_ = INVALID_HANDLE_VALUE;
    }
}

bool PipeClient::WriteAll(const void* data, DWORD len) {
    const auto* p = static_cast<const uint8_t*>(data);
    DWORD written = 0;
    while (written < len) {
        DWORD chunk = 0;
        if (!WriteFile(pipe_, p + written, len - written, &chunk, nullptr)) {
            return false;
        }
        written += chunk;
    }
    return true;
}

bool PipeClient::ReadAll(void* buffer, DWORD len, DWORD& bytesRead) {
    auto* p = static_cast<uint8_t*>(buffer);
    bytesRead = 0;
    while (bytesRead < len) {
        DWORD chunk = 0;
        if (!ReadFile(pipe_, p + bytesRead, len - bytesRead, &chunk, nullptr)) {
            return false;
        }
        if (chunk == 0) return false;
        bytesRead += chunk;
    }
    return true;
}

bool PipeClient::ReadFrame(std::vector<uint8_t>& payload, DWORD* outMagic,
                            DWORD* outVersion) {
    DWORD magic = 0, version = 0, length = 0;
    DWORD br = 0;
    if (!ReadAll(&magic, 4, br)) return false;
    if (!ReadAll(&version, 4, br)) return false;
    if (!ReadAll(&length, 4, br)) return false;

    if (length > kMaxPayloadSize) return false;

    payload.resize(length);
    if (length > 0) {
        if (!ReadAll(payload.data(), length, br)) return false;
    }

    if (outMagic) *outMagic = magic;
    if (outVersion) *outVersion = version;
    return true;
}

bool PipeClient::ReadMessage(std::vector<uint8_t>& payload) {
    // For PIPE_READMODE_MESSAGE: read the complete message, growing buffer as needed.
    payload.resize(65536);
    DWORD totalRead = 0;
    while (true) {
        DWORD chunk = 0;
        BOOL ok = ReadFile(pipe_, payload.data() + totalRead,
                            static_cast<DWORD>(payload.size()) - totalRead,
                            &chunk, nullptr);
        totalRead += chunk;
        if (ok) {
            payload.resize(totalRead);
            return true;
        }
        DWORD err = GetLastError();
        if (err == ERROR_MORE_DATA) {
            if (chunk == 0) return false; // defensive: no progress, avoid infinite loop
            payload.resize(payload.size() * 2);
            continue;
        }
        return false;
    }
}

} // namespace swiftlist::pipe
