// main.cpp - External Overlay with ImGui (DX11) TARGETING GAME WINDOW
#include <windows.h>
#include <d3d11.h>
#include <tchar.h>

#pragma comment(lib, "d3d11.lib")

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

// Globals
static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
static IDXGISwapChain* g_pSwapChain = NULL;
static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

bool show_demo_window = false;

bool overlay_visible = true;
bool insert_key_previous_state = false;

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Target game's window name (Change this to your game's window title)
const TCHAR* target_window_name = _T("Assassin’s Creed® Odyssey");

// Function: Get game's HWND
HWND GetTargetWindowHandle() {
    HWND hwnd = FindWindow(NULL, target_window_name);
    return hwnd;
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = NULL;
    }
}

HRESULT CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    RECT rect;
    GetClientRect(hWnd, &rect);
    sd.BufferDesc.Width = rect.right - rect.left;
    sd.BufferDesc.Height = rect.bottom - rect.top;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.Windowed = TRUE;
    UINT createDeviceFlags = 0;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL levels[1] = { D3D_FEATURE_LEVEL_11_0 };

    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, levels, 1,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return E_FAIL;

    CreateRenderTarget();
    return S_OK;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    HWND game_hwnd = nullptr;
    while (!(game_hwnd = GetTargetWindowHandle())) {
        MessageBoxA(0, "Cannot find target window! Make sure the target game is running.", "Error", 0);
        Sleep(2000);
    }

    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0, 0,
                      GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
                      _T("Overlay"), NULL };
    RegisterClassEx(&wc);

    RECT tSize;
    GetWindowRect(game_hwnd, &tSize);

    HWND hwnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED, wc.lpszClassName,
        _T("External Overlay"), WS_POPUP,
        tSize.left, tSize.top,
        tSize.right - tSize.left, tSize.bottom - tSize.top,
        NULL, NULL, wc.hInstance, NULL);

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    ShowWindow(hwnd, SW_SHOWDEFAULT);

    if (CreateDeviceD3D(hwnd) != S_OK)
    {
        CleanupDeviceD3D();
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Main loop
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (!FindWindow(NULL, target_window_name))
            break;

        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                break;
        }

        // Toggle visibility & clickable status via Insert key
        bool insert_key_current_state = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        if (insert_key_current_state && !insert_key_previous_state)
        {
            overlay_visible = !overlay_visible;

            LONG ex_style = GetWindowLong(hwnd, GWL_EXSTYLE);

            if (overlay_visible)
            {
                // Make window clickable
                ex_style &= ~WS_EX_TRANSPARENT;
            }
            else
            {
                // Make window click-through
                ex_style |= WS_EX_TRANSPARENT;
            }

            SetWindowLong(hwnd, GWL_EXSTYLE, ex_style);
        }
        insert_key_previous_state = insert_key_current_state;

        // Adjust the overlay position to always fit the target window
        RECT tSize;
        GetWindowRect(game_hwnd, &tSize);
        SetWindowPos(hwnd, HWND_TOPMOST, tSize.left, tSize.top,
            tSize.right - tSize.left, tSize.bottom - tSize.top, SWP_SHOWWINDOW);

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (overlay_visible)
        {
            // Overlay drawing
            ImGui::Begin("Targeted Overlay");
            ImGui::Text("Overlaying: \"%s\"", target_window_name);
            ImGui::Checkbox("Show ImGui Demo", &show_demo_window);
            if (show_demo_window)
                ImGui::ShowDemoWindow(&show_demo_window);
            ImGui::End();
        }

        ImGui::Render();
        const float clear_color[4] = { 0,0,0,0 };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (overlay_visible && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}