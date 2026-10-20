#pragma once
#ifndef WATERFALL_WINDOW_H
#define WATERFALL_WINDOW_H

#include <QtWidgets/QMainWindow>
#include <QtCore/QVector>
#include <QtCore/QTimer>
#include "qcustomplot.h"
#include "sdrworker.h" // Verbindung zur Hardware-Engine

class WaterfallWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit WaterfallWindow(SDRWorker* worker, QWidget* parent = nullptr);
    ~WaterfallWindow();

private slots:
    // Taktgeber für flüssiges, ressourcenschonendes Zeichnen des Wasserfalls
    void updateWaterfall();

private:
    QCustomPlot* m_plot;
    QCPColorMap* m_waterfallMap;
    SDRWorker* m_worker;
    QTimer* m_updateTimer;

    // Lokale Puffer für den Datenabruf
    QVector<double> m_freqBuffer;
    QVector<double> m_ampBuffer;
};

#endif // WATERFALL_WINDOW_H
