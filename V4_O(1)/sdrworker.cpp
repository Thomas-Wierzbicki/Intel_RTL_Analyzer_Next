#include "sdrworker.h"
#include <cmath>
#include <QDebug>

#include <QtCore/QVariant>

extern "C" {
#include "rtl-sdr.h"
}

SDRWorker::SDRWorker(QObject* parent)
    : QThread(parent), m_isRunning(true), m_rtldev(nullptr),
    m_fftOrder(12), m_centerFrequency(433500000), m_sampleRate(2400000)
{
    m_fftSize = 1 << m_fftOrder;
    m_pRawUsbBuffer = new unsigned char[USB_BUFFER_SIZE];
    initIntelIPP();

    m_sharedFrequencies.resize(m_fftSize);
    m_sharedAmplitudes.resize(m_fftSize);
}

SDRWorker::~SDRWorker()
{
    stop();
    delete[] m_pRawUsbBuffer;

    if (m_pSpecMem)    ippsFree(m_pSpecMem);
    if (m_pWorkBuffer) ippsFree(m_pWorkBuffer);
    if (m_pSrcReal)    ippsFree(m_pSrcReal);
    if (m_pSrcImag)    ippsFree(m_pSrcImag);
    if (m_pDstReal)    ippsFree(m_pDstReal);
    if (m_pDstImag)    ippsFree(m_pDstImag);
    if (m_pMagnitudes) ippsFree(m_pMagnitudes);
}

void SDRWorker::initIntelIPP()
{
    ippsFFTGetSize_C_32f(m_fftOrder, IPP_FFT_NODIV_BY_ANY, ippAlgHintNone, &m_sizeSpec, &m_sizeInit, &m_sizeBuffer);
    m_pSpecMem = ippsMalloc_8u(m_sizeSpec);
    Ipp8u* initBuf = ippsMalloc_8u(m_sizeInit);
    m_pWorkBuffer = ippsMalloc_8u(m_sizeBuffer);

    m_pSrcReal = ippsMalloc_32f(m_fftSize);
    m_pSrcImag = ippsMalloc_32f(m_fftSize);
    m_pDstReal = ippsMalloc_32f(m_fftSize);
    m_pDstImag = ippsMalloc_32f(m_fftSize);
    m_pMagnitudes = ippsMalloc_32f(m_fftSize);

    ippsFFTInit_C_32f(&m_pFFTSpec, m_fftOrder, IPP_FFT_NODIV_BY_ANY, ippAlgHintNone, m_pSpecMem, initBuf);
    if (initBuf) ippsFree(initBuf);
}

// Wird vom GUI-Thread aufgerufen, um die Daten extrem schnell abzuholen
void SDRWorker::getLatestData(QVector<double>& freqs, QVector<double>& amps)
{
    QMutexLocker locker(&m_dataMutex);
    freqs = m_sharedFrequencies;
    amps = m_sharedAmplitudes;
}

bool SDRWorker::setFrequency(uint32_t freqHz)
{
    m_centerFrequency = freqHz;
    if (m_rtldev) {
        return (rtlsdr_set_center_freq(static_cast<rtlsdr_dev_t*>(m_rtldev), m_centerFrequency) == 0);
    }
    return false;
}

void SDRWorker::stop()
{
    m_isRunning = false;
    if (m_rtldev) {
        rtlsdr_cancel_async(static_cast<rtlsdr_dev_t*>(m_rtldev));
    }
    quit();
    wait();
    closeHardware();
}

void SDRWorker::closeHardware()
{
    if (m_rtldev) {
        rtlsdr_close(static_cast<rtlsdr_dev_t*>(m_rtldev));
        m_rtldev = nullptr;
    }
}

void SDRWorker::run()
{
    rtlsdr_dev_t* dev = nullptr;
    // Wir versuchen den Stick zu öffnen, blockieren den Simulator aber nicht, falls kein Stick steckt
    bool hardwareAvailable = (rtlsdr_open(&dev, 0) == 0);
    if (hardwareAvailable) {
        m_rtldev = dev;
        rtlsdr_set_sample_rate(dev, m_sampleRate);
        rtlsdr_set_center_freq(dev, m_centerFrequency);
        rtlsdr_set_tuner_gain_mode(dev, 0);
        rtlsdr_reset_buffer(dev);
    }

    int nRead = 0;
    double simPhase = 0.0;
    double audioPhase = 0.0;

    while (m_isRunning) {
        bool simActive = this->property("simulationActive").toBool();
        double simOffsetHz = this->property("simulationOffsetHz").toDouble();

        if (simActive) {
            // --- FM DATEN SIMULATOR GENERIERUNG ---
            // Schmalband-FM hat einen maximalen Frequenzhub von 2.5 kHz für ein 12.5 kHz Kanalraster
            double maxDev = 2500.0;
            double audioFreq = 1000.0; // 1 kHz Testton als Modulationssignal

            for (int i = 0; i < m_fftSize; ++i) {
                // 1. NF-Modulationssignal berechnen (Audio-Sinuswelle)
                audioPhase += 2.0 * M_PI * audioFreq / m_sampleRate;
                if (audioPhase > 2.0 * M_PI) audioPhase -= 2.0 * M_PI;
                double audioSignal = std::sin(audioPhase);

                // 2. Momentanfrequenz berechnen (Mittenverschiebung durch Slider + FM-Hub)
                double instantaneousFreq = simOffsetHz + (audioSignal * maxDev);

                // 3. Phase des HF-Trägers akkumulieren
                simPhase += 2.0 * M_PI * instantaneousFreq / m_sampleRate;
                if (simPhase > 2.0 * M_PI) simPhase -= 2.0 * M_PI;

                // 4. Komplexe I/Q-Rohdaten simulieren (Achtung: Intel IPP erwartet normierte Amplituden)
                // Wir fügen ein künstliches Grundrauschen hinzu, damit es absolut real aussieht
                double noiseI = (static_cast<double>(std::rand()) / RAND_MAX - 0.5) * 0.05;
                double noiseQ = (static_cast<double>(std::rand()) / RAND_MAX - 0.5) * 0.05;

                m_pSrcReal[i] = static_cast<float>(std::cos(simPhase) + noiseI);
                m_pSrcImag[i] = static_cast<float>(std::sin(simPhase) + noiseQ);
            }

            // Da wir keine Hardware-USB-Verzögerung haben, drosseln wir die Schleife auf die reale Samplerate
            msleep(static_cast<unsigned long>((static_cast<double>(m_fftSize) / m_sampleRate) * 1000.0));

        }
        else if (hardwareAvailable && m_rtldev) {
            // --- ECHTER HARDWARE USB-ABRUF ---
            if (rtlsdr_read_sync(dev, m_pRawUsbBuffer, USB_BUFFER_SIZE, &nRead) == 0 && nRead > 0) {
                int availableSamples = nRead / 2;
                if (availableSamples < m_fftSize) continue;

                unsigned char* pBlock = m_pRawUsbBuffer; // Wir nehmen den ersten vollen Block
                for (int i = 0; i < m_fftSize; ++i) {
                    m_pSrcReal[i] = (static_cast<float>(pBlock[2 * i]) - 127.5f) / 127.5f;
                    m_pSrcImag[i] = (static_cast<float>(pBlock[2 * i + 1]) - 127.5f) / 127.5f;
                }
            }
            else {
                msleep(1);
                continue;
            }
        }
        else {
            // Weder Hardware noch Simulation aktiv -> Kurz warten
            msleep(10);
            continue;
        }

        // --- GEMEINSAME INTEL IPP FFT-VERARBEITUNGSPIPELINE ---
        ippsWinHann_32f_I(m_pSrcReal, m_fftSize);
        ippsWinHann_32f_I(m_pSrcImag, m_fftSize);

        ippsFFTFwd_CToC_32f(m_pSrcReal, m_pSrcImag, m_pDstReal, m_pDstImag, m_pFFTSpec, m_pWorkBuffer);
        ippsPowerSpectr_32f(m_pDstReal, m_pDstImag, m_pMagnitudes, m_fftSize);

        ippsLn_32f_I(m_pMagnitudes, m_fftSize);
        ippsMulC_32f_I(4.342944819f, m_pMagnitudes, m_fftSize);

        // Daten thread-sicher für das Spektrum und das Wasserfalldiagramm bereitstellen
        {
            QMutexLocker locker(&m_dataMutex);
            int halfSize = m_fftSize / 2;
            double hzPerBin = static_cast<double>(m_sampleRate) / m_fftSize;

            for (int i = 0; i < m_fftSize; ++i) {
                int shiftIdx = (i < halfSize) ? (i + halfSize) : (i - halfSize);
                m_sharedFrequencies[i] = m_centerFrequency + (i - halfSize) * hzPerBin;
                m_sharedAmplitudes[i] = static_cast<double>(m_pMagnitudes[shiftIdx]);
            }
        }
    }

    if (hardwareAvailable) {
        closeHardware();
    }
}