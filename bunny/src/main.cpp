#include "stdafx.h"
#include "DXWindow.h"
// #include "Win32App.h"
#include "Application.h"

#include <iostream>
using namespace std;

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
    // Win32Application app(window, hInstance);
    Application::CreateInstance(hInstance);

    auto app = Application::GetInstance();

    auto window = make_shared<DXWindow>(L"Learn DX12");
    auto hWnd = app->CreateWindow(hInstance, window.get());
    window->Init(hWnd);
    return Application::GetInstance()->Run(window);
}