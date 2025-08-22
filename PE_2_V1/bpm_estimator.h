#ifndef BPM_ESTIMATOR_H
#define BPM_ESTIMATOR_H

void remove_trend(float* in, float* out, int len, int win_size);
void normalize_minmax(float* in, float* out, int len);
int find_peaks(float* signal, int len, int* peaks, float min_prom, int min_dist);
float estimate_bpm(int* peaks, int n_peaks, float fs);
float estimate_spo2(float* ir, float* red, int len);

uint16_t culCalcCRC(uint8_t crcData, uint16_t crcReg);
uint16_t computeCRC(const uint8_t* data, size_t length);

#endif
