#pragma once
#include <windows.h>
#include <thread>
#include <chrono>
#include <iostream>

// ImGui includes
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

class App {
private:
    HWND m_hWnd;
    HINSTANCE m_hInstance;
    bool m_finish;
    std::thread m_readThread;

    // Keep local copies but use globals for compatibility
    HDC m_hdcBuffer;
    HBITMAP m_hbmBuffer;
    RECT m_gameBounds;

    // ImGui/GLFW members
    GLFWwindow* m_glfwWindow;
    bool m_showImGuiDemo;
    bool m_showConfigWindow;
    bool m_imguiInitialized;

    // Private methods
    bool InitializeOffsets();
    bool InitializeConfig();
#ifndef _UC
    void CheckForUpdates();
#endif
    bool CreateOverlayWindow();
    bool InitializeImGui();
    void StartReadThread();
    void HandleKeyInput();
    void MessageLoop();
    void RenderImGui();
    void CleanupImGui();

    // Thread function
    void ReadThreadFunction();

    // Window procedure - needs to be static to be used as callback
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    // Store instance pointer for window procedure access
    static App* s_instance;

public:
    App();
    ~App();

    // Main application methods
    int Initialize(int argc, char* argv[]);
    int Run();
    void Shutdown();

    // Control methods
    void RequestShutdown() { m_finish = true; }
    bool IsFinishing() const { return m_finish; }
};