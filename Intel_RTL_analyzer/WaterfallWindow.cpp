#include "WaterfallWindow.h"
#include "frequencyzoom.h"
#include "measurementmath.h"
#include <QtGui/QKeyEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QLinearGradient>
#include <QtGui/QAction>
#include <QtCore/QDateTime>
#include <QtWidgets/QToolBar>
#include <array>
#include <vector>
#include <limits>
#include <algorithm>

class WaterfallCanvas : public QWidget
{
public:
    explicit WaterfallCanvas(DisplayState* state, QWidget* parent = nullptr)
        : QWidget(parent), m_state(state)
    {
        setMinimumSize(640, 320);
        setMouseTracking(true); setFocusPolicy(Qt::StrongFocus);
        setAttribute(Qt::WA_OpaquePaintEvent);
        setToolTip("Mausrad: Zoom | Doppelklick: Gesamtbereich | Pfeile: Verschieben | Pause im Werkzeugbalken");
        for (int i = 0; i < 256; ++i) {
            const double t = i / 255.0;
            auto channel = [](double x) {
                return int(255 * (std::max)(0.0, (std::min)(1.0, x)));
            };
            m_colors[i] = qRgb(channel(1.5-std::abs(4*t-3)),
                               channel(1.5-std::abs(4*t-2)),
                               channel(1.5-std::abs(4*t-1)));
        }
        connect(state, &DisplayState::viewChanged, this, [this]() {
            if (m_state->coupled) { m_view = m_state->view; invalidate(); }
        });
        connect(state, &DisplayState::scaleChanged, this, [this]() { invalidate(); });
        connect(state, &DisplayState::pauseChanged, this, [this](bool) { update(); });
    }
    void clear()
    {
        m_count = 0; m_head = 0;
        std::fill(m_values.begin(), m_values.end(), std::numeric_limits<float>::quiet_NaN());
        m_stamps.fill(0); invalidate();
    }
    void append(const QVector<double>& frequencies, const QVector<double>& amplitudes, qint64 stamp)
    {
        if (amplitudes.isEmpty() || frequencies.size() != amplitudes.size()) return;
        const int bins = int(amplitudes.size());
        const double step = bins > 1 ? frequencies[1] - frequencies[0] : 1;
        const double lower = frequencies.first() / 1e6;
        const double upper = (frequencies.last() + step) / 1e6;
        if (bins != m_bins || lower != m_lower || upper != m_upper) {
            m_bins = bins; m_lower = lower; m_upper = upper;
            m_values.assign(std::size_t(bins) * rows, std::numeric_limits<float>::quiet_NaN());
            clear(); m_view = {lower, upper}; m_state->setBounds(lower, upper);
            if (m_state->coupled) m_view = m_state->view;
        }
        m_head = (m_head + rows - 1) % rows;
        m_count = (std::min)(rows, m_count + 1);
        float* target = m_values.data() + std::size_t(m_head) * m_bins;
        for (int i = 0; i < m_bins; ++i) target[i] = float(amplitudes[i]);
        m_stamps[m_head] = stamp;
        if (!m_dirty && !m_image.isNull()) rasterizeRow(m_head);
        update();
    }
protected:
    void mouseMoveEvent(QMouseEvent* e) override { m_cursor = e->position(); m_inside = true; update(); }
    void mousePressEvent(QMouseEvent* e) override {
        setFocus(Qt::MouseFocusReason); m_cursor = e->position(); m_inside = true; update();
    }
    void leaveEvent(QEvent*) override { m_inside = false; update(); }
    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() != Qt::Key_Left && e->key() != Qt::Key_Right) { QWidget::keyPressEvent(e); return; }
        applyView(panFrequency(m_view, m_lower, m_upper, e->key() == Qt::Key_Left ? -0.1 : 0.1));
        e->accept();
    }
    void wheelEvent(QWheelEvent* e) override {
        const auto area = plotRect();
        if (!area.contains(e->position())) { e->ignore(); return; }
        const double steps = e->angleDelta().y() ? e->angleDelta().y()/120.0 : e->pixelDelta().y()/40.0;
        applyView(zoomFrequency(m_view, m_lower, m_upper,
            (e->position().x()-area.left())/area.width(), steps));
        e->accept();
    }
    void mouseDoubleClickEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && plotRect().contains(e->position())) {
            applyView({m_lower, m_upper}); e->accept();
        } else QWidget::mouseDoubleClickEvent(e);
    }
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), Qt::white); p.setPen(Qt::black);
        const QRectF plot = plotRect();
        ensureRaster(int(plot.width()));
        if (!m_image.isNull()) {
            const int firstRows = rows - m_head;
            const double splitHeight = plot.height()*firstRows/rows;
            p.drawImage(QRectF(plot.left(), plot.top(), plot.width(), splitHeight),
                        m_image, QRectF(0, m_head, m_image.width(), firstRows));
            if (m_head)
                p.drawImage(QRectF(plot.left(), plot.top()+splitHeight, plot.width(), plot.height()-splitHeight),
                            m_image, QRectF(0, 0, m_image.width(), m_head));
        } else p.fillRect(plot, QColor::fromRgb(m_colors[0]));
        p.drawRect(plot);
        for (int i = 0; i <= 4; ++i) {
            const double x = plot.left()+plot.width()*i/4;
            const double frequency = m_view.lower+(m_view.upper-m_view.lower)*i/4;
            p.drawText(QRectF(x-48, plot.bottom()+7, 96, 20),
                       Qt::AlignHCenter, QString::number(frequency, 'f', 4));
        }
        p.drawText(QRectF(plot.left(), plot.bottom()+30, plot.width(), 22),
                   Qt::AlignHCenter, "Frequenz (MHz)");
        for (int age : {0, 50, 100, 150, 200, 250, 299}) {
            if (age >= m_count) continue;
            const int row = (m_head + age) % rows;
            const double seconds = (m_stamps[m_head] - m_stamps[row]) / 1000.0;
            const double y = plot.top() + plot.height()*(age+0.5)/rows;
            p.drawText(QRectF(20, y-10, 48, 20), Qt::AlignRight|Qt::AlignVCenter,
                       QString::number(seconds, 'f', 1));
        }
        p.save(); p.translate(15, plot.center().y()); p.rotate(-90);
        p.drawText(QRectF(-plot.height()/2, -12, plot.height(), 24), Qt::AlignCenter, "Alter (s)");
        p.restore();
        QString text = m_count ? QString("Neueste Zeile: %1 | %2")
            .arg(QDateTime::fromMSecsSinceEpoch(m_stamps[m_head]).toString("HH:mm:ss.zzz"))
            .arg(m_state->paused ? "PAUSE" : "Maximum seit letzter Anzeige") : "Warte auf Messdaten";
        if (m_inside && plot.contains(m_cursor) && m_count && !m_image.isNull()) {
            const int age = (std::min)(rows-1, int((m_cursor.y()-plot.top())/plot.height()*rows));
            if (age < m_count) {
                const int row = (m_head+age)%rows;
                const int x = (std::min)(m_image.width()-1, int((m_cursor.x()-plot.left())/plot.width()*m_image.width()));
                const auto peak = pickPixel(row, x);
                const double frequency = m_lower + peak.bin*(m_upper-m_lower)/m_bins;
                const double seconds = (m_stamps[m_head]-m_stamps[row])/1000.0;
                text = QString("F: %1 MHz | %2 %3 | Alter: %4 s | %5")
                    .arg(frequency, 0, 'f', 6).arg(peak.db + m_state->offsetDb, 0, 'f', 2)
                    .arg(m_state->levelUnit()).arg(seconds, 0, 'f', 3)
                    .arg(QDateTime::fromMSecsSinceEpoch(m_stamps[row]).toString("HH:mm:ss.zzz"));
                p.save(); p.setClipRect(plot);
                QPen pen(Qt::white); pen.setStyle(Qt::DashLine); pen.setWidth(0); p.setPen(pen);
                p.drawLine(QPointF(m_cursor.x(), plot.top()), QPointF(m_cursor.x(), plot.bottom()));
                p.drawLine(QPointF(plot.left(), m_cursor.y()), QPointF(plot.right(), m_cursor.y()));
                p.restore();
            }
        }
        p.drawText(QRectF(plot.left(), 3, plot.width()+50, 22), Qt::AlignLeft, text);
        const QRectF scale(plot.right()+20, plot.top(), 18, plot.height());
        QLinearGradient gradient(scale.bottomLeft(), scale.topLeft());
        for (int i = 0; i < 256; ++i) gradient.setColorAt(i/255.0, QColor::fromRgb(m_colors[i]));
        p.fillRect(scale, gradient); p.drawRect(scale);
        for (int i = 0; i <= 5; ++i) {
            const double db = m_state->minimumDb + m_state->rangeDb*i/5;
            p.drawText(QRectF(scale.right()+4, scale.bottom()-scale.height()*i/5-10, 55, 20),
                       Qt::AlignLeft|Qt::AlignVCenter, QString::number(db, 'f', 0));
        }
        p.drawText(QRectF(scale.left()-8, plot.bottom()+30, 85, 22), Qt::AlignLeft, m_state->levelUnit());
    }
private:
    static constexpr int rows = 300;
    QRectF plotRect() const { return QRectF(78, 28, width()-190, height()-90); }
    void invalidate() { m_dirty = true; update(); }
    void applyView(FrequencyView view) {
        m_view = view;
        if (m_state->coupled) m_state->setView(view);
        invalidate();
    }
    DisplayPeak pickPixel(int row, int x) const {
        const double begin = (m_view.lower-m_lower)/(m_upper-m_lower)*m_bins;
        const double span = (m_view.upper-m_view.lower)/(m_upper-m_lower)*m_bins;
        return pixelPeak(m_values.data()+std::size_t(row)*m_bins, std::size_t(m_bins),
                         begin+span*x/m_image.width(), begin+span*(x+1)/m_image.width());
    }
    void rasterizeRow(int row) {
        auto* pixels = reinterpret_cast<QRgb*>(m_image.scanLine(row));
        for (int x = 0; x < m_image.width(); ++x) {
            const double db = pickPixel(row, x).db + m_state->offsetDb;
            const double t = (db-m_state->minimumDb)/m_state->rangeDb;
            const int index = int(255*(std::max)(0.0, (std::min)(1.0, t)));
            pixels[x] = m_colors[index];
        }
    }
    void ensureRaster(int columns) {
        if (!m_bins || columns <= 0) return;
        if (m_image.width() != columns) {
            m_image = QImage(columns, rows, QImage::Format_RGB32); m_dirty = true;
        }
        if (m_dirty && !m_image.isNull()) {
            m_image.fill(m_colors[0]);
            for (int i = 0; i < m_count; ++i) rasterizeRow((m_head+i)%rows);
            m_dirty = false;
        }
    }
    DisplayState* m_state;
    FrequencyView m_view{432.30, 434.70};
    double m_lower = 432.30, m_upper = 434.70;
    int m_bins = 0, m_head = 0, m_count = 0;
    bool m_dirty = true, m_inside = false;
    QPointF m_cursor;
    QImage m_image;
    std::vector<float> m_values;
    std::array<qint64, rows> m_stamps{};
    std::array<QRgb, 256> m_colors{};
};
WaterfallWindow::WaterfallWindow(SDRWorker* worker, DisplayState* state, QWidget* parent)
    : QMainWindow(parent), m_canvas(new WaterfallCanvas(state, this)), m_worker(worker),
      m_state(state), m_updateTimer(new QTimer(this))
{
    setWindowTitle("SDR Analyzer Next - Wasserfall");
    resize(1000, 500); setCentralWidget(m_canvas);
    auto* tools = addToolBar("Ansicht");
    auto* pause = tools->addAction("Pause");
    pause->setCheckable(true); pause->setChecked(state->paused);
    connect(pause, &QAction::toggled, state, &DisplayState::setPaused);
    connect(state, &DisplayState::pauseChanged, pause, &QAction::setChecked);
    connect(state, &DisplayState::pauseChanged, this, [this](bool paused) {
        if (!paused && m_worker) m_worker->discardWaterfall();
    });
    connect(m_updateTimer, &QTimer::timeout, this, &WaterfallWindow::updateWaterfall);
    m_updateTimer->start(45);
}
WaterfallWindow::~WaterfallWindow() { m_updateTimer->stop(); }
void WaterfallWindow::updateWaterfall()
{
    if (!m_worker || m_state->paused) return;
    if (!isVisible() || isMinimized()) { m_worker->discardWaterfall(); return; }
    quint64 epoch = 0; qint64 stamp = 0;
    if (!m_worker->takeWaterfall(m_frequencies, m_maxima, stamp, epoch)) return;
    if (epoch != m_lastEpoch) { m_canvas->clear(); m_lastEpoch = epoch; }
    m_canvas->append(m_frequencies, m_maxima, stamp);
}
