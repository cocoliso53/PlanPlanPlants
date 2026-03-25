# PlanPlanPlants

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
