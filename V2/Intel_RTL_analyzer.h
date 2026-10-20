#ifndef INTEL_RTL_ANALYZER_H
#define INTEL_RTL_ANALYZER_H

#include <QtWidgets/QMainWindow>
#include <QtCore/QVector>
#include <QtCore/QTimer>
#include "ui_Intel_RTL_analyzer.h"
#include "sdrworker.h"

class QCustomPlot;

class Intel_RTL_analyzer : public QMainWindow
{
    Q_OBJECT

public:
    explicit Intel_RTL_analyzer(QWidget* parent = nullptr);
    ~Intel_RTL_analyzer();

private slots:
    void onFftDataReady(const QVector<double>& frequencies, const QVector<double>& amplitudes);
    void updatePlot();

private:
    Ui::Intel_RTL_analyzerClass ui;
    SDRWorker* m_worker;
    QCustomPlot* m_plot;
    QTimer* m_paintTimer;

    QVector<double> m_currentFrequencies;
    QVector<double> m_currentAmplitudes;
    QVector<double> m_smoothedAmplitudes;
};

#endif // INTEL_RTL_ANALYZER_H
