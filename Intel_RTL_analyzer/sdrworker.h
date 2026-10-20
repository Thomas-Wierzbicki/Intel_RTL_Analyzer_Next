#ifndef SDRWORKER_H
#define SDRWORKER_H
#include <QtCore/QThread>
#include <QtCore/QVector>
#include <QtCore/QMutex>
#include <QtCore/QString>
#include <atomic>
#include <cstdint>
#include <vector>
#include <ipps.h>

class SDRWorker : public QThread
{
    Q_OBJECT
public:
    explicit SDRWorker(QObject* parent = nullptr);
    ~SDRWorker();
    void stop();
    bool setFrequency(uint32_t freqHz);
    void getLatestData(QVector<double>& freqs, QVector<double>& amps);
    bool getSnapshot(QVector<double>& freqs, QVector<double>& amps,
                     QVector<double>& peaks, quint64& sequence, quint64& epoch);
    // A single consumer drains maxima over ALL FFTs since its previous drain.
    bool takeWaterfall(QVector<double>& freqs, QVector<double>& maxima,
                       qint64& timestampMs, quint64& epoch);
    void discardWaterfall();
    void resetPeakHold();
    void setSimulationActive(bool active) { m_simulationActive.store(active); }
    void setSimulationOffset(double hz) { m_simulationOffset.store(hz); }
    QString statusText();
    double binWidthHz() const { return double(m_sampleRate) / m_fftSize; }
    double fullLowerMHz() const { return (m_centerFrequency.load() - m_sampleRate / 2.0) / 1e6; }
    double fullUpperMHz() const { return (m_centerFrequency.load() + m_sampleRate / 2.0) / 1e6; }
signals:
    void statusChanged(const QString& text);
protected:
    void run() override;
private:
    bool initIntelIPP();
    void processFftBlock();
    void setStatus(const QString& text);
    std::atomic<bool> m_isRunning{true};
    std::atomic<bool> m_simulationActive{false};
    std::atomic<double> m_simulationOffset{0.0};
    std::atomic<uint32_t> m_centerFrequency{433500000};
    QMutex m_dataMutex;
    QString m_status = "Empfang wird initialisiert";
    int m_fftOrder = 12;
    int m_fftSize = 4096;
    uint32_t m_sampleRate = 2400000;
    QVector<double> m_sharedFrequencies, m_sharedAmplitudes, m_sharedPeaks;
    QVector<double> m_waterfallMaxima;
    qint64 m_waterfallTimestamp = 0;
    quint64 m_sequence = 0, m_epoch = 0;
    bool m_sourceChanged = false;
    bool m_ippReady = false;
    std::vector<float> m_window;
    double m_windowSum = 0.0;
    IppsFFTSpec_C_32f* m_pFFTSpec = nullptr;
    Ipp8u* m_pSpecMem = nullptr;
    Ipp8u* m_pWorkBuffer = nullptr;
    Ipp32f* m_pSrcReal = nullptr;
    Ipp32f* m_pSrcImag = nullptr;
    Ipp32f* m_pDstReal = nullptr;
    Ipp32f* m_pDstImag = nullptr;
    Ipp32f* m_pMagnitudes = nullptr;
};
#endif
