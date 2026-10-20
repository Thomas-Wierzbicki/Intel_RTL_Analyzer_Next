#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include "Intel_RTL_analyzer.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    Intel_RTL_analyzer mainWindow;
    mainWindow.show();

    return app.exec();
}
