#pragma once

#include <windows.h>
#include <thread>

class OverlayWindow {
private:
    static OverlayWindow* s_instance;
    
    HWND m_hWnd;
    HINSTANCE m_hInstance;
    bool m_useHardwareAccel;
    
    // GDI resources
    HDC m_hdcBuffer;
    HBITMAP m_hbmBuffer;
    RECT m_gameBounds;

    // Private constructor for singleton
    OverlayWindow();

    // Private methods
    bool _CreateWindow();
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

public:
    ~OverlayWindow();

    // Singleton access
    static OverlayWindow* GetInstance();
    static void DestroyInstance();

    // Window management
    bool Initialize(bool useHardwareAccel = true);
    void Cleanup();
    bool IsValid() const { return m_hWnd != nullptr; }

    // Accessors
    HWND GetHWND() const { return m_hWnd; }
    HDC GetBufferDC() const { return m_hdcBuffer; }
    const RECT& GetGameBounds() const { return m_gameBounds; }
    
    // Static convenience methods
    static HDC GetCurrentBufferDC() {
        OverlayWindow* instance = GetInstance();
        return instance ? instance->GetBufferDC() : nullptr;
    }
    
    static HWND GetCurrentHWND() {
        OverlayWindow* instance = GetInstance();
        return instance ? instance->GetHWND() : nullptr;
    }

    // Hardware acceleration control
    bool IsHardwareAccelEnabled() const { return m_useHardwareAccel; }
    void SetHardwareAcceleration(bool enable);
    void ToggleHardwareAcceleration();

    // Window operations
    void Show();
    void Hide();
    void UpdateBounds();

    // Prevent copying
    OverlayWindow(const OverlayWindow&) = delete;
    OverlayWindow& operator=(const OverlayWindow&) = delete;
};