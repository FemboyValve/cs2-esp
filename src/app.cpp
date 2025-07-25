#include "app.hpp"
#include "classes/utils.h"
#include "memory/memory.hpp"
#include "classes/vector.hpp"
#include "hacks/reader.hpp"
#include "hacks/hack.hpp"
#include "classes/globals.hpp"
#include "classes/render.hpp"
#include "classes/auto_updater.hpp"
#include "utils/log.h"

// Static member initialization
App* App::s_instance = nullptr;

// Global hardware renderer instance
render::HardwareRenderer render::g_hwRenderer;

App::App() : m_hWnd(nullptr), m_hInstance(nullptr), m_finish(false),
m_hdcBuffer(nullptr), m_hbmBuffer(nullptr), m_useHardwareAccel(true) {
    s_instance = this;
    ZeroMemory(&m_gameBounds, sizeof(RECT));
}

App::~App() {
    s_instance = nullptr;
}

int App::Initialize(int argc, char* argv[]) {
    LOG_INIT();
    utils.update_console_title();

    // Check command line arguments for hardware acceleration disable
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-hw-accel") == 0) {
            m_useHardwareAccel = false;
            CLOG_INFO("[render] Hardware acceleration disabled via command line");
            break;
        }
    }

    if (!InitializeConfig()) {
        return -1;
    }

#ifndef _UC
    CheckForUpdates();
#endif

    if (!InitializeOffsets()) {
        return -1;
    }

    g_game.init();

    // Check build number compatibility
    if (g_game.buildNumber != updater::build_number) {
        CLOG_WARN("[cs2] Build number doesnt match, the game has been updated and this esp most likely wont work.");
        CLOG_WARN("[warn] If the esp doesnt work, consider updating offsets manually in the file offsets.json");
        LOG_WARNING(Logger::GetLogger(), "Current build number: {}", static_cast<int>(g_game.buildNumber));
        LOG_WARNING(Logger::GetLogger(), "Build number in offsets.json: {}", static_cast<int>(updater::build_number));
        CLOG_WARN("[cs2] Press any key to continue");
        std::cin.get();
    }
    else {
        CLOG_INFO("[cs2] Offsets seem to be up to date! have fun!");
    }

    return 0;
}

int App::Run() {
    // Wait for game window focus
    std::cout << "[overlay] Waiting to focus game to create the overlay..." << std::endl;
    std::cout << "[overlay] Make sure your game is in \"Full Screen Windowed\"" << std::endl;

    while (GetForegroundWindow() != g_game.process->hwnd_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        g_game.process->UpdateHWND();
        ShowWindow(g_game.process->hwnd_, TRUE);
    }

    if (!CreateOverlayWindow()) {
        return -1;
    }

    StartReadThread();

    // Show keybind information
#ifndef _UC
    std::cout << "\n[settings] In Game keybinds:\n\t[F4] enable/disable Box ESP\n\t[F5] enable/disable Team ESP\n\t[F6] enable/disable automatic updates\n\t[F7] enable/disable extra flags\n\t[F8] enable/disable skeleton esp\n\t[F9] enable/disable head tracker\n\t[F10] toggle hardware acceleration\n\t[end] Unload esp.\n" << std::endl;
#else
    std::cout << "\n[settings] In Game keybinds:\n\t[F4] enable/disable Box ESP\n\t[F5] enable/disable Team ESP\n\t[F7] enable/disable extra flags\n\t[F8] enable/disable skeleton esp\n\t[F9] enable/disable head tracker\n\t[F10] toggle hardware acceleration\n\t[end] Unload esp.\n" << std::endl;
#endif
    std::cout << "[settings] Make sure you check the config for additional settings!" << std::endl;

    MessageLoop();

    return 1;
}

void App::Shutdown() {
    m_finish = true;
    if (m_readThread.joinable()) {
        m_readThread.detach();
    }
    Beep(700, 100);
    Beep(700, 100);
    std::cout << "[overlay] Destroying overlay window." << std::endl;

    // Clean up hardware renderer
    render::g_hwRenderer.Cleanup();

    // Clean up GDI resources that aren't handled by WM_DESTROY
    bool cleanupSuccess = true;

    if (g::hdcBuffer) {
        if (!DeleteDC(g::hdcBuffer)) {
            cleanupSuccess = false;
        }
        g::hdcBuffer = nullptr;
    }

    if (g::hbmBuffer) {
        if (!DeleteObject(g::hbmBuffer)) {
            cleanupSuccess = false;
        }
        g::hbmBuffer = nullptr;
    }

    if (!cleanupSuccess) {
        CLOG_WARN("Failed to destroy overlay resources!");
    }

    if (m_hWnd) {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
    g_game.close();
    CLOG_INFO("Exiting App");
    LOG_DESTROY();
}

bool App::InitializeConfig() {
    CLOG_INFO("[Config] Reading configuration.");
    if (config::read()) {
        CLOG_INFO("[Config] Successfully read configuration file!");
        return true;
    }
    else {
        CLOG_INFO("[Config] Error reading config file, resetting to the default state.");
        return true; // Continue with defaults
    }
}

#ifndef _UC
void App::CheckForUpdates() {
    try {
        updater::check_and_update(config::automatic_update);
    }
    catch (std::exception& e) {
        std::cout << "An exception was caught while updating: " << e.what() << std::endl;
    }
}
#endif

bool App::InitializeOffsets() {
    CLOG_INFO("[Offsets] Reading offsets from file offsets.json.");
    if (updater::read()) {
        CLOG_INFO("[Offsets] Successfully read offsets file");
        return true;
    }
    else {
        CLOG_INFO("[Offsets] Error reading offsets file, resetting to the default state");
        return true; // Continue with defaults
    }
}

bool App::CreateOverlayWindow() {
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

    // Store in global for compatibility with original code
    g::gameBounds = m_gameBounds;

    m_hInstance = NULL; // Like original
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

void App::StartReadThread() {
    m_readThread = std::thread(&App::ReadThreadFunction, this);
}

void App::ReadThreadFunction() {
    while (!m_finish) {
        g_game.loop();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

void App::HandleKeyInput() {
    if (GetAsyncKeyState(VK_END) & 0x8000) {
        m_finish = true;
    }

    if (GetAsyncKeyState(VK_F4) & 0x8000) {
        config::show_box_esp = !config::show_box_esp;
        config::save();
        Beep(700, 100);
    }

    if (GetAsyncKeyState(VK_F5) & 0x8000) {
        config::team_esp = !config::team_esp;
        config::save();
        Beep(700, 100);
    }

#ifndef _UC
    if (GetAsyncKeyState(VK_F6) & 0x8000) {
        config::automatic_update = !config::automatic_update;
        config::save();
        Beep(700, 100);
    }
#endif

    if (GetAsyncKeyState(VK_F7) & 0x8000) {
        config::show_extra_flags = !config::show_extra_flags;
        config::save();
        Beep(700, 100);
    }

    if (GetAsyncKeyState(VK_F8) & 0x8000) {
        config::show_skeleton_esp = !config::show_skeleton_esp;
        config::save();
        Beep(700, 100);
    }

    if (GetAsyncKeyState(VK_F9) & 0x8000) {
        config::show_head_tracker = !config::show_head_tracker;
        config::save();
        Beep(700, 100);
    }

    // Toggle hardware acceleration
    if (GetAsyncKeyState(VK_F10) & 0x8000) {
        m_useHardwareAccel = !m_useHardwareAccel;
        if (m_useHardwareAccel) {
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
        config::save();
    }
}

void App::MessageLoop() {
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) && !m_finish) {
        HandleKeyInput();

        TranslateMessage(&msg);
        DispatchMessage(&msg);

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

LRESULT CALLBACK App::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    App* app = App::s_instance;

    switch (message) {
    case WM_CREATE:
    {
        // Initialize GDI resources for fallback
        g::hdcBuffer = CreateCompatibleDC(NULL);
        g::hbmBuffer = CreateCompatibleBitmap(GetDC(hWnd), g::gameBounds.right, g::gameBounds.bottom);
        SelectObject(g::hdcBuffer, g::hbmBuffer);

        SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
        SetLayeredWindowAttributes(hWnd, RGB(255, 255, 255), 0, LWA_COLORKEY);

        // Initialize hardware acceleration if enabled
        if (app && app->m_useHardwareAccel) {
            HRESULT hr = render::g_hwRenderer.Initialize(hWnd);
            if (SUCCEEDED(hr)) {
                std::cout << "[render] Hardware acceleration initialized successfully" << std::endl;
            }
            else {
                std::cout << "[render] Hardware acceleration failed to initialize, using GDI fallback" << std::endl;
                app->m_useHardwareAccel = false;
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

        bool useHardwareAccel = app && app->m_useHardwareAccel && render::g_hwRenderer.IsInitialized();

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
            FillRect(g::hdcBuffer, &ps.rcPaint, (HBRUSH)GetStockObject(WHITE_BRUSH));

            if (GetForegroundWindow() == g_game.process->hwnd_) {
                hack::loop(); // Your ESP rendering code
            }

            BitBlt(hdc, 0, 0, g::gameBounds.right, g::gameBounds.bottom, g::hdcBuffer, 0, 0, SRCCOPY);
        }

        EndPaint(hWnd, &ps);
        InvalidateRect(hWnd, NULL, TRUE);
        break;
    }
    case WM_DESTROY:
        // Clean up hardware renderer
        render::g_hwRenderer.Cleanup();

        // Clean up GDI resources
        if (g::hdcBuffer) {
            DeleteDC(g::hdcBuffer);
            g::hdcBuffer = nullptr;
        }
        if (g::hbmBuffer) {
            DeleteObject(g::hbmBuffer);
            g::hbmBuffer = nullptr;
        }
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}