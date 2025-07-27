#pragma once

#include <windows.h>
#include <thread>

class App {
private:
    bool m_finish;
    std::thread m_readThread;

    // Private methods
    bool InitializeOffsets();
    bool InitializeConfig();
#ifndef _UC
    void CheckForUpdates();
#endif
    void StartReadThread();
    void HandleKeyInput();
    void MessageLoop();

    // Thread function
    void ReadThreadFunction();

    // Store instance pointer for access
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