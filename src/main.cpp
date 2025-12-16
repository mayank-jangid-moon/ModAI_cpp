#include <QApplication>
#include <QCoreApplication>
#include "ui/MainWindow.h"
#include "core/AppController.h"
#include "network/HttpServer.h"
#include "utils/Logger.h"
#include <iostream>

int main(int argc, char *argv[]) {
    bool headless = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--server") {
            headless = true;
            break;
        }
    }

    if (headless) {
        // Use QCoreApplication for headless mode
        QCoreApplication app(argc, argv);
        app.setApplicationName("ModAI-Server");
        app.setOrganizationName("ModAI");

        ModAI::AppController controller;
        ModAI::HttpServer server(&controller);
        server.start();

        return app.exec();
    } else {
        // Use QApplication for GUI mode
        QApplication app(argc, argv);
        app.setApplicationName("ModAI");
        app.setOrganizationName("ModAI");
        
        ModAI::MainWindow window;
        window.show();
        
        return app.exec();
    }
}

