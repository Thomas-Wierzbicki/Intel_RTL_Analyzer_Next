// Declaration-only shim for CI compile checking. NEVER used by the application target.
#ifndef ANALYZER_TEST_IPPS_H
#define ANALYZER_TEST_IPPS_H
using Ipp32f = float;
using Ipp8u = unsigned char;
struct IppsFFTSpec_C_32f;
enum { ippStsNoErr = 0, IPP_FFT_NODIV_BY_ANY = 0, ippAlgHintNone = 0 };
extern "C" {
int ippsFFTGetSize_C_32f(int, int, int, int*, int*, int*);
Ipp8u* ippsMalloc_8u(int);
Ipp32f* ippsMalloc_32f(int);
void ippsFree(void*);
int ippsFFTInit_C_32f(IppsFFTSpec_C_32f**, int, int, int, Ipp8u*, Ipp8u*);
int ippsFFTFwd_CToC_32f(const Ipp32f*, const Ipp32f*, Ipp32f*, Ipp32f*,
                       const IppsFFTSpec_C_32f*, Ipp8u*);
int ippsPowerSpectr_32f(const Ipp32f*, const Ipp32f*, Ipp32f*, int);
}
#endif
