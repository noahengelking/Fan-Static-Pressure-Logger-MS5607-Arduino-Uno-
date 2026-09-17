# Fan Static-Pressure Logger (MS5607 + Arduino Uno)

A low-cost data-logging system for measuring the small static-pressure
differential produced by a PC fan, using a piezoresistive barometric
sensor and an Arduino Uno. Developed for [course / project name].

## Overview
An absolute barometric pressure sensor (MS5607) is sampled over I²C by an
Arduino Uno. Raw readings are temperature-compensated using the sensor's
factory calibration, averaged to reduce noise, and referenced to a
5-second baseline so that only the *change* in pressure (the fan's effect)
is reported. Data is streamed as CSV over the USB serial port.

## Hardware
| Item | Detail |
|------|--------|
| Microcontroller | Arduino Uno (ATmega328P, 16 MHz, 5 V) |
| Sensor | MS5607-02BA03 barometric pressure sensor on a GY-63 breakout |
| Interface | I²C (100–400 kHz) |
| Host capture | macOS, 115200 baud |

> **Note on the sensor:** the GY-63 boards used here carry **MS5607**
> silicon, *not* MS5611. The two are pin- and protocol-compatible but use
> different compensation constants; applying MS5611 math to an MS5607
> yields roughly half the true pressure. This firmware uses the MS5607
> formula (see `readMS5607()`).

### Wiring (single-sensor configuration)
| GY-63 pin | Arduino Uno |
|-----------|-------------|
| VCC | 5V |
| GND | GND |
| SDA | A4 |
| SCL | A5 |
| PS  | 5V  (selects I²C mode) |
| CSB | 5V  (I²C address 0x76) |
| SDO | not connected |

A dual-sensor variant (inside + outside reference) is included in
`firmware/`; the second sensor uses CSB → GND for address 0x77.

## Sensor / acquisition parameters
| Parameter | Value |
|-----------|-------|
| Oversampling (OSR) | 4096 (maximum) |
| ADC resolution | 24-bit |
| Pressure noise (RMS, single read) | ≈ 2.4 Pa (0.024 mbar) |
| Conversion time per sample | ≈ 9 ms (pressure) + 9 ms (temperature) |
| Samples averaged per logged point | 5 (noise reduced by √5 ≈ 2.2×) |
| Effective logging rate | ≈ 10 Hz (~100 ms/row) |
| Baseline window | 5 s (averaged) |
| I²C clock | 400 kHz |

## Method
1. **PROM read** — the sensor's six factory constants (C1–C6) are read once
   at startup.
2. **Compensated read** — raw pressure (D1) and temperature (D2) are
   converted to Pa and °C using the MS5607 first-order compensation
   (second-order not required above 20 °C).
3. **Averaging** — 5 compensated reads are averaged per output point.
4. **Baseline / zeroing** — pressure is averaged over 5 s to establish a
   reference `p_baseline`; the reported signal is `Δp = p − p_baseline`.
   Sending `z` over serial re-runs the baseline and resets the clock.

## Output format
CSV, one row per sample:

| Column | Units | Description |
|--------|-------|-------------|
| `time_s` | s | Time since last baseline/zero |
| `delta_Pa` | Pa | Pressure change from baseline (the fan signal) |
| `P_Pa` | Pa | Absolute compensated pressure |
| `T_C` | °C | Sensor temperature |

Header and `#`-prefixed comment lines (baseline value, sample count) are
emitted at startup and on each re-zero.

## Usage
1. Open `firmware/fan_logger_single.ino` in the Arduino IDE (2.x or 1.8+).
   **No external libraries are required** — the sensor is accessed via the
   built-in `Wire` library and direct register commands.
2. Select **Board: Arduino Uno** and the correct **Port**, then upload.
3. Open a serial terminal at **115200 baud**. Keep conditions steady for the
   first 5 s while the baseline is taken.
4. Press **`z`** to re-zero before a run; the time column restarts at 0.

### Capturing data to a file
The Arduino IDE Serial Monitor has a limited scrollback buffer. To save a
complete run, either:
- Use **CoolTerm** → *Connection → Capture to Text/Binary File*, or
- Run `tools/logger.py` (requires `pyserial`), which writes `fan_data.csv`
  with an added wall-clock timestamp.

## Limitations
- **Single-sensor drift:** with one absolute sensor, `delta_Pa` includes any
  ambient barometric drift after the baseline is set. Re-zero before each run
  and keep runs short. The dual-sensor variant cancels this by referencing an
  outside sensor.
- **Thermal sensitivity:** airflow across the sensor changes its die
  temperature; because pressure compensation depends on temperature, shield
  the sensor from direct flow (tube tap) and monitor `T_C`.
- **Accuracy:** MS5607 absolute accuracy is on the order of ±1.5 mbar; this
  system measures *relative* change, not absolute pressure.
