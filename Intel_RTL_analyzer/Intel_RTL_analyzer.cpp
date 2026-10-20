#include "Intel_RTL_analyzer.h"
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QToolBar>
#include <QtGui/QAction>
#include <QtCore/QSignalBlocker>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>
#include <QtWidgets/QStatusBar>
#include <QtGui/QPen>
#include <QtGui/QColor>
#include "qcustomplot.h"
#include "spectrumdisplay.h"
#include "frequencyzoom.h"
#include "cursorreadout.h"
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtGui/QWheelEvent>
#include <QtGui/QMouseEvent>

// Lightweight child overlay: mouse motion never calls QCustomPlot::replot.
class SpectrumCursorOverlay : public QWidget
{
public:
    explicit SpectrumCursorOverlay(QWidget* parent) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        hide();
    }
    void setArea(const QRectF& area) { m_area = area; }
    void clearCross() { m_hasCross = false; setVisible(!m_markers.isEmpty()); update(); }
    void setMarkers(const QVector<QPointF>& points) {
        m_markers = points; setVisible(m_hasCross || !points.isEmpty()); update();
    }
    void setCross(const QRectF& area, const QPointF& point, const QColor& color) {
        m_hasCross = true; m_area = area; m_point = point; m_color = color;
        show(); raise(); update();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setClipRect(m_area);
        QPen pen(m_color); pen.setWidth(0); pen.setStyle(Qt::DashLine);
        painter.setPen(pen);
        if (m_hasCross) {
        painter.drawLine(QPointF(m_point.x(), m_area.top()), QPointF(m_point.x(), m_area.bottom()));
        painter.drawLine(QPointF(m_area.left(), m_point.y()), QPointF(m_area.right(), m_point.y()));
        painter.setPen(QPen(m_color, 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(m_point, 4, 4);
        }
        for (int i = 0; i < m_markers.size(); ++i) {
            const QPointF p = m_markers[i];
            if (!m_area.contains(p)) continue;
            painter.setPen(QPen(i == 0 ? Qt::darkGreen : Qt::magenta, 1));
            painter.drawLine(QPointF(p.x(), m_area.top()), QPointF(p.x(), m_area.bottom()));
            painter.drawEllipse(p, 5, 5);
            painter.drawText(p + QPointF(7, -7), i == 0 ? "A" : "B");
        }
    }
private:
    bool m_hasCross = false;
    QVector<QPointF> m_markers;
    QRectF m_area;
    QPointF m_point;
    QColor m_color;
};

Intel_RTL_analyzer::Intel_RTL_analyzer(QWidget* parent)
    : QMainWindow(parent), m_worker(new SDRWorker(this)), m_plot(nullptr), m_paintTimer(new QTimer(this))
{
    ui.setupUi(this);
    m_displayState = new DisplayState(this);
    setWindowTitle("Intel IPP Core - SDR Analyzer Next");
    resize(1000, 700);

    // 1. ZENTRALES LAYOUT AUFBAUEN
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    setCentralWidget(centralWidget);

    // 2. DIAGRAMM EINBETTEN
    m_plot = new QCustomPlot(this);
    mainLayout->addWidget(m_plot, 1); // Nimmt den Hauptplatz ein
    m_cursorReadout = new QLabel("Maus auf Messpunkt | Klick ins Diagramm, dann Pfeil links/rechts zum Verschieben", this);
    m_cursorReadout->setWordWrap(true);
    m_cursorReadout->setFixedHeight(2 * m_cursorReadout->fontMetrics().lineSpacing() + 4);
    m_cursorReadout->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    mainLayout->addWidget(m_cursorReadout);
    m_cursorOverlay = new SpectrumCursorOverlay(m_plot);
    m_cursorOverlay->setGeometry(m_plot->rect());
    m_plot->setMouseTracking(true);
    m_plot->setFocusPolicy(Qt::StrongFocus);
    m_plot->installEventFilter(this);

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

    QHBoxLayout* peakLayout = new QHBoxLayout();
    m_peakHoldCheck = new QCheckBox("Peak Hold (gelb)", this);
    m_peakHoldCheck->setChecked(true);
    m_peakHoldCheck->setToolTip("Maximum jeder FFT seit dem letzten Reset. Ausschalten blendet nur die Kurve aus.");
    QPushButton* resetPeaks = new QPushButton("Peak Hold zuruecksetzen", this);
    peakLayout->addWidget(m_peakHoldCheck);
    peakLayout->addWidget(resetPeaks);
    peakLayout->addStretch();
    mainLayout->addLayout(peakLayout);

    // 4. QCUSTOMPLOT SPEKTRUM KONFIGURIEREN
    m_plot->addGraph();
    QPen livePen(QColor(0, 120, 255));
    livePen.setWidth(0); // Cosmetic one-device-pixel stroke, no thick-line tessellation.
    m_plot->graph(0)->setPen(livePen);
    m_plot->graph(0)->setName("Live (geglaettet)");
    m_plot->addGraph();
    QPen peakPen(QColor(220, 170, 0));
    peakPen.setWidth(0);
    m_plot->graph(1)->setPen(peakPen);
    m_plot->setNotAntialiasedElements(QCP::aePlottables);
    m_plot->setPlottingHint(QCP::phFastPolylines, true);
    m_plot->graph(0)->setAdaptiveSampling(true);
    m_plot->graph(1)->setAdaptiveSampling(true);
    m_plot->graph(1)->setName("Peak Hold");
    m_plot->legend->setVisible(true);
    m_plot->xAxis->setLabel("Frequenz (70cm Amateurband - MHz)");
    m_plot->yAxis->setLabel("Tonpegel pro FFT-Bin (dBFS)");
    m_plot->xAxis->setRange(m_fullLowerMHz, m_fullUpperMHz);
    m_plot->yAxis->setRange(-100.0, 0.0);

    m_plot->xAxis->setTicker(QSharedPointer<QCPAxisTicker>(new QCPAxisTicker));
    m_plot->xAxis->setNumberFormat("f");
    m_plot->xAxis->setNumberPrecision(4);
    m_plot->setToolTip("Mausrad: Zoom. Doppelklick: voller Bereich. Klick, dann Pfeil links/rechts: Ausschnitt verschieben. Maus: Messwert.");
    connect(m_plot, &QCustomPlot::mouseWheel, this, [this](QWheelEvent* event) {
        const QRect area = m_plot->axisRect()->rect();
        if (!area.contains(event->position().toPoint()) || area.width() <= 0) return;
        const double steps = event->angleDelta().y() != 0 ?
            event->angleDelta().y() / 120.0 : event->pixelDelta().y() / 40.0;
        if (steps == 0.0) return;
        const auto range = m_plot->xAxis->range();
        const auto view = zoomFrequency({range.lower, range.upper}, m_fullLowerMHz, m_fullUpperMHz,
            (event->position().x() - area.left()) / area.width(), steps);
        if (m_displayState->coupled) m_displayState->setView(view);
        m_plot->xAxis->setRange(view.lower, view.upper);
        m_viewDirty = true;
        updatePlot();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
        event->accept();
    });
    connect(m_plot, &QCustomPlot::mouseDoubleClick, this, [this](QMouseEvent* event) {
        if (event->button() != Qt::LeftButton ||
            !m_plot->axisRect()->rect().contains(event->position().toPoint())) return;
        if (m_displayState->coupled)
            m_displayState->setView({m_fullLowerMHz, m_fullUpperMHz});
        m_plot->xAxis->setRange(m_fullLowerMHz, m_fullUpperMHz);
        m_viewDirty = true;
        updatePlot();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    });

    auto* tools = addToolBar("Ansicht");
    auto* pause = tools->addAction("Pause");
    pause->setCheckable(true);
    pause->setShortcut(QKeySequence(Qt::Key_Space));
    connect(pause, &QAction::toggled, m_displayState, &DisplayState::setPaused);
    connect(m_displayState, &DisplayState::pauseChanged, pause, &QAction::setChecked);
    auto* coupled = tools->addAction("Zoom koppeln");
    coupled->setCheckable(true); coupled->setChecked(true);
    connect(coupled, &QAction::toggled, m_displayState, &DisplayState::setCoupled);
    tools->addSeparator();
    tools->addWidget(new QLabel(" Cursor: ", this));
    m_cursorSource = new QComboBox(this);
    m_cursorSource->addItems({"Automatisch", "Live", "Peak Hold"});
    tools->addWidget(m_cursorSource);
    connect(m_cursorSource, &QComboBox::currentIndexChanged, this, [this](int) { updateCursorReadout(); });
    m_markerMode = new QComboBox(this);
    m_markerMode->addItems({"Maus", "Marker A setzen", "Marker B setzen"});
    tools->addWidget(m_markerMode);
    auto* clearMarkers = tools->addAction("Marker loeschen");
    connect(clearMarkers, &QAction::triggered, this, [this]() {
        m_markerValid[0] = m_markerValid[1] = false; refreshMarkers();
    });
    auto* scaleTools = addToolBar("Pegel");
    insertToolBarBreak(scaleTools);
    auto* minimum = new QDoubleSpinBox(this);
    minimum->setRange(-200, 100); minimum->setValue(-100); minimum->setSuffix(" dB");
    auto* dynamicRange = new QDoubleSpinBox(this);
    dynamicRange->setRange(10, 200); dynamicRange->setValue(100); dynamicRange->setSuffix(" dB");
    auto* offset = new QDoubleSpinBox(this);
    offset->setRange(-150, 150); offset->setValue(0); offset->setSuffix(" dB");
    offset->setToolTip("Manueller Anzeigeoffset. Keine automatische dBm-Kalibrierung; Tuner-AGC beachten.");
    scaleTools->addWidget(new QLabel(" Untergrenze: ", this)); scaleTools->addWidget(minimum);
    scaleTools->addWidget(new QLabel(" Dynamik: ", this)); scaleTools->addWidget(dynamicRange);
    scaleTools->addWidget(new QLabel(" Offset: ", this)); scaleTools->addWidget(offset);
    auto setScale = [this, minimum, dynamicRange, offset](double) {
        m_displayState->setScale(minimum->value(), dynamicRange->value(), offset->value());
    };
    connect(minimum, &QDoubleSpinBox::valueChanged, this, setScale);
    connect(dynamicRange, &QDoubleSpinBox::valueChanged, this, setScale);
    connect(offset, &QDoubleSpinBox::valueChanged, this, setScale);
    m_markerReadout = new QLabel("Marker: A/B im Diagramm setzen", this);
    m_markerReadout->setWordWrap(true);
    m_markerReadout->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_markerReadout->setFixedHeight(2 * m_markerReadout->fontMetrics().lineSpacing() + 4);
    mainLayout->addWidget(m_markerReadout);
    m_deviceStatus = new QLabel(m_worker->statusText(), this);
    mainLayout->addWidget(m_deviceStatus);
    connect(m_worker, &SDRWorker::statusChanged, m_deviceStatus, &QLabel::setText);
    connect(m_displayState, &DisplayState::viewChanged, this, [this]() {
        if (!m_displayState->coupled) return;
        m_plot->xAxis->setRange(m_displayState->view.lower, m_displayState->view.upper);
        m_viewDirty = true; updatePlot(); refreshMarkers();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    });
    connect(m_displayState, &DisplayState::scaleChanged, this, [this]() {
        m_plot->yAxis->setRange(m_displayState->minimumDb,
            m_displayState->minimumDb + m_displayState->rangeDb);
        m_plot->yAxis->setLabel(QString("Tonpegel pro FFT-Bin (%1)").arg(m_displayState->levelUnit()));
        m_markerValid[0] = m_markerValid[1] = false;
        m_viewDirty = true; updatePlot(); refreshMarkers();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    });
    connect(m_displayState, &DisplayState::pauseChanged, this, [this](bool paused) {
        if (!paused) { m_viewDirty = true; updatePlot(); }
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    });

    // 5. SIGNALE UND SLOTS VERKNÜPFEN
    connect(m_simEnableCheck, &QCheckBox::toggled, this, &Intel_RTL_analyzer::onSimulatorToggled);
    connect(m_freqSlider, &QSlider::valueChanged, this, &Intel_RTL_analyzer::onSliderValueChanged);
    connect(m_paintTimer, &QTimer::timeout, this, &Intel_RTL_analyzer::updatePlot);

    connect(m_peakHoldCheck, &QCheckBox::toggled, this, [this](bool visible) {
        m_plot->graph(1)->setVisible(visible);
        m_viewDirty = true;
        updatePlot();
        updateCursorReadout();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    });
    connect(resetPeaks, &QPushButton::clicked, this, [this]() {
        if (m_displayState->paused) return;
        m_worker->resetPeakHold();
        m_peakAmplitudes.clear();
        m_plot->graph(1)->data()->clear();
        m_displayFrequencies[1].clear();
        m_displayLevels[1].clear();
        updateCursorReadout();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    });

    connect(m_plot, &QCustomPlot::beforeReplot, this, [this]() {
        m_replotClock.start();
    });
    connect(m_plot, &QCustomPlot::afterReplot, this, [this]() {
        const qint64 renderMs = m_replotClock.elapsed();
        updateCursorReadout();
        refreshMarkers();
        // Leave event-loop time for the waterfall and controls on slower PCs.
        const qint64 requestedInterval = renderMs * 4;
        const int intervalMs = static_cast<int>(requestedInterval < 80 ? 80 :
                                               (requestedInterval > 500 ? 500 : requestedInterval));
        if (m_paintTimer->interval() != intervalMs) m_paintTimer->setInterval(intervalMs);
        statusBar()->showMessage(QString("Linienbild: %1 ms | Intervall: %2 ms | max. 800 Punkte/Kurve")
                                 .arg(renderMs).arg(intervalMs) +
            QString(" | FFT: 4096 | Binabstand: %1 Hz | Hann | %2")
                .arg(m_worker->binWidthHz(), 0, 'f', 2)
                .arg(m_displayState->paused ? "PAUSE" : "LIVE"));
    });
    m_paintTimer->start(80); // At most 12.5 spectrum frames/s; FFT rate is unchanged.
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
        m_worker->setSimulationActive(checked);
    }
}

void Intel_RTL_analyzer::onSliderValueChanged(int value)
{
    if (m_worker) {
        // Skalierung: Der Slider-Wert wird in eine Frequenzverschiebung in Hz umgerechnet
        double offsetHz = value * 1000.0; // +/- 1 MHz in 1-kHz-Schritten
        m_worker->setSimulationOffset(offsetHz);
    }
}

void Intel_RTL_analyzer::onFftDataReady(const QVector<double>&, const QVector<double>&) {}

void Intel_RTL_analyzer::updatePlot()
{
    if (!isVisible() || isMinimized() || !m_plot) return;
    quint64 sequence = 0, epoch = 0;
    if (m_displayState->paused) {
        if (!m_viewDirty) return;
        sequence = m_lastSequence; epoch = m_lastEpoch;
    } else if (!m_worker->getSnapshot(m_currentFrequencies, m_currentAmplitudes,
                                     m_peakAmplitudes, sequence, epoch)) return;
    if (sequence == 0 || (!m_viewDirty && sequence == m_lastSequence && epoch == m_lastEpoch)) return;
    if (epoch != m_lastEpoch) {
        m_smoothedAmplitudes.clear();
        m_markerValid[0] = m_markerValid[1] = false;
        m_lastEpoch = epoch;
    }
    if (m_currentFrequencies.isEmpty() || m_currentAmplitudes.isEmpty() || !m_plot) return;

    const double lower = m_currentFrequencies.first() / 1e6;
    const double upper = (m_currentFrequencies.last() + m_worker->binWidthHz()) / 1e6;
    if (lower != m_fullLowerMHz || upper != m_fullUpperMHz) {
        m_fullLowerMHz = lower; m_fullUpperMHz = upper;
        m_plot->xAxis->setRange(lower, upper);
        m_displayState->setBounds(lower, upper);
    }

    if (m_smoothedAmplitudes.size() != m_currentAmplitudes.size()) {
        m_smoothedAmplitudes = m_currentAmplitudes;
    }
    else if (sequence != m_lastSequence) {
        // Preserve approximately the former 40 ms / alpha=0.12 smoothing rate.
        const double alpha = 1.0 - std::pow(0.88, m_paintTimer->interval() / 40.0);
        double* smoothed = m_smoothedAmplitudes.data();
        const double* current = m_currentAmplitudes.constData();
        for (int i = 0; i < m_currentAmplitudes.size(); ++i)
            smoothed[i] = alpha * current[i] + (1.0 - alpha) * smoothed[i];
    }

    m_lastSequence = sequence;
    m_viewDirty = false;
    const int plotWidth = static_cast<int>(m_plot->axisRect()->width());
    const int pointBudget = plotWidth < 64 ? 64 : (plotWidth > 800 ? 800 : plotWidth);
    const double* frequencies = m_currentFrequencies.constData();
    auto displayCurve = [&](int graphIndex, const QVector<double>& values) {
        const auto range = m_plot->xAxis->range();
        const auto bins = spectrumVisibleBins(frequencies, values.constData(),
            static_cast<std::size_t>(values.size()), static_cast<std::size_t>(pointBudget),
            range.lower * 1e6, range.upper * 1e6);
        QVector<double> x(static_cast<int>(bins.size())), y(static_cast<int>(bins.size()));
        double* xp = x.data();
        double* yp = y.data();
        const double* amplitudes = values.constData();
        for (std::size_t i = 0; i < bins.size(); ++i) {
            xp[i] = frequencies[bins[i]] / 1e6;
            yp[i] = amplitudes[bins[i]] + m_displayState->offsetDb;
        }
        m_displayFrequencies[graphIndex] = x;
        m_displayLevels[graphIndex] = y;
        m_plot->graph(graphIndex)->setData(x, y, true);
    };
    displayCurve(0, m_smoothedAmplitudes);
    if (m_peakHoldCheck->isChecked() && m_peakAmplitudes.size() == m_currentFrequencies.size())
        displayCurve(1, m_peakAmplitudes);
    else {
        m_plot->graph(1)->data()->clear();
        m_displayFrequencies[1].clear();
        m_displayLevels[1].clear();
    }
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}



bool Intel_RTL_analyzer::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_plot) {
        if (event->type() == QEvent::Resize && m_cursorOverlay)
            m_cursorOverlay->setGeometry(m_plot->rect());
        if (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            m_cursorPosition = mouse->position();
            m_cursorInside = m_plot->axisRect()->rect().contains(m_cursorPosition.toPoint());
            if (event->type() == QEvent::MouseButtonPress && m_cursorInside)
                m_plot->setFocus(Qt::MouseFocusReason);
            updateCursorReadout();
            if (event->type() == QEvent::MouseButtonPress && mouse->button() == Qt::LeftButton &&
                m_cursorInside && m_selectedValid && m_markerMode->currentIndex() > 0) {
                const int marker = m_markerMode->currentIndex() - 1;
                m_markerValid[marker] = true;
                m_markerFrequency[marker] = m_selectedFrequency;
                m_markerLevel[marker] = m_selectedLevel;
                refreshMarkers();
            }
        } else if (event->type() == QEvent::Leave) {
            m_cursorInside = false;
            updateCursorReadout();
        } else if (event->type() == QEvent::KeyPress) {
            auto* key = static_cast<QKeyEvent*>(event);
            if (key->key() == Qt::Key_Left || key->key() == Qt::Key_Right) {
                const auto range = m_plot->xAxis->range();
                const auto view = panFrequency({range.lower, range.upper}, m_fullLowerMHz, m_fullUpperMHz,
                    key->key() == Qt::Key_Left ? -0.1 : 0.1);
                if (m_displayState->coupled) m_displayState->setView(view);
                m_plot->xAxis->setRange(view.lower, view.upper);
                m_viewDirty = true;
                updatePlot();
                m_plot->replot(QCustomPlot::rpQueuedReplot);
                key->accept();
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void Intel_RTL_analyzer::updateCursorReadout()
{
    m_selectedValid = false;
    if (!m_cursorOverlay || !m_cursorReadout) return;
    const QRectF area = m_plot->axisRect()->rect();
    if (!m_cursorInside || !area.contains(m_cursorPosition)) {
        m_cursorOverlay->clearCross();
        m_cursorReadout->setText("Maus auf Messpunkt | Klick ins Diagramm, dann Pfeil links/rechts zum Verschieben");
        return;
    }
    const auto xRange = m_plot->xAxis->range();
    const auto yRange = m_plot->yAxis->range();
    SpectrumPick best;
    int chosen = -1;
    for (int graph = 0; graph < 2; ++graph) {
        if (m_cursorSource->currentIndex() == 1 && graph != 0) continue;
        if (m_cursorSource->currentIndex() == 2 && graph != 1) continue;
        if (graph == 1 && !m_peakHoldCheck->isChecked()) continue;
        const auto& x = m_displayFrequencies[graph];
        const auto& y = m_displayLevels[graph];
        const auto pick = pickSpectrumPoint(x.constData(), y.constData(),
            static_cast<std::size_t>(x.size()), {xRange.lower, xRange.upper},
            yRange.lower, yRange.upper, area.width(), area.height(),
            m_cursorPosition.x() - area.left(), m_cursorPosition.y() - area.top());
        if (pick.valid && pick.distanceSquared < best.distanceSquared) {
            best = pick; chosen = graph;
        }
    }
    if (chosen < 0) {
        m_cursorOverlay->clearCross();
        m_cursorReadout->setText("Keine Messpunkte im sichtbaren Bereich");
        return;
    }
    const double frequency = m_displayFrequencies[chosen].at(static_cast<int>(best.index));
    const double level = m_displayLevels[chosen].at(static_cast<int>(best.index));
    m_selectedFrequency = frequency; m_selectedLevel = level; m_selectedValid = true;
    const QPointF point(m_plot->xAxis->coordToPixel(frequency), m_plot->yAxis->coordToPixel(level));
    m_cursorOverlay->setCross(area, point, chosen == 0 ? QColor(0, 90, 220) : QColor(170, 120, 0));
    m_cursorReadout->setText(QString("F: %1 MHz | %2: %3 %4")
        .arg(frequency, 0, 'f', 6)
        .arg(chosen == 0 ? "Live (geglaettet)" : "Peak Hold")
        .arg(level, 0, 'f', 2).arg(m_displayState->levelUnit()));
}

void Intel_RTL_analyzer::refreshMarkers()
{
    QVector<QPointF> points;
    QString text;
    for (int i = 0; i < 2; ++i) {
        if (m_markerValid[i]) {
            points.append(QPointF(m_plot->xAxis->coordToPixel(m_markerFrequency[i]),
                                 m_plot->yAxis->coordToPixel(m_markerLevel[i])));
            text += QString("%1: %2 MHz / %3 %4   ").arg(i == 0 ? "A" : "B")
                .arg(m_markerFrequency[i], 0, 'f', 6).arg(m_markerLevel[i], 0, 'f', 2)
                .arg(m_displayState->levelUnit());
        } else points.append(QPointF(-1e6, -1e6));
    }
    if (m_markerValid[0] && m_markerValid[1])
        text += QString("| B-A: %1 kHz / %2 dB")
            .arg((m_markerFrequency[1] - m_markerFrequency[0]) * 1000, 0, 'f', 3)
            .arg(m_markerLevel[1] - m_markerLevel[0], 0, 'f', 2);
    m_markerReadout->setText(text.isEmpty() ? "Marker: A/B auswaehlen und Messpunkt anklicken" : text);
    m_cursorOverlay->setArea(m_plot->axisRect()->rect());
    m_cursorOverlay->setMarkers(points);
}
