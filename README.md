# PlanPlanPlants

## Node Pinout Mapping

Pin numbers below are GPIO numbers, not physical header positions. These mappings
match the active `mini_node` and `main_node` PlatformIO environments in
[planplants/platformio.ini](planplants/platformio.ini).

### Mini Node — ESP32-C3

The `mini_node` environment uses the `esp32-c3-devkitm-1` board target. Sensor pin
assignments are defined in
[planplants/src/espnow_common.h](planplants/src/espnow_common.h).

| Connection | ESP32-C3 pin | Firmware constant | Purpose |
| --- | --- | --- | --- |
| Soil moisture sensor analog output | GPIO 0 | `MOISTURE_PIN` | Raw soil moisture ADC reading |
| Battery sensing signal | GPIO 1 | `BATTERY_PIN` | Raw battery ADC reading |
| BH1750 SDA | GPIO 6 | `SDA_PIN` | I²C data |
| BH1750 SCL | GPIO 7 | `SCL_PIN` | I²C clock |
| Both sensors’ GND | GND | — | Connect the soil moisture sensor and BH1750 GND pins here |
| Both sensors’ VCC | 3V3 | — | Connect the soil moisture sensor and BH1750 VCC pins here |

The BH1750 is configured at I²C address `0x23` (`LUX_SENSOR_ADDRESS`), with its
ADDR pin connected to GND. The battery input is a sensing signal; the firmware
does not define the battery voltage-divider circuit or convert the reading to
volts.

### Main Node — ESP32

The `main_node` environment uses the `esp32dev` board target. LED pin assignments
are defined in
[planplants/src/main_node.cpp](planplants/src/main_node.cpp).

| Connection | ESP32 pin | Firmware constant | Purpose |
| --- | --- | --- | --- |
| Mode LED | GPIO 2 | `MODE_LED_PIN` | Pulses once per minute while listening over ESP-NOW; stays on when Wi-Fi connects |
| Activity LED | GPIO 4 | `ACTIVITY_LED_PIN` | Blinks three times when a reading batch arrives and twice before an HTTP upload |
| External LED ground | GND | — | Common ground for external LEDs |

Both LEDs are active high. Connect each external LED through a current-limiting
resistor. Both LEDs blink six times if the Wi-Fi connection times out.

The main node receives sensor readings wirelessly over ESP-NOW and forwards them
over Wi-Fi, so it has no sensor GPIO connections. No signal wires are required
between the two nodes.

## Server Deployment

The Go backend runs in one Docker container. SQLite does not need a separate
container; its database file remains on the server and is mounted into the
backend container.

### Verify Docker

```bash
sudo systemctl status docker
sudo docker run --rm hello-world
```

### Build and Run

Ensure the persistent database directory exists:

```bash
mkdir -p /home/cuau/Projects/PlanPlanPlants/data
```

Build the backend image:

```bash
sudo docker build -t planplants /home/cuau/Projects/PlanPlanPlants
```

Run the backend:

```bash
sudo docker run -d \
  --name planplants \
  --restart unless-stopped \
  -p 8080:8080 \
  -v /home/cuau/Projects/PlanPlanPlants/data:/app/data \
  planplants
```

### Telegram Bot

The Telegram bot runs inside the same backend process. There is no separate bot
binary or Docker container.

If `TELEGRAM_BOT_TOKEN` is not set, the backend still runs normally, but the bot
does not start.

For local development, compile the backend and then run the binary:

```bash
go build -o planplants .
TELEGRAM_BOT_TOKEN="your_bot_token_here" ./planplants
```

For Docker, pass the token as an environment variable when starting the
container:

```bash
sudo docker run -d \
  --name planplants \
  --restart unless-stopped \
  -p 8080:8080 \
  -e TELEGRAM_BOT_TOKEN="your_bot_token_here" \
  -v /home/cuau/Projects/PlanPlanPlants/data:/app/data \
  planplants
```

Use `/latets` in Telegram to get the latest 5 readings from the database. If you do not want to run the bot, omit the `-e TELEGRAM_BOT_TOKEN=...` line.

The persistent database lives at:

```text
/home/cuau/Projects/PlanPlanPlants/data/planplants.db
```

Inside the container, the backend accesses the same file at:

```text
/app/data/planplants.db
```

### Check the Backend

```bash
sudo docker logs planplants
curl http://localhost:8080/health
```

Expected health response:

```json
{"status":"ok"}
```

### Deploy an Updated Version

```bash
sudo docker build -t planplants /home/cuau/Projects/PlanPlanPlants
sudo docker stop planplants
sudo docker rm planplants
sudo docker run -d \
  --name planplants \
  --restart unless-stopped \
  -p 8080:8080 \
  -e TELEGRAM_BOT_TOKEN="your_bot_token_here" \
  -v /home/cuau/Projects/PlanPlanPlants/data:/app/data \
  planplants
```


## Soil Sensor Calibration Analysis

### Air

Dataset analyzed: [data/calibration_soil_sensor_logs_air.csv](/home/cuau/Projects/PlanPlanPlants/data/calibration_soil_sensor_logs_air.csv)

This file contains 57 air calibration readings for `soil_sensor_1`. The goal of this pass was to get a basic statistical view of the raw values before using them for calibration work.

#### Summary Statistics

| Metric | Value |
| --- | ---: |
| Sample count | 57 |
| Mean | 2732.35 |
| Median | 2736 |
| Mode | 2699 |
| Min | 2612 |
| Max | 2847 |
| Range | 235 |
| Sample standard deviation | 48.75 |
| Sample variance | 2376.95 |
| Q1 | 2699 |
| Q3 | 2758 |
| IQR | 59 |
| Coefficient of variation | 1.78% |

#### Distribution Check

- The distribution is centered tightly around the mean and median, which are very close to each other (`2732.35` vs `2736`).
- Skewness is `0.219`, which indicates only a slight right skew.
- Excess kurtosis is `-0.051`, which is very close to zero and therefore close to a normal-shaped distribution.
- A Jarque-Bera normality check gives `JB = 0.461` with an approximate `p = 0.794`.
- Based on that result, there is no strong evidence to reject a normal-distribution assumption for this sample.

This does not prove the process is perfectly normal, but for a small first-pass calibration analysis, the readings look reasonably close to normal.

#### Outliers

- Using Tukey fences (`Q1 - 1.5*IQR`, `Q3 + 1.5*IQR`), the lower and upper bounds are `2610.5` and `2846.5`.
- One value falls just outside the upper fence: `2847`.
- That is a very mild outlier, not a dramatic anomaly.

#### Stability Notes

- The coefficient of variation is only `1.78%`, which suggests the air calibration readings are fairly stable relative to their mean.
- The lag-1 autocorrelation is `-0.104`, so there is no obvious strong short-term dependence between consecutive readings.
- A simple linear trend over sample order is about `+0.39` units per sample, which is small relative to the standard deviation.

#### Calibration Interpretation

- For an "air" reference point, this dataset is fairly tight and consistent.
- A practical baseline for the air calibration point is the mean, around `2732`.
- A rough expected band for normal readings is approximately mean ± 2 standard deviations:
  - `2635` to `2829`
- Readings far outside that band should be treated as suspicious and rechecked against wiring, power stability, sensor placement, or ADC noise.

#### Next Steps

- Collect a matching dataset for fully wet soil or water immersion.
- Compare both calibration datasets to estimate the usable sensor range.
- Plot a histogram and QQ plot to visually confirm the normality assumption.
- Repeat the capture on different days to verify the calibration point is stable over time.

### Water

Dataset analyzed: [data/calibration_soil_sensor_logs_water.csv](/home/cuau/Projects/PlanPlanPlants/data/calibration_soil_sensor_logs_water.csv)

This file contains 61 water calibration readings for `soil_sensor_1`. The goal of this pass was to get the same first statistical view as the air dataset, but for the fully wet reference point.

#### Summary Statistics

| Metric | Value |
| --- | ---: |
| Sample count | 61 |
| Mean | 1125.30 |
| Median | 1124 |
| Mode | 1102 |
| Min | 1028 |
| Max | 1207 |
| Range | 179 |
| Sample standard deviation | 35.83 |
| Sample variance | 1284.14 |
| Q1 | 1102 |
| Q3 | 1149 |
| IQR | 47 |
| Coefficient of variation | 3.18% |

#### Distribution Check

- The distribution is centered tightly around the mean and median, which are again very close to each other (`1125.30` vs `1124`).
- Skewness is `0.103`, which indicates only a very slight right skew.
- Excess kurtosis is `0.179`, which is close to zero and still consistent with an approximately normal shape.
- A Jarque-Bera normality check gives `JB = 0.190` with an approximate `p = 0.910`.
- Based on that result, there is no strong evidence to reject a normal-distribution assumption for this sample.

This does not prove the process is perfectly normal, but for a first-pass calibration analysis, the water readings also look reasonably close to normal.

#### Outliers

- Using Tukey fences (`Q1 - 1.5*IQR`, `Q3 + 1.5*IQR`), the lower and upper bounds are `1031.5` and `1219.5`.
- One value falls just below the lower fence: `1028`.
- That is a mild outlier, not a dramatic anomaly.

#### Stability Notes

- The coefficient of variation is `3.18%`, which is still low and suggests the water calibration readings are fairly stable relative to their mean.
- The lag-1 autocorrelation is `-0.054`, so there is no obvious strong short-term dependence between consecutive readings.
- A simple linear trend over sample order is about `+0.34` units per sample, which is small relative to the standard deviation.

#### Calibration Interpretation

- For a "water" reference point, this dataset is also fairly tight and consistent.
- A practical baseline for the water calibration point is the mean, around `1125`.
- A rough expected band for normal readings is approximately mean ± 2 standard deviations:
- `1054` to `1197`
- Readings far outside that band should be treated as suspicious and rechecked against immersion depth, power stability, wiring, or ADC noise.

#### Next Steps

- Compare the air mean (`2732`) against the water mean (`1125`) to estimate the raw sensor span.
- Use both baselines to define a first normalization formula for moisture percentage.
- Plot both datasets together to verify the two calibration regions are well separated.
- Repeat both captures on different days to confirm that the calibration points stay stable over time.

### Dry Soil 1

Dataset analyzed: [data/calibration_soil_sensor_logs_dry_soil1.csv](/home/cuau/Projects/PlanPlanPlants/data/calibration_soil_sensor_logs_dry_soil1.csv)

This file contains 59 dry-soil calibration readings for `soil_sensor_1`. The goal of this pass was to evaluate the dry-soil reference point with the same first statistical view used for the air and water datasets.

#### Summary Statistics

| Metric | Value |
| --- | ---: |
| Sample count | 59 |
| Mean | 2577.83 |
| Median | 2581 |
| Mode | 2559 |
| Min | 2533 |
| Max | 2639 |
| Range | 106 |
| Sample standard deviation | 27.50 |
| Sample variance | 756.11 |
| Q1 | 2557.5 |
| Q3 | 2602 |
| IQR | 44.5 |
| Coefficient of variation | 1.07% |

#### Distribution Check

- The distribution is centered tightly around the mean and median, which are close to each other (`2577.83` vs `2581`).
- Skewness is `0.141`, which indicates only a slight right skew.
- Excess kurtosis is `-1.132`, which suggests a somewhat flatter-than-normal shape, but not an extreme departure.
- A Jarque-Bera normality check gives `JB = 3.344` with an approximate `p = 0.188`.
- Based on that result, there is still no strong evidence to reject a normal-distribution assumption for this sample.

This does not prove the process is perfectly normal, but for a first-pass calibration analysis, the dry-soil readings also look reasonably close to normal.

#### Outliers

- Using Tukey fences (`Q1 - 1.5*IQR`, `Q3 + 1.5*IQR`), the lower and upper bounds are `2490.75` and `2668.75`.
- No values fall outside those bounds.
- That suggests the dry-soil calibration run is internally consistent and free of obvious outliers.

#### Stability Notes

- The coefficient of variation is `1.07%`, which is the lowest of the three datasets so far and suggests the dry-soil readings are very stable relative to their mean.
- The lag-1 autocorrelation is `-0.114`, so there is no obvious strong short-term dependence between consecutive readings.
- A simple linear trend over sample order is about `-0.37` units per sample, which is small relative to the standard deviation.

#### Calibration Interpretation

- For a "dry soil" reference point, this dataset is tight and consistent.
- A practical baseline for the dry-soil calibration point is the mean, around `2578`.
- A rough expected band for normal readings is approximately mean ± 2 standard deviations:
- `2523` to `2633`
- Readings far outside that band should be treated as suspicious and rechecked against sensor placement, soil packing density, contact consistency, or ADC noise.

#### Next Steps

- Compare the dry-soil mean (`2578`) against the air mean (`2732`) to understand how separated those two dry-end conditions really are.
- Compare the dry-soil mean (`2578`) against the water mean (`1125`) to estimate the practical measurement span.
- Capture more dry-soil runs with different packing densities to see how sensitive the sensor is to compaction.
- Plot air, dry soil, and water together to decide which pair of reference points is best for moisture normalization.
