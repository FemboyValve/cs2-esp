#pragma once
#include <windows.h>
#include <thread>
#include <chrono>
#include <iostream>

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

    // Hardware acceleration toggle
    bool m_useHardwareAccel;

    // Private methods
    bool InitializeOffsets();
    bool InitializeConfig();
#ifndef _UC
    void CheckForUpdates();
#endif
    bool CreateOverlayWindow();
    void StartReadThread();
    void HandleKeyInput();
    void MessageLoop();

    // Thread function
    void ReadThreadFunction();

    // Window procedure - needs to be static to be used as callback
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    // Store instance pointer for window procedure access
    static App* s_instance;

    // Make WndProc a friend so it can access private members
    friend LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

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

    // Hardware acceleration control
    bool IsHardwareAccelEnabled() const { return m_useHardwareAccel; }
    void SetHardwareAcceleration(bool enable) { m_useHardwareAccel = enable; }
};