#include <QtWidgets/QApplication>
#include "Intel_RTL_analyzer.h"
#include "WaterfallWindow.h" // Den neuen Wasserfall einbinden

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // 1. Hauptfenster oeffnen (erzeugt intern den SDRWorker)
    Intel_RTL_analyzer mainWindow;
    mainWindow.show();

    // 2. Den Hardware-Worker Zeiger aus dem Hauptfenster herausholen
    // Wir nutzen das in der GUI eingebettete Worker-Objekt
    SDRWorker* sharedWorker = mainWindow.findChild<SDRWorker*>();

    // 3. Wasserfall-Fenster als eigenstaendiges, paralleles Fenster oeffnen
    DisplayState* sharedState = mainWindow.findChild<DisplayState*>();
    WaterfallWindow waterfallWindow(sharedWorker, sharedState);
    waterfallWindow.show();

    return app.exec();
}