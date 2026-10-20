#ifndef INTEL_RTL_ANALYZER_H
#define INTEL_RTL_ANALYZER_H

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QSlider>
#include <QtWidgets/QCheckBox>
#include <QtCore/QVector>
#include <QtCore/QTimer>
#include <QtCore/QElapsedTimer>
#include <QtCore/QPointF>
#include "ui_Intel_RTL_analyzer.h"
#include "sdrworker.h"
#include "displaystate.h"
#include <QtWidgets/QComboBox>

class QCustomPlot;
class SpectrumCursorOverlay;
class QLabel;

class Intel_RTL_analyzer : public QMainWindow
{
    Q_OBJECT

public:
    explicit Intel_RTL_analyzer(QWidget* parent = nullptr);
    ~Intel_RTL_analyzer();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onFftDataReady(const QVector<double>& frequencies, const QVector<double>& amplitudes);
    void updatePlot();

    // Neue Slots für die Steuerung des Simulators
    void onSimulatorToggled(bool checked);
    void onSliderValueChanged(int value);

private:
    void updateCursorReadout();
    void refreshMarkers();
    DisplayState* m_displayState = nullptr;
    QLabel* m_deviceStatus = nullptr;
    QLabel* m_markerReadout = nullptr;
    QComboBox* m_cursorSource = nullptr;
    QComboBox* m_markerMode = nullptr;
    bool m_markerValid[2] = {false, false};
    double m_markerFrequency[2] = {0, 0};
    double m_markerLevel[2] = {0, 0};
    double m_selectedFrequency = 0, m_selectedLevel = 0;
    bool m_selectedValid = false;
    double m_fullLowerMHz = 432.30, m_fullUpperMHz = 434.70;
    SpectrumCursorOverlay* m_cursorOverlay = nullptr;
    QLabel* m_cursorReadout = nullptr;
    QPointF m_cursorPosition;
    bool m_cursorInside = false;
    QVector<double> m_displayFrequencies[2];
    QVector<double> m_displayLevels[2];
    Ui::Intel_RTL_analyzerClass ui;
    SDRWorker* m_worker;
    QCustomPlot* m_plot;
    QTimer* m_paintTimer;
    QElapsedTimer m_replotClock;

    // Neue GUI-Elemente für den FM-Simulator
    QSlider* m_freqSlider;
    QCheckBox* m_simEnableCheck;
    QCheckBox* m_peakHoldCheck;
    QVector<double> m_peakAmplitudes;
    quint64 m_lastSequence = 0;
    quint64 m_lastEpoch = 0;
    bool m_viewDirty = false;

    QVector<double> m_currentFrequencies;
    QVector<double> m_currentAmplitudes;
    QVector<double> m_smoothedAmplitudes;
};

#endif // INTEL_RTL_ANALYZER_H



