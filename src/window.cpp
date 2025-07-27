#include "window.hpp"
#include "classes/renderer.hpp"
#include "hacks/reader.hpp"
#include "hacks/hack.hpp"
#include "utils/log.h"

// Static member initialization
OverlayWindow* OverlayWindow::s_instance = nullptr;

OverlayWindow::OverlayWindow() 
    : m_hWnd(nullptr), m_hInstance(nullptr) {
    ZeroMemory(&m_gameBounds, sizeof(RECT));
}

OverlayWindow::~OverlayWindow() {
    Cleanup();
}

OverlayWindow* OverlayWindow::GetInstance() {
    if (s_instance == nullptr) {
        s_instance = new OverlayWindow();
    }
    return s_instance;
}

void OverlayWindow::DestroyInstance() {
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
    }
}

bool OverlayWindow::Initialize() {
    if (!_CreateWindow()) {
        CLOG_WARN("[overlay] Failed to create overlay window");
        return false;
    }

    CLOG_INFO("[overlay] Window initialized successfully");
    return true;
}

bool OverlayWindow::_CreateWindow() {
    std::cout << "[overlay] Creating window overlay..." << std::endl;

    WNDCLASSEXA wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = WndProc;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hbrBackground = WHITE_BRUSH;
    wc.hInstance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrA(g_game.process->hwnd_, GWLP_HINSTANCE));
    wc.lpszMenuName = NULL;
    wc.lpszClassName = " ";

    if (!RegisterClassExA(&wc)) {
        std::cout << "[overlay] Failed to register window class" << std::endl;
        return false;
    }

    GetClientRect(g_game.process->hwnd_, &m_gameBounds);

    m_hInstance = NULL;
    m_hWnd = CreateWindowExA(
        WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        " ",
        "cs2-external-esp",
        WS_POPUP,
        m_gameBounds.left,
        m_gameBounds.top,
        m_gameBounds.right - m_gameBounds.left,
        m_gameBounds.bottom + m_gameBounds.left,
        NULL,
        NULL,
        m_hInstance,
        NULL
    );

    if (m_hWnd == NULL) {
        std::cout << "[overlay] Failed to create overlay window" << std::endl;
        return false;
    }

    SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    ShowWindow(m_hWnd, TRUE);

    return true;
}

void OverlayWindow::Cleanup() {
    // Clean up hardware renderer
    render::g_hwRenderer.Cleanup();

    if (m_hWnd) {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

void OverlayWindow::Show() {
    if (m_hWnd) {
        ShowWindow(m_hWnd, SW_SHOW);
    }
}

void OverlayWindow::Hide() {
    if (m_hWnd) {
        ShowWindow(m_hWnd, SW_HIDE);
    }
}

void OverlayWindow::UpdateBounds() {
    if (g_game.process && g_game.process->hwnd_) {
        GetClientRect(g_game.process->hwnd_, &m_gameBounds);
        
        if (m_hWnd) {
            SetWindowPos(m_hWnd, HWND_TOPMOST, 
                m_gameBounds.left, m_gameBounds.top,
                m_gameBounds.right - m_gameBounds.left,
                m_gameBounds.bottom + m_gameBounds.left,
                SWP_NOZORDER);
        }
    }
}

LRESULT CALLBACK OverlayWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    OverlayWindow* window = OverlayWindow::GetInstance();

    switch (message) {
    case WM_CREATE:
    {
        SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
        SetLayeredWindowAttributes(hWnd, RGB(255, 255, 255), 0, LWA_COLORKEY);

        // Initialize hardware acceleration
        HRESULT hr = render::g_hwRenderer.Initialize(hWnd);
        if (SUCCEEDED(hr)) {
            std::cout << "[render] Hardware acceleration initialized successfully" << std::endl;
        }
        else {
            std::cout << "[render] Hardware acceleration failed to initialize" << std::endl;
            return -1; // Fail window creation if hardware renderer fails
        }

        std::cout << "[overlay] Window created successfully" << std::endl;
        Beep(500, 100);
        break;
    }
    case WM_SIZE:
    {
        // Handle window resize for hardware renderer
        if (render::g_hwRenderer.IsInitialized()) {
            UINT width = LOWORD(lParam);
            UINT height = HIWORD(lParam);
            render::g_hwRenderer.Resize(width, height);
        }
        break;
    }
    case WM_ERASEBKGND: // We handle this message to avoid flickering
        return TRUE;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        if (render::g_hwRenderer.IsInitialized()) {
            // Use hardware accelerated rendering
            render::g_hwRenderer.BeginDraw();

            if (GetForegroundWindow() == g_game.process->hwnd_) {
                hack::loop(); // Your ESP rendering code
            }

            HRESULT hr = render::g_hwRenderer.EndDraw();
            if (FAILED(hr)) {
                std::cout << "[render] Hardware rendering failed" << std::endl;
            }
        }

        EndPaint(hWnd, &ps);
        InvalidateRect(hWnd, NULL, TRUE);
        break;
    }
    case WM_DESTROY:
        // Clean up hardware renderer
        render::g_hwRenderer.Cleanup();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}