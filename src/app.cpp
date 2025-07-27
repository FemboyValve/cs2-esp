#include "app.hpp"
#include "window.hpp"
#include "classes/utils.h"
#include "memory/memory.hpp"
#include "classes/vector.hpp"
#include "hacks/reader.hpp"
#include "hacks/hack.hpp"
#include "classes/renderer.hpp"
#include "classes/auto_updater.hpp"
#include "utils/log.h"

// Static member initialization
App* App::s_instance = nullptr;

// Global hardware renderer instance
render::HardwareRenderer render::g_hwRenderer;

App::App() : m_finish(false) {
    s_instance = this;
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

    // Initialize overlay window
    OverlayWindow* window = OverlayWindow::GetInstance();
    if (!window->Initialize()) {
        CLOG_WARN("[overlay] Failed to initialize overlay window");
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

    // Cleanup overlay window
    OverlayWindow::DestroyInstance();

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
    OverlayWindow* window = OverlayWindow::GetInstance();
    HWND hwnd = window ? window->GetHWND() : nullptr;
    
    while (GetMessage(&msg, NULL, 0, 0) && !m_finish) {
        HandleKeyInput();

        TranslateMessage(&msg);
        DispatchMessage(&msg);

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}