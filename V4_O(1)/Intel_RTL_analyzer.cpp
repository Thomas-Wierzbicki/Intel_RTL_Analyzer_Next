#include "Intel_RTL_analyzer.h"
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QWidget>
#include <QtWidgets/QStatusBar>
#include <QtGui/QPen>
#include <QtGui/QColor>
#include "qcustomplot.h"

Intel_RTL_analyzer::Intel_RTL_analyzer(QWidget* parent)
    : QMainWindow(parent), m_worker(new SDRWorker(this)), m_plot(nullptr), m_paintTimer(new QTimer(this))
{
    ui.setupUi(this);
    setWindowTitle("Intel IPP Core - Ultra-SDR Analyzer & FM Sim");
    resize(1000, 700);

    // 1. ZENTRALES LAYOUT AUFBAUEN
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    setCentralWidget(centralWidget);

    // 2. DIAGRAMM EINBETTEN
    m_plot = new QCustomPlot(this);
    mainLayout->addWidget(m_plot, 1); // Nimmt den Hauptplatz ein

    // 3. STEUERUNG FÜR FM-SIMULATOR AUFBAUEN (Horizontalbar unten)
    QHBoxLayout* controlLayout = new QHBoxLayout();

    m_simEnableCheck = new QCheckBox("FM Simulator aktivieren", this);
    controlLayout->addWidget(m_simEnableCheck);

    QLabel* sliderLabel = new QLabel("Sim-Frequenz:", this);
    controlLayout->addWidget(sliderLabel);

    m_freqSlider = new QSlider(Qt::Horizontal, this);
    m_freqSlider->setRange(-1000, 1000); // Erlaubt das Schieben um +/- 1 MHz vom Zentrum
    m_freqSlider->setValue(0);           // Start exakt in der Bandmitte (433.5 MHz)
    m_freqSlider->setEnabled(false);     // Standardmäßig aus
    controlLayout->addWidget(m_freqSlider, 1);

    mainLayout->addLayout(controlLayout);

    // 4. QCUSTOMPLOT SPEKTRUM KONFIGURIEREN
    m_plot->addGraph();
    m_plot->graph(0)->setPen(QPen(QColor(0, 120, 255), 1.2));
    m_plot->xAxis->setLabel("Frequenz (70cm Amateurband - MHz)");
    m_plot->yAxis->setLabel("Leistungsspektrum (dB)");
    m_plot->xAxis->setRange(432.30, 434.70);
    m_plot->yAxis->setRange(-20.0, 50.0);

    QSharedPointer<QCPAxisTickerText> textTicker(new QCPAxisTickerText);
    textTicker->addTick(432.30, "432.3 MHz");
    textTicker->addTick(432.90, "432.9 MHz");
    textTicker->addTick(433.50, "433.5 MHz");
    textTicker->addTick(434.10, "434.1 MHz");
    textTicker->addTick(434.70, "434.7 MHz");
    m_plot->xAxis->setTicker(textTicker);

    // 5. SIGNALE UND SLOTS VERKNÜPFEN
    connect(m_simEnableCheck, &QCheckBox::toggled, this, &Intel_RTL_analyzer::onSimulatorToggled);
    connect(m_freqSlider, &QSlider::valueChanged, this, &Intel_RTL_analyzer::onSliderValueChanged);
    connect(m_paintTimer, &QTimer::timeout, this, &Intel_RTL_analyzer::updatePlot);

    m_paintTimer->start(40); // 25 FPS
    m_worker->start();
}

Intel_RTL_analyzer::~Intel_RTL_analyzer()
{
    m_paintTimer->stop();
    m_worker->stop();
}

void Intel_RTL_analyzer::onSimulatorToggled(bool checked)
{
    m_freqSlider->setEnabled(checked);
    if (m_worker) {
        // Dem Worker mitteilen, ob er simulieren oder echte USB-Daten lesen soll
        m_worker->setProperty("simulationActive", checked);
    }
}

void Intel_RTL_analyzer::onSliderValueChanged(int value)
{
    if (m_worker) {
        // Skalierung: Der Slider-Wert wird in eine Frequenzverschiebung in Hz umgerechnet
        double offsetHz = value * 1000.0; // +/- 1 MHz in 1-kHz-Schritten
        m_worker->setProperty("simulationOffsetHz", offsetHz);
    }
}

void Intel_RTL_analyzer::onFftDataReady(const QVector<double>&, const QVector<double>&) {}

void Intel_RTL_analyzer::updatePlot()
{
    m_worker->getLatestData(m_currentFrequencies, m_currentAmplitudes);
    if (m_currentFrequencies.isEmpty() || m_currentAmplitudes.isEmpty() || !m_plot) return;

    QVector<double> freqsMhz(m_currentFrequencies.size());
    for (int i = 0; i < m_currentFrequencies.size(); ++i) {
        freqsMhz[i] = m_currentFrequencies[i] / 1000000.0;
    }

    if (m_smoothedAmplitudes.size() != m_currentAmplitudes.size()) {
        m_smoothedAmplitudes = m_currentAmplitudes;
    }
    else {
        const double alpha = 0.12;
        for (int i = 0; i < m_currentAmplitudes.size(); ++i) {
            m_smoothedAmplitudes[i] = (alpha * m_currentAmplitudes[i]) + ((1.0 - alpha) * m_smoothedAmplitudes[i]);
        }
    }

    m_plot->graph(0)->setData(freqsMhz, m_smoothedAmplitudes);
    m_plot->replot(QCustomPlot::rpRefreshHint);
}