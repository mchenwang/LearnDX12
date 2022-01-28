#ifndef __WIN32APP_H__
#define __WIN32APP_H__

#include "DXWindow.h"

class Win32Application
{ 
    std::shared_ptr<DXWindow> window;
    void RegisterWindowClass(HINSTANCE hInst, const wchar_t* windowClassName);
    HWND CreateWindow(const wchar_t* windowClassName, HINSTANCE hInst,
        const wchar_t* windowTitle, uint32_t width, uint32_t height);
public:
    Win32Application() = delete;
    ~Win32Application() {}
    Win32Application(std::shared_ptr<DXWindow> w, HINSTANCE hInst) noexcept;
    void Init(HINSTANCE);
    int Run();
    HWND GetHwnd();

    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};

#endif