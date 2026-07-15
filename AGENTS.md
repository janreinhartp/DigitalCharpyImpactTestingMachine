# Digital Charpy Impact Testing Machine — Agent Instructions

Firmware for a digital Charpy impact testing machine on the **CrowPanel ESP32-P4** (9" IPS touchscreen, 1024×600). See [README.md](README.md) for full hardware wiring and component specs.

## Build & Flash

Uses **ESP-IDF 5.x** with the IDF extension in VS Code. Standard CMake build system.

```
# Build
idf.py build

# Flash (adjust PORT as needed)
idf.py -p COM3 flash monitor
```

> The CMake project name is still `Lesson18_LVGL_LED` — do not rename it; it affects build output paths.

## Project Structure

```
main/
  main.c              — App entry: LDO init, peripheral init, LVGL port, main loop
  test_manager.c/h    — Core state machine (IDLE→ARMING→ARMED→MEASURING→COMPLETE)
  data_logger.c/h     — SD card CSV logging & RAM history cache (50 entries)
  dbtt_manager.c/h    — DBTT (ductile-to-brittle) multi-specimen session manager
  audio_manager.c/h   — Sound effect helpers (wraps bsp_audio)
  include/
    charpy_calc.h     — Charpy energy formula & data structs (header-only)
    main.h            — Shared includes & log macros
  ui/
    ui.c / ui.h       — Dark-theme styles, screen objects, status bar labels
    ui_dashboard.c    — Home screen
    ui_specimen.c     — Specimen entry form
    ui_test_active.c  — Live angle display during test
    ui_history.c      — Past results list
    ui_settings.c     — Pendulum config (mass, arm length, angles) + calibration
    ui_dbtt*.c        — DBTT workflow screens

peripheral/           — Custom BSP components (each is an IDF component)
  bsp_angle/          — AS5600 I2C encoder + ADC potentiometer (GPIO49, ADC2_CH0)
  bsp_audio/          — I2S audio driver
  bsp_display/        — MIPI DSI EK79007 display init
  bsp_extra/          — PCF8575 relay control + endstop GPIO ISRs
  bsp_i2c/            — Shared I2C master bus (SDA=GPIO45, SCL=GPIO46)
  bsp_illuminate/     — Backlight control
  bsp_pcf8575/        — PCF8575 16-bit I2C I/O expander driver
  bsp_rtc/            — DS1307 RTC driver (I2C 0x68)
  bsp_sdcard/         — SDMMC 1-line SD card (FAT32)
  bsp_servo/          — MCPWM servo driver (GPIO27, 14 V supply)
```

## Key Hardware Facts

| Item | Detail |
|---|---|
| I2C bus | SDA=GPIO45, SCL=GPIO46 (3.3 V); all I2C peripherals on HV side of BSS138 level shifter (5 V) |
| AS5600 encoder | I2C 0x36, 12-bit angle |
| Angle ADC (alt) | ADC2_CH0, GPIO49 — potentiometer fallback, zero-offset & scale stored in NVS |
| DS1307 RTC | I2C 0x68, battery-backed; auto-synced from build time if year < 2025 |
| SD card | FAT32, `/sdcard/` mount point; daily CSV files named `CHARPY_YYYYMMDD.csv` |
| PCF8575 | I2C 0x20 — P0=motor_fwd, P1=motor_rev, P2=safety_lock, P3=release (all active-low outputs) |
| Endstops | GPIO50 = top, GPIO51 = bottom — **direct ESP32 GPIOs**, active-low, internal pull-up |
| Servo | GPIO27, MCPWM, 50 Hz, 1000–2000 µs, 14 V power rail (GND shared with ESP32) |
| I2S audio | IO21 LRCLK, IO22 BCLK, IO23 DOUT; AMP_EN=GPIO30 active-low |
| LVGL | v9.2.2 via managed component; display 1024×600 |

> **README vs code discrepancy**: The README shows the old PCF8575 pinout (P0=motor relay, P1=actuator, P2=endstop). The actual pinout in `bsp_extra.h` is P0=motor_fwd, P1=motor_rev, P2=safety_lock, P3=release with direct GPIO endstops. Trust `bsp_extra.h`.

## Coding Conventions

### Log Macros
Every module defines its own tag and wrapper macros — do not use raw `ESP_LOGI` calls:
```c
#define TM_TAG "TEST_MGR"
#define TM_INFO(fmt, ...)  ESP_LOGI(TM_TAG, fmt, ##__VA_ARGS__)
#define TM_ERROR(fmt, ...) ESP_LOGE(TM_TAG, fmt, ##__VA_ARGS__)
```

### Error Handling
- Public functions return `esp_err_t`. Check with `ESP_ERROR_CHECK()` for fatal paths, or propagate with early return.
- LVGL calls are made only while holding the LVGL mutex via `esp_lvgl_port_lock(0)` / `esp_lvgl_port_unlock()`.

### Active-Low Outputs
All PCF8575-driven relays are active-low. Use `motor_forward_set(true)` / `motor_forward_set(false)` etc. — never toggle the PCF8575 pin directly from application code.

### NVS Storage
Namespace: `"charpy"`. Keys defined in `test_manager.c` (`NVS_KEY_*`). Pendulum config (`mass`, `arm_len`, `rel_ang`) and calibration (`zero_off`, `scale`, `brake_ang`, `brake_hld`, `latch_ms`) are persisted here.

### LVGL UI Patterns
- Screen objects declared as `extern lv_obj_t *` in `ui.h`; created in `ui_*.c` files
- Dark-theme color palette defined in `ui.h` (`UI_COLOR_*` macros)
- Status bar labels (`ui_status_time_label`, `ui_status_date_label`, etc.) updated from main loop

### Adding a New Peripheral BSP
1. Create `peripheral/bsp_foo/` with `CMakeLists.txt` and `include/bsp_foo.h`
2. Add `bsp_foo` to `REQUIRES` in `main/CMakeLists.txt`

## Test State Machine

Full lifecycle: `IDLE → SPECIMEN_ENTRY → HOMING → HOMED_FWD → LATCH_OPENING → RETURNING_HOME → LATCHED → ARMING → ARMED → RELEASED → MEASURING → COMPLETE → LOGGING → IDLE`

See [main/include/test_manager.h](main/include/test_manager.h) for the full enum and API.

## Energy Calculation

```
E = m * g * L * (cos(β) − cos(α))
```

Implemented as `charpy_calc_energy()` in [main/include/charpy_calc.h](main/include/charpy_calc.h). Default pendulum: 3.95 kg, 0.4 m arm, 150° release.

## DBTT Mode

`dbtt_manager` runs a multi-specimen session at varying temperatures to plot the ductile-to-brittle transition curve. Session data lives in RAM only; results are also written to the standard CSV log by `data_logger`.
