#include "app.hpp"

int main(int argc, char* argv[])
{
    App app;

    // Initialize the application
    int initResult = app.Initialize(argc, argv);
    if (initResult != 0) {
        return initResult;
    }

    // Run the application
    int runResult = app.Run();

    // Shutdown
    app.Shutdown();

    return runResult;
}

#if WIN32
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
// get argc and argv
	int argc;
	wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	return main(argc, (char**)argv);
}
#endif

