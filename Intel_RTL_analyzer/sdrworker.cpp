#include "sdrworker.h"
#include "iqblockassembler.h"
#include "spectrumhistory.h"
#include "measurementmath.h"
#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QElapsedTimer>
#include <cmath>
#include <mutex>
#include <deque>
#include <thread>
#include <random>
#include <chrono>
extern "C" {
#include "rtl-sdr.h"
}
namespace {
constexpr double pi = 3.14159265358979323846;
struct UsbQueue {
    std::mutex mutex;
    std::deque<QByteArray> blocks;
    std::atomic<bool> done{false};
    std::atomic<bool> enabled{true};
    std::atomic<unsigned long long> dropped{0};
    int result = 0;
};
void usbCallback(unsigned char* bytes, uint32_t length, void* context)
{
    auto* queue = static_cast<UsbQueue*>(context);
    if (!queue->enabled.load()) return;
    std::lock_guard<std::mutex> guard(queue->mutex);
    // Bounded memory. An overrun is reported and invalidates partial blocks.
    if (queue->blocks.size() >= 16) {
        queue->blocks.clear();
        ++queue->dropped;
    }
    queue->blocks.emplace_back(reinterpret_cast<const char*>(bytes), int(length));
}
}
SDRWorker::SDRWorker(QObject* parent) : QThread(parent)
{
    m_ippReady = initIntelIPP();
    m_sharedFrequencies.resize(m_fftSize);
    m_sharedAmplitudes.resize(m_fftSize);
}
SDRWorker::~SDRWorker()
{
    stop();
    if (m_pSpecMem) ippsFree(m_pSpecMem);
    if (m_pWorkBuffer) ippsFree(m_pWorkBuffer);
    if (m_pSrcReal) ippsFree(m_pSrcReal);
    if (m_pSrcImag) ippsFree(m_pSrcImag);
    if (m_pDstReal) ippsFree(m_pDstReal);
    if (m_pDstImag) ippsFree(m_pDstImag);
    if (m_pMagnitudes) ippsFree(m_pMagnitudes);
}
bool SDRWorker::initIntelIPP()
{
    int specSize = 0, initSize = 0, workSize = 0;
    if (ippsFFTGetSize_C_32f(m_fftOrder, IPP_FFT_NODIV_BY_ANY, ippAlgHintNone,
                            &specSize, &initSize, &workSize) != ippStsNoErr) return false;
    m_pSpecMem = ippsMalloc_8u(specSize);
    Ipp8u* init = initSize ? ippsMalloc_8u(initSize) : nullptr;
    m_pWorkBuffer = workSize ? ippsMalloc_8u(workSize) : nullptr;
    m_pSrcReal = ippsMalloc_32f(m_fftSize);
    m_pSrcImag = ippsMalloc_32f(m_fftSize);
    m_pDstReal = ippsMalloc_32f(m_fftSize);
    m_pDstImag = ippsMalloc_32f(m_fftSize);
    m_pMagnitudes = ippsMalloc_32f(m_fftSize);
    if (!m_pSpecMem || (initSize && !init) || (workSize && !m_pWorkBuffer) ||
        !m_pSrcReal || !m_pSrcImag || !m_pDstReal || !m_pDstImag || !m_pMagnitudes) {
        if (init) ippsFree(init);
        return false;
    }
    const auto status = ippsFFTInit_C_32f(&m_pFFTSpec, m_fftOrder, IPP_FFT_NODIV_BY_ANY,
                                         ippAlgHintNone, m_pSpecMem, init);
    if (init) ippsFree(init);
    if (status != ippStsNoErr) return false;
    m_window.resize(m_fftSize);
    for (int i = 0; i < m_fftSize; ++i) {
        m_window[i] = float(0.5 - 0.5 * std::cos(2 * pi * i / (m_fftSize - 1)));
        m_windowSum += m_window[i];
    }
    return true;
}
void SDRWorker::getLatestData(QVector<double>& f, QVector<double>& a)
{
    QMutexLocker guard(&m_dataMutex);
    f = m_sharedFrequencies; a = m_sharedAmplitudes;
}
bool SDRWorker::getSnapshot(QVector<double>& f, QVector<double>& a,
                            QVector<double>& p, quint64& sequence, quint64& epoch)
{
    std::unique_lock<QMutex> guard(m_dataMutex, std::try_to_lock);
    if (!guard.owns_lock()) return false;
    f = m_sharedFrequencies; a = m_sharedAmplitudes; p = m_sharedPeaks;
    sequence = m_sequence; epoch = m_epoch;
    return true;
}
bool SDRWorker::takeWaterfall(QVector<double>& f, QVector<double>& maxima,
                             qint64& timestampMs, quint64& epoch)
{
    std::unique_lock<QMutex> guard(m_dataMutex, std::try_to_lock);
    if (!guard.owns_lock() || m_waterfallMaxima.isEmpty()) return false;
    f = m_sharedFrequencies;
    maxima.swap(m_waterfallMaxima);
    m_waterfallMaxima.clear();
    timestampMs = m_waterfallTimestamp; epoch = m_epoch;
    return true;
}
void SDRWorker::discardWaterfall()
{
    QMutexLocker guard(&m_dataMutex); m_waterfallMaxima.clear();
}
void SDRWorker::resetPeakHold()
{
    QMutexLocker guard(&m_dataMutex); m_sharedPeaks.clear();
}
bool SDRWorker::setFrequency(uint32_t hz)
{
    if (hz < m_sampleRate / 2) return false;
    m_centerFrequency.store(hz); return true; // Applied by acquisition thread.
}
void SDRWorker::stop()
{
    m_isRunning.store(false);
    requestInterruption();
    if (QThread::currentThread() != this) wait();
}
QString SDRWorker::statusText()
{
    QMutexLocker guard(&m_dataMutex); return m_status;
}
void SDRWorker::setStatus(const QString& text)
{
    {
        QMutexLocker guard(&m_dataMutex);
        if (m_status == text) return;
        m_status = text;
    }
    emit statusChanged(text);
}
void SDRWorker::run()
{
    if (!m_ippReady) {
        setStatus("Fehler: Intel IPP konnte nicht initialisiert werden");
        return;
    }
    rtlsdr_dev_t* dev = nullptr;
    bool hardware = rtlsdr_open(&dev, 0) == 0;
    uint32_t appliedFrequency = m_centerFrequency.load();
    if (hardware && (rtlsdr_set_sample_rate(dev, m_sampleRate) != 0 ||
                     rtlsdr_set_center_freq(dev, appliedFrequency) != 0 ||
                     rtlsdr_set_tuner_gain_mode(dev, 0) != 0 ||
                     rtlsdr_reset_buffer(dev) != 0)) {
        rtlsdr_close(dev); dev = nullptr; hardware = false;
        setStatus("Fehler bei RTL-SDR-Konfiguration; Simulator verfuegbar");
    } else {
        setStatus(hardware ? "RTL-SDR aktiv | Tuner-AGC | Pegel unkalibriert" :
                            "Kein RTL-SDR gefunden | Simulator verfuegbar");
    }
    UsbQueue usb;
    std::thread reader;
    if (hardware) reader = std::thread([&]() {
        // This API can be cancelled, unlike read_sync.
        usb.result = rtlsdr_read_async(dev, usbCallback, &usb, 0, 262144);
        usb.done.store(true);
    });
    IQBlockAssembler assembler(static_cast<std::size_t>(m_fftSize));
    bool previousSimulation = false;
    unsigned long long lastDrops = 0;
    double carrierPhase = 0, audioPhase = 0;
    std::mt19937 random(0x52544c);
    std::uniform_real_distribution<double> noise(-0.025, 0.025);
    QElapsedTimer simulationClock;
    simulationClock.start();
    qint64 simulatedSamples = 0;
    while (m_isRunning.load()) {
        const bool simulation = m_simulationActive.load();
        const bool retune = appliedFrequency != m_centerFrequency.load();
        if (simulation != previousSimulation || retune) {
            assembler.reset(); m_sourceChanged = true;
            previousSimulation = simulation;
            usb.enabled.store(!simulation);
            { std::lock_guard<std::mutex> guard(usb.mutex); usb.blocks.clear(); }
            if (retune) {
                const uint32_t requested = m_centerFrequency.load();
                if (hardware && !usb.done.load() && rtlsdr_set_center_freq(dev, requested) != 0)
                    setStatus("Fehler beim Einstellen der Empfangsfrequenz");
                appliedFrequency = requested;
            }
            simulationClock.restart(); simulatedSamples = 0;
            setStatus(simulation ? "FM-Simulator aktiv" :
                (hardware && !usb.done.load() ? "RTL-SDR aktiv | Tuner-AGC | Pegel unkalibriert" :
                                               "Kein aktiver RTL-SDR | Simulator verfuegbar"));
        }
        if (simulation) {
            const double offset = m_simulationOffset.load();
            for (int i = 0; i < m_fftSize; ++i) {
                audioPhase = std::remainder(audioPhase + 2 * pi * 1000 / m_sampleRate, 2 * pi);
                carrierPhase = std::remainder(carrierPhase +
                    2 * pi * (offset + 2500 * std::sin(audioPhase)) / m_sampleRate, 2 * pi);
                m_pSrcReal[i] = float(0.8 * std::cos(carrierPhase) + noise(random));
                m_pSrcImag[i] = float(0.8 * std::sin(carrierPhase) + noise(random));
            }
            processFftBlock();
            simulatedSamples += m_fftSize;
            const qint64 remainingNs = simulatedSamples * 1000000000LL / m_sampleRate -
                                       simulationClock.nsecsElapsed();
            if (remainingNs > 0) usleep(static_cast<unsigned long>(remainingNs / 1000));
        } else if (hardware) {
            if (usb.done.load()) {
                setStatus(QString("RTL-SDR-Lesefehler/Stream beendet (Code %1); Simulator verfuegbar")
                          .arg(usb.result));
                msleep(10); continue;
            }
            QByteArray bytes;
            {
                std::lock_guard<std::mutex> guard(usb.mutex);
                const auto drops = usb.dropped.load();
                if (drops != lastDrops) {
                    assembler.reset(); m_sourceChanged = true; lastDrops = drops;
                }
                if (!usb.blocks.empty()) {
                    bytes = std::move(usb.blocks.front()); usb.blocks.pop_front();
                }
            }
            if (lastDrops) setStatus(QString("RTL-SDR aktiv | USB-Pufferueberlaeufe: %1")
                                     .arg(qulonglong(lastDrops)));
            if (bytes.isEmpty()) { msleep(1); continue; }
            assembler.append(reinterpret_cast<const unsigned char*>(bytes.constData()),
                std::size_t(bytes.size()), [this](const unsigned char* block) {
                    if (!m_isRunning.load()) return;
                    for (int i = 0; i < m_fftSize; ++i) {
                        m_pSrcReal[i] = (float(block[2*i]) - 127.5f) / 127.5f;
                        m_pSrcImag[i] = (float(block[2*i+1]) - 127.5f) / 127.5f;
                    }
                    processFftBlock();
                });
        } else msleep(10);
    }
    // Repeat cancellation to cover stop arriving during async stream startup.
    while (reader.joinable() && !usb.done.load()) {
        rtlsdr_cancel_async(dev);
        msleep(10);
    }
    if (reader.joinable()) reader.join();
    if (dev) rtlsdr_close(dev); // Only close AFTER all callbacks have ended.
    setStatus("Empfang beendet");
}
void SDRWorker::processFftBlock()
{
    for (int i = 0; i < m_fftSize; ++i) {
        m_pSrcReal[i] *= m_window[i]; m_pSrcImag[i] *= m_window[i];
    }
    if (ippsFFTFwd_CToC_32f(m_pSrcReal, m_pSrcImag, m_pDstReal, m_pDstImag,
                           m_pFFTSpec, m_pWorkBuffer) != ippStsNoErr ||
        ippsPowerSpectr_32f(m_pDstReal, m_pDstImag, m_pMagnitudes, m_fftSize) != ippStsNoErr) {
        setStatus("Fehler bei Intel-IPP-FFT"); return;
    }
    QMutexLocker guard(&m_dataMutex);
    if (m_sourceChanged) {
        m_sharedPeaks.clear(); m_waterfallMaxima.clear();
        ++m_epoch; m_sourceChanged = false;
    }
    double* f = m_sharedFrequencies.data();
    double* a = m_sharedAmplitudes.data();
    const double center = m_centerFrequency.load();
    for (int i = 0; i < m_fftSize; ++i) {
        const int shifted = (i + m_fftSize / 2) % m_fftSize;
        f[i] = center + (i - m_fftSize / 2) * binWidthHz();
        a[i] = normalizedToneDb(m_pMagnitudes[shifted], m_windowSum);
    }
    accumulatePeakHold(m_sharedPeaks, m_sharedAmplitudes);
    accumulatePeakHold(m_waterfallMaxima, m_sharedAmplitudes);
    m_waterfallTimestamp = QDateTime::currentMSecsSinceEpoch();
    ++m_sequence;
}
