#ifndef SDRWORKER_H
#define SDRWORKER_H

#include <QtCore/QThread>
#include <QtCore/QVector>

// Intel IPP DSP-Header laden
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

signals:
    void fftDataReady(const QVector<double>& frequencies, const QVector<double>& amplitudes);

protected:
    void run() override;

private:
    void initIntelIPP();
    void closeHardware();

    bool m_isRunning;
    void* m_rtldev;       // Abstrahierter Hardware-Zeiger

    int m_fftOrder;
    int m_fftSize;
    uint32_t m_centerFrequency;
    uint32_t m_sampleRate;

    // Intel IPP FFT-Strukturen
    int m_sizeSpec, m_sizeInit, m_sizeBuffer;
    IppsFFTSpec_C_32f* m_pFFTSpec;
    Ipp8u* m_pSpecMem;
    Ipp8u* m_pWorkBuffer;

    // AVX-ausgerichteter Speicher (Intel allokiert)
    Ipp32f* m_pSrcReal;
    Ipp32f* m_pSrcImag;
    Ipp32f* m_pDstReal;
    Ipp32f* m_pDstImag;
    Ipp32f* m_pMagnitudes;

    static const int USB_BUFFER_SIZE = 262144; // 256 KB
    unsigned char* m_pRawUsbBuffer;
};

#endif // SDRWORKER_H
