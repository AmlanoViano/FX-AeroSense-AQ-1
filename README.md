# FX AeroSense AQ-1

> Drone-mounted air quality monitor for spatial pollution mapping over Dhaka, Bangladesh.

Built on an ESP32, the AQ-1 logs particulate matter, CO₂, NOₓ, temperature, and humidity to a timestamped CSV at 5-second intervals during flight.

---

## Sensors

| Sensor | Interface | Measures |
|---|---|---|
| Sensirion SPS30 | I²C `0x69` | PM₁.₀, PM₂.₅, PM₄.₀, PM₁₀, number concentration, typical particle size |
| Sensirion SHT35 | I²C `0x44` | Temperature (°C), relative humidity (%) |
| Winsen MH-Z19E | UART2 9600 8N1 | CO₂ (ppm) |
| SGX MICS-4514 | ADC GPIO 34/35 | NOₓ, reducing gases (raw ADC) |
| DS3231 | I²C `0x68` | Real-time clock with battery backup |

---

## Wiring

```
ESP32 GPIO 21 (SDA) ──── SPS30, SHT35, DS3231
ESP32 GPIO 22 (SCL) ──── SPS30, SHT35, DS3231
ESP32 GPIO 16 (RX2) ──── MH-Z19E TX
ESP32 GPIO 17 (TX2) ──── MH-Z19E RX
ESP32 GPIO  5 (CS)  ──── microSD
ESP32 GPIO 23 (MOSI)──── microSD
ESP32 GPIO 19 (MISO)──── microSD
ESP32 GPIO 18 (SCK) ──── microSD
ESP32 GPIO 34       ──── MICS-4514 NOX out  [voltage divider → 3.3 V max]
ESP32 GPIO 35       ──── MICS-4514 RED out  [voltage divider → 3.3 V max]
ESP32 GPIO  2       ──── Status LED (330 Ω series)
```

> **Power:** SPS30 and MH-Z19E require 5 V. MICS-4514 heater requires 5 V. All modules share a common ground. ESP32, SHT35, and SD module run on 3.3 V.

---

## CSV output format

One file per day: `YYYY-MM-DD.csv`

```
Timestamp, PM1.0, PM2.5, PM4.0, PM10.0,
NC0.5, NC1.0, NC2.5, NC4.0, NC10.0, TypicalSize,
Temperature_C, Humidity_%, CO2_ppm,
NOX_ADC, RED_ADC, NOX_V, RED_V
```

Failed reads are recorded as `-999` / `-999.9` to preserve row structure. Filter before analysis.

---

## Sensor physics

PM mass concentrations are reported in $\mu g \cdot m^{-3}$. The SPS30 derives these from raw particle counts using Mie scattering theory, assuming a mean particle density $\rho_p$:

$$C_{mass} = \frac{\pi}{6} \rho_p \sum_i d_i^3 \cdot N_i$$

where $d_i$ is the midpoint diameter of size bin $i$ and $N_i$ is the number concentration in that bin $(\text{cm}^{-3})$.

The MICS-4514 outputs a load voltage $V_L$ from which sensor resistance is derived as:

$$R_S = R_L \left( \frac{V_{CC}}{V_L} - 1 \right)$$

Gas concentration is then estimated from the $R_S / R_0$ ratio using the datasheet calibration curve, where $R_0$ is the baseline resistance in clean air. Temperature compensation uses the SHT35 reading.

---

## Repository structure

```
FX-AeroSense-AQ1/
├── firmware/
│   └── air_quality_monitor/
│       └── air_quality_monitor.ino
├── analysis/               # data analysis pipeline (in development)
├── data/
│   └── sample/
│       └── sample_data.csv
├── hardware/
│   └── wiring_diagram.pdf
├── .gitignore
└── README.md
```

---

## Dependencies

Install via Arduino Library Manager:

- `RTClib` — Adafruit
- `Adafruit SHT31 Library` — Adafruit
- `MHZ19` — Jonathan Dempsey
- `Sensirion I2C SPS30` — Sensirion
- `SD`, `Wire`, `SPI` — bundled with ESP32 Arduino core

---

## License

MIT © FX AeroSense
