//
// Main.cpp
//

#include "pch.h"
#include "Game.h"
//#include "dwmapi.h"
#include <thread>
#include "wingdi.h"

using namespace DirectX;
//
//typedef struct _MARGINS
//{
//	int cxLeftWidth;      // width of left border that retains its size
//	int cxRightWidth;     // width of right border that retains its size
//	int cyTopHeight;      // height of top border that retains its size
//	int cyBottomHeight;   // height of bottom border that retains its size
//} MARGINS, *PMARGINS;

namespace
{
    std::unique_ptr<Game> g_game;
};

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// Indicates to hybrid graphics systems to prefer the discrete part by default
extern "C"
{
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

// Entry point
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

	SetErrorMode(SetErrorMode(SEM_NOGPFAULTERRORBOX)
		| SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

    if (!XMVerifyCPUSupport())
        return 1;

    HRESULT hr = CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);
    if (FAILED(hr))
        return 1;

    g_game = std::make_unique<Game>();

    // Register class and create window
    {
        // Register class
        WNDCLASSEX wcex;
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = hInstance;
        wcex.hIcon = LoadIcon(hInstance, L"IDI_ICON");
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
        wcex.lpszMenuName = nullptr;
        wcex.lpszClassName = L"EeayAC";
        wcex.hIconSm = LoadIcon(wcex.hInstance, L"IDI_ICON");
        if (!RegisterClassEx(&wcex))
            return 1;

        // Create window
        int w, h;
        g_game->GetDefaultSize(w, h);

        RECT rc;
        rc.top = 0;
        rc.left = 0;
        rc.right = static_cast<LONG>(w); 
        rc.bottom = static_cast<LONG>(h);
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        HWND hwnd = CreateWindowEx(WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOPMOST, L"EeayAC", L"EeayAC",
            WS_POPUP,
            CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
            nullptr);
        if (!hwnd)
            return 1;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(g_game.get()));
        g_game->updateWindow();
        GetClientRect(hwnd, &rc);
        g_game->Initialize(hwnd, rc.right - rc.left, rc.bottom - rc.top);
        SetWindowLong(hwnd,
            GWL_EXSTYLE,
            GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW);
    }

	std::thread t(std::bind(&Game::Manager, g_game.get()));
	t.detach();
    // Main message loop
    MSG msg = {};
	while (WM_QUIT != msg.message)
	{
		if (GetMessage(&msg, NULL, 0, 0) > 0)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	g_game->exit();
    g_game.reset();

    CoUninitialize();

    return (int) msg.wParam;
}

// Windows procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static bool s_in_suspend = false;
    static bool s_minimized = false;
    static bool s_fullscreen = false;
    // TODO: Set s_fullscreen to true if defaulting to fullscreen.

    auto game = reinterpret_cast<Game*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
        {
            if (!s_minimized)
            {
                s_minimized = true;
                if (!s_in_suspend && game)
                    game->OnSuspending();
                s_in_suspend = true;
            }
        }
        else if (s_minimized)
        {
            s_minimized = false;
            if (s_in_suspend && game)
                game->OnResuming();
            s_in_suspend = false;
        }
        else if (game)
        {
            game->OnWindowSizeChanged(LOWORD(lParam), HIWORD(lParam));
        }
        break;

    case WM_GETMINMAXINFO:
        {
            auto info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = 320;
            info->ptMinTrackSize.y = 200;
        }
        break;

    case WM_ACTIVATEAPP:
        if (game)
        {
            if (wParam)
            {
                game->OnActivated();
            }
            else
            {
                game->OnDeactivated();
            }
        }
        break;

    case WM_POWERBROADCAST:
        switch (wParam)
        {
        case PBT_APMQUERYSUSPEND:
            if (!s_in_suspend && game)
                game->OnSuspending();
            s_in_suspend = true;
            return TRUE;

        case PBT_APMRESUMESUSPEND:
            if (!s_minimized)
            {
                if (s_in_suspend && game)
                    game->OnResuming();
                s_in_suspend = false;
            }
            return TRUE;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_MENUCHAR:
        // A menu is active and the user presses a key that does not correspond
        // to any mnemonic or accelerator key. Ignore so we don't produce an error beep.
        return MAKELRESULT(0, MNC_CLOSE);
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}
