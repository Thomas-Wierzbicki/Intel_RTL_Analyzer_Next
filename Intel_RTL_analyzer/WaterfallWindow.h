#ifndef WATERFALL_WINDOW_H
#define WATERFALL_WINDOW_H
#include <QtWidgets/QMainWindow>
#include <QtCore/QTimer>
#include "sdrworker.h"
#include "displaystate.h"
class WaterfallCanvas;
class WaterfallWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit WaterfallWindow(SDRWorker* worker, DisplayState* state, QWidget* parent = nullptr);
    ~WaterfallWindow();
private:
    void updateWaterfall();
    WaterfallCanvas* m_canvas;
    SDRWorker* m_worker;
    DisplayState* m_state;
    QTimer* m_updateTimer;
    QVector<double> m_frequencies, m_maxima;
    quint64 m_lastEpoch = 0;
};
#endif
