#ifndef SDRWORKER_H
#define SDRWORKER_H

#include <QtCore/QThread>
#include <QtCore/QVector>
#include <QtCore/QMutex>

#pragma warning(push)
#pragma warning(disable : 4244)
#include <ipps.h>
#pragma warning(pop)

class SDRWorker : public QThread
{
    Q_OBJECT

public:
    explicit SDRWorker(QObject* parent = nullptr);
    ~SDRWorker();

    void stop();
    bool setFrequency(uint32_t freqHz);

    // PERFORMANCE-BOOST: Das GUI-Fenster holt sich die Daten hiermit thread-sicher ab
    void getLatestData(QVector<double>& freqs, QVector<double>& amps);

protected:
    void run() override;

private:
    void initIntelIPP();
    void closeHardware();

    bool m_isRunning;
    void* m_rtldev;
    QMutex m_dataMutex; // Schützt die Daten vor gleichzeitigem Zugriff

    int m_fftOrder;
    int m_fftSize;
    uint32_t m_centerFrequency;
    uint32_t m_sampleRate;

    // Thread-sichere Puffer für die Datenübergabe
    QVector<double> m_sharedFrequencies;
    QVector<double> m_sharedAmplitudes;

    int m_sizeSpec, m_sizeInit, m_sizeBuffer;
    IppsFFTSpec_C_32f* m_pFFTSpec;
    Ipp8u* m_pSpecMem;
    Ipp8u* m_pWorkBuffer;

    Ipp32f* m_pSrcReal;
    Ipp32f* m_pSrcImag;
    Ipp32f* m_pDstReal;
    Ipp32f* m_pDstImag;
    Ipp32f* m_pMagnitudes;

    static const int USB_BUFFER_SIZE = 262144;
    unsigned char* m_pRawUsbBuffer;
};

#endif // SDRWORKER_H
