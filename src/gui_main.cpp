#include "../include/mainwindow.h"
#include "../include/platform_wrapper.h"
#include <QApplication>

// main entry point to GUI client

int main(int argc, char *argv[]) {
    // initialize platform utils for networking across platforms
    if (!PlatformUtils::initialize()) {
        return 1;
    }
    
    QApplication app(argc, argv);
    
    MainWindow window;
    window.show();
    
    int result = app.exec();
    
    PlatformUtils::cleanup();
    
    return result;
}