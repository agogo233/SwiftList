#include "cli/terminal.h"

namespace swiftlist::cli {

Terminal::Terminal() = default;

Terminal::~Terminal() {
    Shutdown();
}

bool Terminal::Initialize() {
    hOut_ = GetStdHandle(STD_OUTPUT_HANDLE);
    hIn_ = GetStdHandle(STD_INPUT_HANDLE);

    if (hOut_ == INVALID_HANDLE_VALUE || hIn_ == INVALID_HANDLE_VALUE) {
        return false;
    }

    CONSOLE_SCREEN_BUFFER_INFO info = {};
    if (GetConsoleScreenBufferInfo(hOut_, &info)) {
        width_ = info.dwSize.X;
        height_ = info.dwSize.Y;
        originalAttr_ = info.wAttributes;
    }

    GetConsoleMode(hIn_, &originalMode_);
    SetConsoleMode(hIn_, ENABLE_EXTENDED_FLAGS | ENABLE_PROCESSED_INPUT);

    return true;
}

void Terminal::Shutdown() {
    if (hIn_ != INVALID_HANDLE_VALUE) {
        SetConsoleMode(hIn_, originalMode_);
    }
    ResetTextAttribute();
}

void Terminal::Clear() {
    CONSOLE_SCREEN_BUFFER_INFO info = {};
    GetConsoleScreenBufferInfo(hOut_, &info);

    DWORD written = 0;
    DWORD cells = info.dwSize.X * info.dwSize.Y;
    COORD topLeft = {0, 0};
    FillConsoleOutputCharacterW(hOut_, L' ', cells, topLeft, &written);
    SetCursorPosition(0, 0);
}

void Terminal::SetCursorPosition(int x, int y) {
    COORD pos = {static_cast<SHORT>(x), static_cast<SHORT>(y)};
    SetConsoleCursorPosition(hOut_, pos);
}

void Terminal::GetCursorPosition(int& x, int& y) const {
    CONSOLE_SCREEN_BUFFER_INFO info = {};
    GetConsoleScreenBufferInfo(hOut_, &info);
    x = info.dwCursorPosition.X;
    y = info.dwCursorPosition.Y;
}

void Terminal::SetCursorVisible(bool visible) {
    CONSOLE_CURSOR_INFO ci = {};
    GetConsoleCursorInfo(hOut_, &ci);
    ci.bVisible = visible ? TRUE : FALSE;
    SetConsoleCursorInfo(hOut_, &ci);
}

void Terminal::Write(const std::wstring& text) {
    DWORD written = 0;
    WriteConsoleW(hOut_, text.c_str(),
                   static_cast<DWORD>(text.size()), &written, nullptr);
}

void Terminal::WriteLine(const std::wstring& text) {
    Write(text);
    Write(L"\n");
}

void Terminal::WriteAt(int x, int y, const std::wstring& text) {
    SetCursorPosition(x, y);
    Write(text);
}

wchar_t Terminal::ReadKey() {
    INPUT_RECORD rec = {};
    DWORD read = 0;

    while (true) {
        ReadConsoleInputW(hIn_, &rec, 1, &read);
        if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown) {
            return rec.Event.KeyEvent.uChar.UnicodeChar;
        }
    }
}

std::wstring Terminal::ReadLine() {
    std::wstring line;
    while (true) {
        wchar_t ch = ReadKey();
        if (ch == L'\r' || ch == L'\n') {
            WriteLine(L"");
            return line;
        }
        if (ch == L'\b') {
            if (!line.empty()) {
                line.pop_back();
                int cx, cy;
                GetCursorPosition(cx, cy);
                SetCursorPosition(cx - 1, cy);
                Write(L" ");
                SetCursorPosition(cx - 1, cy);
            }
            continue;
        }
        if (ch >= 32) {
            line.push_back(ch);
            Write(std::wstring(1, ch));
        }
    }
}

void Terminal::SetTextAttribute(WORD attr) {
    SetConsoleTextAttribute(hOut_, attr);
}

void Terminal::ResetTextAttribute() {
    SetConsoleTextAttribute(hOut_, originalAttr_);
}

} // namespace swiftlist::cli
