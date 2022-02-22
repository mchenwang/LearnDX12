#include "stdafx.h"
#include "DXWindow.h"
#include "Application.h"

#include <iostream>
using namespace std;

class com_auto_init
{
public:
    com_auto_init()
    {
        CoInitialize(nullptr);
    }
    ~com_auto_init()
    {
        CoUninitialize();
    }
};
__declspec(selectany) com_auto_init _com_init = com_auto_init();

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
    // Win32Application app(window, hInstance);
    Application::CreateInstance(hInstance);

    auto app = Application::GetInstance();

    // auto model = make_shared<Model>(L"bun_zipper.ply", ModelType::PLY, true);
    auto model = make_shared<Model>(L"african_head.obj", ModelType::OBJ);
    // auto model = make_shared<Model>(L"box.obj", ModelType::OBJ);
    //auto model = make_shared<Model>(L"box.ply", ModelType::PLY);
    app->SetModel(model);

    auto window = make_shared<DXWindow>(L"Learn DX12");
    auto hWnd = app->CreateWindow(hInstance, window.get());
    window->Init(hWnd);
    return app->Run(window);
}