#include <Arduino.h>
#include "bpm_estimator.h"
#include <math.h>

void remove_trend(float* in, float* out, int len, int win_size) {
  for (int i = 0; i < len; i++) {
    float sum = 0;
    int count = 0;
    for (int j = i - win_size / 2; j <= i + win_size / 2; j++) {
      if (j >= 0 && j < len) {
        sum += in[j];
        count++;
      }
    }
    out[i] = in[i] - (sum / count);
  }
}

void normalize_minmax(float* in, float* out, int len) {
  float min_val = in[0], max_val = in[0];
  for (int i = 1; i < len; i++) {
    if (in[i] < min_val) min_val = in[i];
    if (in[i] > max_val) max_val = in[i];
  }
  for (int i = 0; i < len; i++) {
    out[i] = (in[i] - min_val) / (max_val - min_val);
  }
}

int find_peaks(float* signal, int len, int* peaks, float min_prom, int min_dist) {
  int count = 0;
  for (int i = 1; i < len - 1; i++) {
    if (signal[i] > signal[i - 1] && signal[i] > signal[i + 1] && signal[i] > min_prom) {
      if (count == 0 || (i - peaks[count - 1]) > min_dist) {
        peaks[count++] = i;
      }
    }
  }
  return count;
}

float estimate_bpm(int* peaks, int n_peaks, float fs) {
  if (n_peaks < 2) return NAN;
  float sum_intervals = 0;
  for (int i = 1; i < n_peaks; i++) {
    sum_intervals += (peaks[i] - peaks[i - 1]) / fs;
  }
  float avg_interval = sum_intervals / (n_peaks - 1);
  return 60.0 / avg_interval;
}


float estimate_spo2(float* ir, float* red, int len) {
  float ac_ir = 0, dc_ir = 0;
  float ac_red = 0, dc_red = 0;

  for (int i = 0; i < len; i++) {
    dc_ir += ir[i];
    dc_red += red[i];
  }
  dc_ir /= len;
  dc_red /= len;

  for (int i = 0; i < len; i++) {
    ac_ir += fabs(ir[i] - dc_ir);
    ac_red += fabs(red[i] - dc_red);
  }
  ac_ir /= len;
  ac_red /= len;

  if (ac_ir == 0 || dc_ir == 0 || ac_red == 0 || dc_red == 0) return NAN;

  float R = (ac_red / dc_red) / (ac_ir / dc_ir);
  float spo2 = 110.0 - 25.0 * R;

  if (spo2 > 100.0) spo2 = 100.0;
  if (spo2 < 70.0) spo2 = 70.0;

  return spo2;
}
