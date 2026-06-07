#include "windowmonitor.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// Callback for EnumWindows - collects all visible window titles
struct EnumWindowsContext {
    QStringList *titles;
};

static BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam)
{
    auto *ctx = reinterpret_cast<EnumWindowsContext *>(lParam);

    // Skip invisible windows
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }

    // Skip windows without a title/empty title
    int len = GetWindowTextLengthW(hwnd);
    if (len == 0) {
        return TRUE;
    }

    // Get window title
    wchar_t *buffer = new wchar_t[static_cast<size_t>(len) + 1];
    int actualLen = GetWindowTextW(hwnd, buffer, len + 1);
    if (actualLen > 0) {
        buffer[actualLen] = L'\0';
        ctx->titles->append(QString::fromWCharArray(buffer, actualLen));
    }
    delete[] buffer;

    return TRUE;
}

QStringList WindowMonitor::allWindowTitles()
{
    QStringList titles;
    EnumWindowsContext ctx = { &titles };

    EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&ctx));

    titles.removeDuplicates();
    return titles;
}