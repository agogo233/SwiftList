#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <string>
#include <vector>

namespace swiftlist::cli {

class Terminal {
public:
    Terminal();
    ~Terminal();

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

    bool Initialize();
    void Shutdown();

    void Clear();
    void SetCursorPosition(int x, int y);
    void GetCursorPosition(int& x, int& y) const;
    void SetCursorVisible(bool visible);

    void Write(const std::wstring& text);
    void WriteLine(const std::wstring& text);
    void WriteAt(int x, int y, const std::wstring& text);

    wchar_t ReadKey();
    std::wstring ReadLine();

    int Width() const { return width_; }
    int Height() const { return height_; }

    void SetTextAttribute(WORD attr);
    void ResetTextAttribute();

private:
    HANDLE hOut_ = INVALID_HANDLE_VALUE;
    HANDLE hIn_ = INVALID_HANDLE_VALUE;
    DWORD originalMode_ = 0;
    WORD originalAttr_ = 0;
    int width_ = 80;
    int height_ = 25;
};

} // namespace swiftlist::cli
