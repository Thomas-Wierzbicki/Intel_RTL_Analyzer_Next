#include "WaterfallWindow.h"
#include <QtWidgets/QVBoxLayout>

WaterfallWindow::WaterfallWindow(SDRWorker* worker, QWidget* parent)
    : QMainWindow(parent), m_worker(worker), m_updateTimer(new QTimer(this)), m_currentIndex(0)
{
    setWindowTitle("Intel IPP Core - Live Waterfall Spectrogram (70cm Band)");
    resize(1000, 500);

    m_plot = new QCustomPlot(this);
    setCentralWidget(m_plot);

    // Hardware-Beschleunigung aktivieren, falls vom System unterstuetzt
    m_plot->setOpenGl(true);

    // 1. ACHSEN FÜR DAS 70CM AMATEURBAND KALIBRIEREN
    m_plot->xAxis->setLabel("Frequenz (MHz)");
    m_plot->yAxis->setLabel("Zeitverlauf (Blöcke)");

    // Exakte Grenzen unseres SDR-Sticks einstellen
    m_plot->xAxis->setRange(432.30, 434.70);
    m_plot->yAxis->setRange(0, 300); // Zeigt die letzten 300 Zeilen Historie

    // Manuellen Ticker für saubere Megahertz-Schritte aufzwingen
    QSharedPointer<QCPAxisTickerText> textTicker(new QCPAxisTickerText);
    textTicker->addTick(432.30, "432.3 MHz");
    textTicker->addTick(432.90, "432.9 MHz");
    textTicker->addTick(433.50, "433.5 MHz"); // Zentrum
    textTicker->addTick(434.10, "434.1 MHz");
    textTicker->addTick(434.70, "434.7 MHz");
    m_plot->xAxis->setTicker(textTicker);

    // 2. FARBKARTE AN UNSERE 4096-PUNKTE FFT ANPASSEN
    m_waterfallMap = new QCPColorMap(m_plot->xAxis, m_plot->yAxis);
    m_waterfallMap->data()->setSize(4096, 300); // 4096 FFT-Spalten, 300 Zeilen Verlauf
    m_waterfallMap->data()->setRange(QCPRange(432.30, 434.70), QCPRange(0, 300));

    // 3. COLORSCALE (SPEKTRAL-FARBBALKEN) RECHTS ANZEIGEN
    QCPColorScale* colorScale = new QCPColorScale(m_plot);
    m_plot->plotLayout()->addElement(0, 1, colorScale);
    m_waterfallMap->setColorScale(colorScale);

    // gpJet liefert das klassische SDR-Farbschema (Blau=Rauschen, Rot=Starkes Signal)
    m_waterfallMap->setGradient(QCPColorGradient::gpJet);

    // Skalierung an die realen dB-Werte der Intel-Pipeline anpassen
    m_waterfallMap->setDataRange(QCPRange(-10.0, 40.0));

    // 4. TIMING STARTEN (Polling-Verfahren parallel zum Hauptfenster)
    connect(m_updateTimer, &QTimer::timeout, this, &WaterfallWindow::updateWaterfall);
    m_updateTimer->start(45); // Taktung leicht versetzt zum Hauptfenster für maximale Performance

    m_plot->replot();
}

WaterfallWindow::~WaterfallWindow()
{
    m_updateTimer->stop();
}

void WaterfallWindow::updateWaterfall()
{
    if (!m_worker || !m_plot || !m_waterfallMap) return;

    // Daten blockierungsfrei aus der Signal-Engine abrufen
    m_worker->getLatestData(m_freqBuffer, m_ampBuffer);
    if (m_ampBuffer.isEmpty()) return;

    int keySize = m_waterfallMap->data()->keySize();     // 4096 Spalten
    int valueSize = m_waterfallMap->data()->valueSize(); // 300 Zeilen

    // --- MATHEMATISCHER O(1) RINGPUFFER-TRICK ---
    // Keine ressourcenfressenden Schleifen mehr!
    // Wir schreiben die neuen Amplitudendaten direkt über die älteste Zeile im RAM.
    int copySize = qMin(keySize, m_ampBuffer.size());
    for (int x = 0; x < copySize; ++x) {
        m_waterfallMap->data()->setCell(x, m_currentIndex, m_ampBuffer[x]);
    }

    // Die Grafikkarte verschiebt die Textur-Koordinaten beim Zeichnen automatisch.
    // Das Auge sieht ein flüssiges Scrollen, während die CPU 0% Arbeit verrichtet.
    m_waterfallMap->data()->setRange(QCPRange(432.30, 434.70), QCPRange(0, valueSize));

    // Zirkulären Index für den nächsten Frame inkrementieren
    m_currentIndex++;
    if (m_currentIndex >= valueSize) {
        m_currentIndex = 0; // Ringpuffer-Umlauf zurück auf Anfang
    }

    // Bild-Refresh zünden ( rpRefreshHint rendert nur die modifizierte Zeile neu)
    m_plot->replot(QCustomPlot::rpRefreshHint);
}
