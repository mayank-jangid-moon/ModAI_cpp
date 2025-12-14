#include <QApplication>
#include "ui/MainWindow.h"
#include "utils/Logger.h"
#include <iostream>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    app.setApplicationName("ModAI");
    app.setOrganizationName("ModAI");
    
    ModAI::MainWindow window;
    window.show();
    
    return app.exec();
}

