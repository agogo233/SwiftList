#pragma once

#include <cstdint>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace swiftlist::pipe {

class PipeClient {
public:
    PipeClient();
    ~PipeClient();

    PipeClient(const PipeClient&) = delete;
    PipeClient& operator=(const PipeClient&) = delete;

    bool Connect(const std::wstring& pipeName, DWORD timeoutMs = 5000);
    void Disconnect();
    bool IsConnected() const { return pipe_ != INVALID_HANDLE_VALUE; }

    bool WriteAll(const void* data, DWORD len);
    bool ReadAll(void* buffer, DWORD len, DWORD& bytesRead);
    bool ReadFrame(std::vector<uint8_t>& payload, DWORD* outMagic = nullptr,
                   DWORD* outVersion = nullptr);
    bool ReadMessage(std::vector<uint8_t>& payload);

private:
    HANDLE pipe_ = INVALID_HANDLE_VALUE;
};

} // namespace swiftlist::pipe
