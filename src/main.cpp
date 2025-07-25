#include "app.hpp"

int main(int argc, char* argv[]) {
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