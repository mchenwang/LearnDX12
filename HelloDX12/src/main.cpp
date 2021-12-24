#include "stdafx.h"
#include "DXWindow.h"
#include "Win32App.h"

#include <iostream>
using namespace std;

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
    auto window = new DXWindow(L"Learn DX12");
    Win32Application app(window, hInstance);
    return app.Run();
}