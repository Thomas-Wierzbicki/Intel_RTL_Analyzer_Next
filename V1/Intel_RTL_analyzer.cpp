#include "Intel_RTL_analyzer.h"
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <QtWidgets/QStatusBar>
#include <QtGui/QPen>
#include <QtGui/QColor>
#include "qcustomplot.h"

Intel_RTL_analyzer::Intel_RTL_analyzer(QWidget* parent)
    : QMainWindow(parent), m_worker(new SDRWorker(this)), m_plot(nullptr), m_paintTimer(new QTimer(this))
{
    ui.setupUi(this);
    setWindowTitle("Intel IPP Core - Ultra-SDR Analyzer (Optimized)");
    resize(1000, 600);

    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(centralWidget);
    setCentralWidget(centralWidget);

    m_plot = new QCustomPlot(this);
    layout->addWidget(m_plot);

    m_plot->addGraph();
    m_plot->graph(0)->setPen(QPen(QColor(0, 50, 240), 1.2)); // Helles Mess-Blau

    m_plot->xAxis->setLabel("Frequenz (70cm Amateurband - MHz)");
    m_plot->yAxis->setLabel("Leistungsspektrum (dB)");

    // Festes, sauberes Sichtfenster fuer die geglaetteten Werte
    m_plot->xAxis->setRange(432.30, 434.70);
    m_plot->yAxis->setRange(-20.0, 50.0);

    // Manuellen Ticker setzen, um QCustomPlot-Fehlinterpretationen zu blockieren
    QSharedPointer<QCPAxisTickerText> textTicker(new QCPAxisTickerText);
    textTicker->addTick(432.30, "432.3 MHz");
    textTicker->addTick(432.90, "432.9 MHz");
    textTicker->addTick(433.50, "433.5 MHz");
    textTicker->addTick(434.10, "434.1 MHz");
    textTicker->addTick(434.70, "434.7 MHz");
    m_plot->xAxis->setTicker(textTicker);

    connect(m_worker, &SDRWorker::fftDataReady, this, &Intel_RTL_analyzer::onFftDataReady);

    // PERFORMANCE-LIMITER: GUI-Zeichentakt fest auf 50ms drosseln (20 Bilder pro Sekunde)
    connect(m_paintTimer, &QTimer::timeout, this, &Intel_RTL_analyzer::updatePlot);
    m_paintTimer->start(50);

    m_worker->start();

    if (this->statusBar()) {
        this->statusBar()->showMessage("SDR-Engine aktiv. 20 FPS GUI-Limiter mit Intel AVX EMA-Filter.", 5000);
    }
}

Intel_RTL_analyzer::~Intel_RTL_analyzer()
{
    m_paintTimer->stop();
    m_worker->stop();
}

void Intel_RTL_analyzer::onFftDataReady(const QVector<double>& frequencies, const QVector<double>& amplitudes)
{
    // Kopiert Daten nur extrem schnell in den RAM, blockiert die GUI nicht
    m_currentFrequencies = frequencies;
    m_currentAmplitudes = amplitudes;
}

void Intel_RTL_analyzer::updatePlot()
{
    if (m_currentFrequencies.isEmpty() || m_currentAmplitudes.isEmpty() || !m_plot) return;

    QVector<double> freqsMhz(m_currentFrequencies.size());
    for (int i = 0; i < m_currentFrequencies.size(); ++i) {
        freqsMhz[i] = m_currentFrequencies[i] / 1000000.0;
    }

    // Exponentieller Gleitdurchschnitt (EMA-Glättungsfilter)
    if (m_smoothedAmplitudes.size() != m_currentAmplitudes.size()) {
        m_smoothedAmplitudes = m_currentAmplitudes;
    }
    else {
        const double alpha = 0.15; // Daempfungsfaktor (kleiner = ruhiger, groesser = agiler)
        for (int i = 0; i < m_currentAmplitudes.size(); ++i) {
            m_smoothedAmplitudes[i] = (alpha * m_currentAmplitudes[i]) + ((1.0 - alpha) * m_smoothedAmplitudes[i]);
        }
    }

    m_plot->graph(0)->setData(freqsMhz, m_smoothedAmplitudes);
    m_plot->replot(QCustomPlot::rpRefreshHint);
}
