# Onderzoek sensoren testen op gras

### Lijst van sensoren die getest worden
- VL53L1X (Time of Flight sensor)
- RCWL-1604 (Ultrasoon sensor)

---

### Testcriteria

| Criterium | Waarde |
|-----------|--------|
| Maximale margin of error | ≤ 5% |
| Minimale testduur | 10 seconden |
| Maximale snelheid tijdens testen | 10 km/h |
| Aantal herhalingen per test | 10 |
| Afronding resultaten | 1 decimaal |
| Testomgeving | Binnen en op gras |

---

### Tests

#### Test 1 — ToF data op gladde vloer

**Sensor:** VL53L1X
**Datum:** 22 Maart 2026
**Testduur:** 10 sec
**Snelheid:** 2 km/h
**Omstandigheden:** Binnen, vlake vloer

| Meting     | Gemeten waarde (cm) | Werkelijke waarde (cm) | Afwijking (cm) | Margin of error (%) |
| ---------- | ------------------- | ---------------------- | -------------- | ------------------- |
| 1          | 137                 | 150                    | 13             | 8.7                 |
| 2          | 138                 | 150                    | 12             | 8.0                 |
| 3          | 135                 | 150                    | 15             | 10.0                |
| 4          | 135                 | 150                    | 15             | 10.0                |
| 5          | 136                 | 150                    | 14             | 9.3                 |
| 6          | 140                 | 150                    | 10             | 6.7                 |
| 7          | 138                 | 150                    | 12             | 8.0                 |
| 8          | 139                 | 150                    | 11             | 7.3                 |
| 9          | 137                 | 150                    | 13             | 8.7                 |
| 10         | 140                 | 150                    | 10             | 6.7                 |
| Gemiddelde | 137.5               | 150                    | 12.5           | 8.3                 |

**Voldoet aan criteria:** Nee
**Opmerkingen:** <!-- Opmerkingen invullen -->

---

#### Test 2 — Sonic data op gladde vloer

**Sensor:** RCWL-1604
**Datum:** 22 Maart 2026
**Testduur:** 10 sec
**Snelheid:** 2 km/h
**Omstandigheden:** Binnen, vlake vloer

| Meting     | Gemeten waarde (cm) | Werkelijke waarde (cm) | Afwijking (cm) | Margin of error (%) |
| ---------- | ------------------- | ---------------------- | -------------- | ------------------- |
| 1          | 144                 | 150                    | 6              | 4.0                 |
| 2          | 144                 | 150                    | 6              | 4.0                 |
| 3          | 144                 | 150                    | 6              | 4.0                 |
| 4          | 143                 | 150                    | 7              | 4.7                 |
| 5          | 144                 | 150                    | 6              | 4.0                 |
| 6          | 144                 | 150                    | 6              | 4.0                 |
| 7          | 143                 | 150                    | 7              | 4.7                 |
| 8          | 144                 | 150                    | 6              | 4.0                 |
| 9          | 158                 | 150                    | 8              | 5.3                 |
| 10         | 154                 | 150                    | 4              | 2.7                 |
| Gemiddelde | 146.2               | 150                    | 6.2            | 4.1                 |

**Voldoet aan criteria:** Ja
**Opmerkingen:** <!-- Opmerkingen invullen -->

---

#### Test 3 — ToF data 3cm gras

**Sensor:** VL53L1X
**Datum:** 22 Maart 2026
**Testduur:** 10 sec
**Snelheid:** 5 km/h
**Omstandigheden:** Buiten, 3cm gras, vochtig, schemerend weer

| Meting     | Gemeten waarde (cm) | Werkelijke waarde (cm) | Afwijking (cm) | Margin of error (%) |
| ---------- | ------------------- | ---------------------- | -------------- | ------------------- |
| 1          | 129                 | 140                    | 11             | 7.9                 |
| 2          | 129                 | 140                    | 11             | 7.9                 |
| 3          | 130                 | 140                    | 10             | 7.1                 |
| 4          | 146                 | 140                    | 6              | 4.3                 |
| 5          | 132                 | 140                    | 8              | 5.7                 |
| 6          | 146                 | 140                    | 6              | 4.3                 |
| 7          | 131                 | 140                    | 9              | 6.4                 |
| 8          | 140                 | 140                    | 0              | 0.0                 |
| 9          | 145                 | 140                    | 5              | 3.6                 |
| 10         | 138                 | 140                    | 2              | 1.4                 |
| Gemiddelde | 136.6               | 140                    | 6.8            | 4.9                 |

**Voldoet aan criteria:** Ja
**Opmerkingen:** <!-- Opmerkingen invullen -->

---

#### Test 4 — Sonic data 3cm gras

**Sensor:** RCWL-1604
**Datum:** 22 Maart 2026
**Testduur:** 10 sec
**Snelheid:** 5 km/h
**Omstandigheden:** Buiten, 3cm gras, vochtig, schemerend weer

| Meting     | Gemeten waarde (cm) | Werkelijke waarde (cm) | Afwijking (cm) | Margin of error (%) |
| ---------- | ------------------- | ---------------------- | -------------- | ------------------- |
| 1          | 161                 | 140                    | 21             | 15.0                |
| 2          | 177                 | 140                    | 37             | 26.4                |
| 3          | 167                 | 140                    | 27             | 19.3                |
| 4          | 164                 | 140                    | 24             | 17.1                |
| 5          | 177                 | 140                    | 37             | 26.4                |
| 6          | 158                 | 140                    | 18             | 12.9                |
| 7          | 171                 | 140                    | 31             | 22.1                |
| 8          | 177                 | 140                    | 37             | 26.4                |
| 9          | 154                 | 140                    | 14             | 10.0                |
| 10         | 174                 | 140                    | 34             | 24.3                |
| Gemiddelde | 168                 | 140                    | 28             | 20.0                |

**Voldoet aan criteria:** Nee
**Opmerkingen:** <!-- Opmerkingen invullen -->

---

### Rapport van verzamelde testdata en overwegingen

#### Samenvatting resultaten

| Sensor | Gemiddelde margin of error (%) | Werkt op 10 km/h | Voldoet aan alle criteria |
|--------|-------------------------------|------------------|---------------------------|
| VL53L1X |              |                  |                           |
| RCWL-1604 |              |                  |                           |

#### Overwegingen

<!-- Overwegingen invullen op basis van de testdata -->

---

### Conclusie

<!-- Conclusie invullen -->

---

### Bronnen
<!-- Bronnen invullen -->