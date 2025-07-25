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

App::App() : m_hWnd(nullptr), m_hInstance(nullptr), m_finish(false), m_glfwWindow(nullptr),
m_showConfigWindow(true), m_imguiInitialized(false) {
    s_instance = this;
    ZeroMemory(&g::gameBounds, sizeof(RECT));
    g::hdcBuffer = nullptr;
	g::hbmBuffer = nullptr;
}

App::~App() {
    s_instance = nullptr;
}

int App::Initialize(int argc, char* argv[]) {
    LOG_INIT();
    utils.update_console_title();

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

    // Initialize ImGui
    if (!InitializeImGui()) {
        CLOG_WARN("[ImGui] Failed to initialize ImGui!");
        return -1;
    }

    return 0;
}

bool App::InitializeImGui() {
    // Initialize GLFW
    if (!glfwInit()) {
        CLOG_WARN("[GLFW] Failed to initialize GLFW");
        return false;
    }

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // Create window with graphics context
    m_glfwWindow = glfwCreateWindow(800, 600, "CS2 ESP Config", nullptr, nullptr);
    if (m_glfwWindow == nullptr) {
        CLOG_WARN("[GLFW] Failed to create GLFW window");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_glfwWindow);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(m_glfwWindow, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    m_imguiInitialized = true;
    CLOG_INFO("[ImGui] Successfully initialized ImGui with GLFW and OpenGL3");
    return true;
}

void App::RenderImGui() {
    if (!m_imguiInitialized || !m_glfwWindow) {
        return;
    }

    // Poll and handle events (inputs, window resize, etc.)
    glfwPollEvents();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Main configuration window
    if (m_showConfigWindow) {
        ImGui::Begin("CS2 ESP Configuration", &m_showConfigWindow);

        ImGui::Text("ESP Settings");
        ImGui::Separator();

        // Box ESP toggle
        bool boxEsp = config::show_box_esp;
        if (ImGui::Checkbox("Box ESP (F4)", &boxEsp)) {
            config::show_box_esp = boxEsp;
            config::save();
        }

        // Team ESP toggle
        bool teamEsp = config::team_esp;
        if (ImGui::Checkbox("Team ESP (F5)", &teamEsp)) {
            config::team_esp = teamEsp;
            config::save();
        }

#ifndef _UC
        // Automatic updates toggle
        bool autoUpdate = config::automatic_update;
        if (ImGui::Checkbox("Automatic Updates (F6)", &autoUpdate)) {
            config::automatic_update = autoUpdate;
            config::save();
        }
#endif

        // Extra flags toggle
        bool extraFlags = config::show_extra_flags;
        if (ImGui::Checkbox("Extra Flags (F7)", &extraFlags)) {
            config::show_extra_flags = extraFlags;
            config::save();
        }

        // Skeleton ESP toggle
        bool skeletonEsp = config::show_skeleton_esp;
        if (ImGui::Checkbox("Skeleton ESP (F8)", &skeletonEsp)) {
            config::show_skeleton_esp = skeletonEsp;
            config::save();
        }

        // Head tracker toggle
        bool headTracker = config::show_head_tracker;
        if (ImGui::Checkbox("Head Tracker (F9)", &headTracker)) {
            config::show_head_tracker = headTracker;
            config::save();
        }

        // Exit button
        if (ImGui::Button("Exit ESP (END)")) {
            m_finish = true;
        }

        ImGui::End();
    }

    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(m_glfwWindow, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(m_glfwWindow);
}

void App::CleanupImGui() {
    if (m_imguiInitialized) {
        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        if (m_glfwWindow) {
            glfwDestroyWindow(m_glfwWindow);
            m_glfwWindow = nullptr;
        }
        glfwTerminate();

        m_imguiInitialized = false;
        CLOG_INFO("[ImGui] ImGui cleanup completed");
    }
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
    std::cout << "\n[settings] In Game keybinds:\n\t[F4] enable/disable Box ESP\n\t[F5] enable/disable Team ESP\n\t[F6] enable/disable automatic updates\n\t[F7] enable/disable extra flags\n\t[F8] enable/disable skeleton esp\n\t[F9] enable/disable head tracker\n\t[end] Unload esp.\n" << std::endl;
#else
    std::cout << "\n[settings] In Game keybinds:\n\t[F4] enable/disable Box ESP\n\t[F5] enable/disable Team ESP\n\t[F7] enable/disable extra flags\n\t[F8] enable/disable skeleton esp\n\t[F9] enable/disable head tracker\n\t[end] Unload esp.\n" << std::endl;
#endif
    std::cout << "[settings] Make sure you check the config for additional settings!" << std::endl;
    std::cout << "[ImGui] ImGui configuration window is now available!" << std::endl;

    MessageLoop();

    return 1;
}

void App::Shutdown() {
    m_finish = true;
    if (m_readThread.joinable()) {
        m_readThread.detach();
    }
    
    // Cleanup ImGui first
    CleanupImGui();
    
    Beep(700, 100);
    Beep(700, 100);
    std::cout << "[overlay] Destroying overlay window." << std::endl;

    // Clean up resources that aren't handled by WM_DESTROY
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

    GetClientRect(g_game.process->hwnd_, &g::gameBounds);

    m_hInstance = NULL; // Like original
    m_hWnd = CreateWindowExA(
        WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        " ",
        "cs2-external-esp",
        WS_POPUP,
        g::gameBounds.left,
        g::gameBounds.top,
        g::gameBounds.right - g::gameBounds.left,
        g::gameBounds.bottom + g::gameBounds.left,
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
}

void App::MessageLoop() {
    MSG msg;
    while (!m_finish && (!m_glfwWindow || !glfwWindowShouldClose(m_glfwWindow))) {
        // Handle Windows messages for overlay
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                m_finish = true;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (m_finish) break;

        HandleKeyInput();

        // Render ImGui
        RenderImGui();

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

LRESULT CALLBACK App::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    App* app = App::s_instance;

    switch (message) {
    case WM_CREATE:
    {
        g::hdcBuffer = CreateCompatibleDC(NULL);
        g::hbmBuffer = CreateCompatibleBitmap(GetDC(hWnd), g::gameBounds.right, g::gameBounds.bottom);
        SelectObject(g::hdcBuffer, g::hbmBuffer);

        SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
        SetLayeredWindowAttributes(hWnd, RGB(255, 255, 255), 0, LWA_COLORKEY);

        std::cout << "[overlay] Window created successfully" << std::endl;
        Beep(500, 100);
        break;
    }
    case WM_ERASEBKGND: // We handle this message to avoid flickering
        return TRUE;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        //DOUBLE BUFFERING
        FillRect(g::hdcBuffer, &ps.rcPaint, (HBRUSH)GetStockObject(WHITE_BRUSH));

        if (GetForegroundWindow() == g_game.process->hwnd_) {
            //render::RenderText(g::hdcBuffer, 10, 10, "cs2 | ESP", RGB(75, 175, 175), 15);
            hack::loop();
        }

        BitBlt(hdc, 0, 0, g::gameBounds.right, g::gameBounds.bottom, g::hdcBuffer, 0, 0, SRCCOPY);

        EndPaint(hWnd, &ps);
        InvalidateRect(hWnd, NULL, TRUE);
        break;
    }
    case WM_DESTROY:
        DeleteDC(g::hdcBuffer);
        DeleteObject(g::hbmBuffer);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}