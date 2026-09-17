#include <Wire.h>

const uint8_t ADDR_OUT   = 0x77;    // OUTSIDE (CSB -> GND)
const uint8_t ADDR_IN    = 0x76;    // INSIDE  (CSB -> VCC)
const int     AVG        = 5;       // reads averaged per row
const unsigned long ZERO_MS = 5000; // zeroing window (ms)

uint16_t C_out[7], C_in[7];      // calibration, index 1..6
float    offsetDiff = 0.0;       // fan-OFF baseline of (Pin - Pout), Pa
uint32_t t0 = 0;                 // time origin (ms); reset on every zeroing

// ---- low-level MS5607 access ----
void msReset(uint8_t addr) {
  Wire.beginTransmission(addr); Wire.write(0x1E); Wire.endTransmission(); delay(5);
}
uint16_t msPromRead(uint8_t addr, uint8_t idx) {
  Wire.beginTransmission(addr); Wire.write(0xA0 + idx * 2); Wire.endTransmission();
  Wire.requestFrom(addr, (uint8_t)2);
  uint16_t hi = Wire.read(), lo = Wire.read();
  return (hi << 8) | lo;
}
uint32_t msADC(uint8_t addr, uint8_t cmd) {         // 0x48=D1(P), 0x58=D2(T), OSR4096
  Wire.beginTransmission(addr); Wire.write(cmd); Wire.endTransmission();
  delay(10);                                        // OSR4096 conversion ~9 ms
  Wire.beginTransmission(addr); Wire.write(0x00); Wire.endTransmission();
  Wire.requestFrom(addr, (uint8_t)3);
  uint32_t b2 = Wire.read(), b1 = Wire.read(), b0 = Wire.read();
  return (b2 << 16) | (b1 << 8) | b0;
}
void loadPROM(uint8_t addr, uint16_t *C) {
  msReset(addr);
  for (uint8_t i = 1; i <= 6; i++) C[i] = msPromRead(addr, i);
}

// ---- single MS5607 compensated read (first-order; temps here >20C, no 2nd-order needed) ----
void readMS5607(uint8_t addr, uint16_t *C, float &pPa, float &tC) {
  uint32_t D1 = msADC(addr, 0x48);   // pressure
  uint32_t D2 = msADC(addr, 0x58);   // temperature
  int64_t dT   = (int64_t)D2 - ((int64_t)C[5] << 8);
  int64_t TEMP = 2000 + (dT * (int64_t)C[6]) / 8388608LL;              // /2^23
  int64_t OFF  = ((int64_t)C[2] << 17) + ((int64_t)C[4] * dT) / 64;    // MS5607: 2^17, /2^6
  int64_t SENS = ((int64_t)C[1] << 16) + ((int64_t)C[3] * dT) / 128;   // MS5607: 2^16, /2^7
  int64_t P    = ((((int64_t)D1 * SENS) >> 21) - OFF) >> 15;
  pPa = (float)P;
  tC  = TEMP / 100.0;
}

// ---- averaged read: mean of n full reads ----
void readAvg(uint8_t addr, uint16_t *C, int n, float &pPa, float &tC) {
  double accP = 0, accT = 0;
  float p, t;
  for (int i = 0; i < n; i++) {
    readMS5607(addr, C, p, t);
    accP += p; accT += t;
  }
  pPa = accP / n;
  tC  = accT / n;
}

// ---- zeroing: average the (Pin - Pout) difference over a time window ----
void autoZero(unsigned long durationMs) {
  Serial.println(F("# Zeroing over 5 s... keep the FAN OFF"));
  double acc = 0; long count = 0;
  float pIn, pOut, tIn, tOut;
  unsigned long start = millis();
  while (millis() - start < durationMs) {
    readAvg(ADDR_IN,  C_in,  AVG, pIn,  tIn);
    readAvg(ADDR_OUT, C_out, AVG, pOut, tOut);
    acc += (pIn - pOut);
    count++;
  }
  offsetDiff = (count > 0) ? (acc / count) : 0.0;
  Serial.print(F("# offset_Pa = ")); Serial.print(offsetDiff, 2);
  Serial.print(F("  (samples: ")); Serial.print(count); Serial.println(F(")"));
  t0 = millis();                     // restart the clock at zero
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);

  loadPROM(ADDR_OUT, C_out);
  loadPROM(ADDR_IN,  C_in);

  autoZero(ZERO_MS);   // 5 s average; keep fan OFF during this

  Serial.println(F("time_s,diff_zeroed_Pa,P_in_Pa,P_out_Pa,T_in_C,T_out_C"));
}

void loop() {
  if (Serial.available()) { char c = Serial.read(); if (c == 'z' || c == 'Z') autoZero(ZERO_MS); }

  float t_s = (millis() - t0) / 1000.0;   // seconds since last zeroing
  float pIn, pOut, tIn, tOut;
  readAvg(ADDR_IN,  C_in,  AVG, pIn,  tIn);
  readAvg(ADDR_OUT, C_out, AVG, pOut, tOut);
  float zeroed = (pIn - pOut) - offsetDiff;

  Serial.print(t_s, 3);    Serial.print(',');   // 1: seconds since zeroing
  Serial.print(zeroed, 2); Serial.print(',');   // 2: zeroed differential (fan effect)
  Serial.print(pIn, 2);    Serial.print(',');   // 3: inside avg
  Serial.print(pOut, 2);   Serial.print(',');   // 4: outside avg
  Serial.print(tIn, 2);    Serial.print(',');   // 5: inside temp
  Serial.println(tOut, 2);                        // 6: outside temp
}
