#include "window.hpp"
#include "classes/render.hpp"
#include "hacks/reader.hpp"
#include "hacks/hack.hpp"
#include "utils/log.h"

// Static member initialization
OverlayWindow* OverlayWindow::s_instance = nullptr;

OverlayWindow::OverlayWindow() 
    : m_hWnd(nullptr), m_hInstance(nullptr), m_useHardwareAccel(true),
      m_hdcBuffer(nullptr), m_hbmBuffer(nullptr) {
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

bool OverlayWindow::Initialize(bool useHardwareAccel) {
    m_useHardwareAccel = useHardwareAccel;
    
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

    // Clean up GDI resources
    bool cleanupSuccess = true;

    if (m_hdcBuffer) {
        if (!DeleteDC(m_hdcBuffer)) {
            cleanupSuccess = false;
        }
        m_hdcBuffer = nullptr;
    }

    if (m_hbmBuffer) {
        if (!DeleteObject(m_hbmBuffer)) {
            cleanupSuccess = false;
        }
        m_hbmBuffer = nullptr;
    }

    if (!cleanupSuccess) {
        CLOG_WARN("Failed to destroy overlay resources!");
    }

    if (m_hWnd) {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

void OverlayWindow::SetHardwareAcceleration(bool enable) {
    if (m_useHardwareAccel == enable) return;
    
    m_useHardwareAccel = enable;
    
    if (enable) {
        // Try to initialize hardware renderer
        HRESULT hr = render::g_hwRenderer.Initialize(m_hWnd);
        if (SUCCEEDED(hr)) {
            CLOG_INFO("[render] Hardware acceleration enabled");
            Beep(800, 100);
        }
        else {
            m_useHardwareAccel = false;
            CLOG_WARN("[render] Failed to initialize hardware acceleration, staying with GDI");
            Beep(400, 200);
        }
    }
    else {
        render::g_hwRenderer.Cleanup();
        CLOG_INFO("[render] Hardware acceleration disabled, using GDI");
        Beep(600, 100);
    }
}

void OverlayWindow::ToggleHardwareAcceleration() {
    SetHardwareAcceleration(!m_useHardwareAccel);
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
        // Initialize GDI resources for fallback
        window->m_hdcBuffer = CreateCompatibleDC(NULL);
        window->m_hbmBuffer = CreateCompatibleBitmap(GetDC(hWnd), window->m_gameBounds.right, window->m_gameBounds.bottom);
        SelectObject(window->m_hdcBuffer, window->m_hbmBuffer);

        SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
        SetLayeredWindowAttributes(hWnd, RGB(255, 255, 255), 0, LWA_COLORKEY);

        // Initialize hardware acceleration if enabled
        if (window && window->m_useHardwareAccel) {
            HRESULT hr = render::g_hwRenderer.Initialize(hWnd);
            if (SUCCEEDED(hr)) {
                std::cout << "[render] Hardware acceleration initialized successfully" << std::endl;
            }
            else {
                std::cout << "[render] Hardware acceleration failed to initialize, using GDI fallback" << std::endl;
                window->m_useHardwareAccel = false;
            }
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

        bool useHardwareAccel = window && window->m_useHardwareAccel && render::g_hwRenderer.IsInitialized();

        if (useHardwareAccel) {
            // Use hardware accelerated rendering
            render::g_hwRenderer.BeginDraw();

            if (GetForegroundWindow() == g_game.process->hwnd_) {
                hack::loop(); // Your ESP rendering code
            }

            HRESULT hr = render::g_hwRenderer.EndDraw();
            if (FAILED(hr)) {
                // Hardware rendering failed, fall back to GDI for this frame
                std::cout << "[render] Hardware rendering failed, using GDI fallback" << std::endl;
                useHardwareAccel = false;
            }
        }

        if (!useHardwareAccel) {
            // Use GDI double buffering (original method)
            FillRect(window->m_hdcBuffer, &ps.rcPaint, (HBRUSH)GetStockObject(WHITE_BRUSH));

            if (GetForegroundWindow() == g_game.process->hwnd_) {
                hack::loop(); // Your ESP rendering code
            }

            BitBlt(hdc, 0, 0, window->m_gameBounds.right, window->m_gameBounds.bottom, window->m_hdcBuffer, 0, 0, SRCCOPY);
        }

        EndPaint(hWnd, &ps);
        InvalidateRect(hWnd, NULL, TRUE);
        break;
    }
    case WM_DESTROY:
        // Clean up hardware renderer
        render::g_hwRenderer.Cleanup();

        // Clean up GDI resources
        if (window->m_hdcBuffer) {
            DeleteDC(window->m_hdcBuffer);
            window->m_hdcBuffer = nullptr;
        }
        if (window->m_hbmBuffer) {
            DeleteObject(window->m_hbmBuffer);
            window->m_hbmBuffer = nullptr;
        }
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}