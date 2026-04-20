## Plan: Digital Charpy Impact Tester Firmware

Digitize the Charpy impact tester by adding an **AS5600 magnetic angle sensor** and **DS3231 RTC** to the CrowPanel ESP32-P4 9" display. The system reads the pendulum arm angle via I2C, calculates absorbed energy (Joules), displays results on a redesigned touch UI, and logs extended test data to the built-in SD card. Motor/actuator relay control included; test is physically triggered.

### Hardware Wiring Summary

| Device | Interface | Pins | I2C Address |
|---|---|---|---|
| AS5600 (angle sensor) | I2C (shared) | IO45 SDA, IO46 SCL | 0x36 |
| DS3231 (RTC) | I2C (shared) | IO45 SDA, IO46 SCL | 0x68 |
| GT911 (touch, existing) | I2C (shared) | IO45 SDA, IO46 SCL | 0x14 |
| SD Card (built-in) | SDMMC | IO44 CMD, IO43 SCK, IO39 D0 | — |
| I2S Audio (speaker) | I2S STD | IO21 LRCLK, IO22 BCLK, IO23 SDATA, IO30 AMP CTRL | — |
| Motor relay | GPIO out | IO47 (repurposed UART1 TX) | — |
| Actuator relay | GPIO out | IO48 (repurposed UART1 RX) | — |
| Endstop switch | GPIO in (pull-up) | IO33 (repurposed UART3 RX) | — |

---

**Steps**

### Phase 1: Peripheral Drivers (all steps parallelizable)

1. **AS5600 driver** — Create `peripheral/bsp_angle/` component. I2C reads of raw angle register (0x0C-0x0D), convert 12-bit → 0°-360°. Configurable zero offset. High-speed continuous sampling function (~1kHz) for capturing swing profile.

2. **DS3231 RTC driver** — Create `peripheral/bsp_rtc/` component. Read/write time registers with BCD conversion. Get/set datetime, format timestamp strings for logging.

3. **SD card filesystem driver** — Create `peripheral/bsp_sdcard/` component. SDMMC host init (1-line mode), mount FAT32, CSV file create/append helpers.

4. **GPIO relay & endstop driver** — Extend existing `peripheral/bsp_extra/` to add motor relay (IO47), actuator relay (IO48), and endstop switch input (IO33) with debounce.

5. **Audio driver** — Create `peripheral/bsp_audio/` component (based on Elecrow Lesson12 reference). I2S STD mode on I2S_NUM_1 (master, 16kHz/16-bit stereo). GPIO config: BCLK=IO22, LRCLK=IO21, SDATA=IO23, amplifier control=IO30 (active-low enable). Functions: `audio_init()`, `audio_ctrl_init()`, `set_Audio_ctrl(bool)`, `Audio_play_wav_sd(const char *filepath)`. WAV header validation (RIFF/PCM). DMA-buffered playback from SD card with volume scaling. Short notification WAV files stored on SD card under `/sdcard/sounds/`.

### Phase 2: Application Logic

6. **Energy calculation module** — *depends on step 1*. Formula: $E = m \cdot g \cdot L \cdot (\cos\beta - \cos\alpha)$, where $\alpha$ = release angle, $\beta$ = post-impact angle. Pendulum mass/length configurable via NVS.

7. **Test state machine** — *depends on steps 1, 3, 4, 5, 6*. Create `main/test_manager.c`. States: `IDLE → SPECIMEN_ENTRY → ARMING → ARMED → RELEASED → MEASURING → RESULT → LOGGING → IDLE`. ARMED→RELEASED transition is triggered by the operator pressing the "RELEASE ARM" button on the touchscreen (fires actuator relay). A dedicated high-priority FreeRTOS task samples the AS5600 at ~1kHz during MEASURING, detects when the pendulum settles (angle stable ±0.5° for 500ms). ABORT can return to IDLE from ARMING or ARMED states.

   **Sound notifications** (played via `bsp_audio` on a low-priority FreeRTOS task, non-blocking):
   - `ARMED` → play `/sdcard/sounds/armed.wav` — short confirmation beep (operator knows arm is ready)
   - `RELEASED → MEASURING` → play `/sdcard/sounds/released.wav` — alert tone (stand clear)
   - `COMPLETE` → play `/sdcard/sounds/complete.wav` — success chime (test done, review results)
   - `ABORT` → play `/sdcard/sounds/abort.wav` — warning tone
   - `ERROR` → play `/sdcard/sounds/error.wav` — error buzzer
   - Sound playback must not block the state machine or UI — use a dedicated `audio_task` with a FreeRTOS queue of sound file paths. State machine posts to queue, audio task picks up and plays.

8. **Data logging module** — *depends on steps 2, 3*. CSV format: `timestamp, specimen_id, material, dimensions, operator, release_angle, final_angle, energy_joules`. One file per day (`/sdcard/CHARPY_YYYYMMDD.csv`). Recent 50 results cached in RAM for UI history.

### Phase 3: UI (LVGL) — Complete Redesign

9. **New UI screens** — *depends on steps 6, 7, 8*. Replace demo UI entirely:
   - **Dashboard** — Live angle arc/gauge, last result (angle + energy), status indicator, nav buttons (New Test, History, Settings)
   - **Specimen Entry** — Text inputs for specimen ID, material, dimensions, operator name. On-screen keyboard. "Start Test" button.
   - **Test Active** — Live angle animation, state text ("Lifting arm…" → "Armed" → "Measuring…" → "Complete"), result display (angle + energy), Save/Discard buttons.
   - **History** — Scrollable table of past tests from SD card (date, specimen, material, angle, energy).
   - **Settings** — Pendulum mass/length, release angle, AS5600 zero calibration button, RTC date/time set, SD card status.

### Phase 3a: UI Layout & Dark Theme Specification

**Display**: 1024x600 px, landscape. All screens share a common dark theme and top status bar.

#### Dark Theme Color Palette

| Role | Hex | Usage |
|---|---|---|
| Background | `#1A1A2E` | Screen background, deepest layer |
| Surface | `#16213E` | Cards, panels, content containers |
| Surface Elevated | `#0F3460` | Active cards, selected items, modal backgrounds |
| Primary Accent | `#00D4FF` | Arc gauges, active buttons, focus rings, highlights |
| Secondary Accent | `#E94560` | Warnings, discard buttons, error states |
| Success | `#00E676` | "Armed" status, save buttons, good results |
| Text Primary | `#EAEAEA` | Headings, large values, primary labels |
| Text Secondary | `#8892A0` | Subtitles, units, secondary info |
| Text Muted | `#4A5568` | Disabled text, placeholders |
| Border | `#2D3748` | Dividers, card outlines, input borders |

#### Fonts

| Size | Usage |
|---|---|
| 48px Bold | Large result values (angle, energy) |
| 28px Bold | Section headings, gauge labels |
| 20px Regular | Body text, table rows, input text |
| 16px Regular | Status bar, timestamps, units, captions |

#### Top Status Bar (shared across all screens, 1024x40 px)

```
┌──────────────────────────────────────────────────────────────────────────┐
│  ◉ CHARPY TESTER          IDLE          12:34:56  20-Apr-2026   SD ✓   │
│  [logo/dot]   [app title]  [state badge]    [RTC time]   [date]  [sd]  │
└──────────────────────────────────────────────────────────────────────────┘
```
- Height: 40px, Background: `#16213E`, bottom border 1px `#2D3748`
- Left: colored status dot (green=idle, cyan=measuring, yellow=arming, red=error) + app title
- Center: state badge with rounded background (e.g. `IDLE` on `#0F3460` pill)
- Right: RTC time (live), date, SD card icon (checkmark if mounted, ✗ if missing)

---

#### Screen 1: Dashboard (default, 1024x560 below status bar)

```
┌─────────────────────────────────────────────────────────────────────────┐
│ Status Bar (40px)                                                       │
├──────────────────────────────┬──────────────────────────────────────────┤
│                              │                                          │
│      ┌──────────────┐       │   LAST TEST RESULT          [timestamp]  │
│      │              │       │   ─────────────────────────────────────   │
│      │   ARC GAUGE  │       │                                          │
│      │   270° sweep │       │   ANGLE        ENERGY                    │
│      │              │       │   ┌──────┐     ┌──────┐                  │
│      │  current:    │       │   │127.3°│     │ 48.2 │                  │
│      │   45.2°      │       │   │      │     │  J   │                  │
│      └──────────────┘       │   └──────┘     └──────┘                  │
│                              │                                          │
│    LIVE ANGLE                │   Specimen: STEEL-042                    │
│                              │   Material: Mild Steel                   │
│                              │   Operator: J. Reyes                     │
├──────────────────────────────┼──────────────────────────────────────────┤
│                              │                                          │
│   [ ▶ NEW TEST ]            │  [ 📋 HISTORY ]    [ ⚙ SETTINGS ]       │
│                              │                                          │
└──────────────────────────────┴──────────────────────────────────────────┘
```

**Layout breakdown:**
- **Left half (500px wide)**: Large arc gauge (LVGL `lv_arc`)
  - Arc radius ~180px, 270° sweep (from 7 o'clock to 5 o'clock)
  - Arc track: `#2D3748`, arc indicator: `#00D4FF` gradient
  - Center label: current angle in 48px bold `#EAEAEA`
  - Sub-label "LIVE ANGLE" in 16px `#8892A0`
  - Arc animates in real-time from AS5600 reading

- **Right half (524px wide)**: Last test result card
  - Card background: `#16213E` with 12px rounded corners, 1px `#2D3748` border
  - Top row: "LAST TEST RESULT" heading (20px `#8892A0`) + timestamp right-aligned
  - Two result boxes side-by-side:
    - Angle box: `#0F3460` background, angle value in 48px `#00D4FF`, "°" unit in 20px `#8892A0`
    - Energy box: `#0F3460` background, energy value in 48px `#00E676`, "J" unit in 20px `#8892A0`
  - Below: specimen summary (ID, Material, Operator) in 16px `#8892A0`

- **Bottom row (80px tall)**: Navigation buttons
  - "NEW TEST" button: `#00D4FF` background, `#1A1A2E` text, 48px wide, left side — primary action
  - "HISTORY" button: `#16213E` background, `#00D4FF` border/text, right area
  - "SETTINGS" button: `#16213E` background, `#8892A0` border/text, far right

---

#### Screen 2: Specimen Entry (1024x560)

```
┌─────────────────────────────────────────────────────────────────────────┐
│ Status Bar (40px)                                                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   NEW TEST — Enter Specimen Details                          [ ← Back ] │
│   ─────────────────────────────────────────────────────────────────────  │
│                                                                         │
│   ┌─ Specimen ID ──────────────┐   ┌─ Operator ───────────────────┐    │
│   │  STEEL-043                 │   │  J. Reyes                    │    │
│   └────────────────────────────┘   └──────────────────────────────┘    │
│                                                                         │
│   ┌─ Material Type ────────────┐   ┌─ Dimensions (W×H×L mm) ────┐    │
│   │  Mild Steel            [▼] │   │  10 × 10 × 55             │    │
│   └────────────────────────────┘   └──────────────────────────────┘    │
│                                                                         │
│   ┌─ Notes (optional) ────────────────────────────────────────────┐    │
│   │                                                               │    │
│   └───────────────────────────────────────────────────────────────┘    │
│                                                                         │
│                              [ ▶ ARM & START TEST ]                     │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**Layout breakdown:**
- **Header row**: Screen title "NEW TEST — Enter Specimen Details" (28px `#EAEAEA`), "Back" button top-right
- **Form area** (2-column grid, 20px padding):
  - Input fields: `#16213E` background, 1px `#2D3748` border, on focus border becomes `#00D4FF`
  - Label floats above input in 16px `#8892A0`
  - Input text: 20px `#EAEAEA`
  - Material Type: dropdown with common presets (Mild Steel, Stainless Steel, Aluminum, Copper, Plastic, Custom)
  - Dimensions: three numeric inputs with `×` separators, defaulting to standard Charpy 10×10×55mm
  - Notes: optional full-width textarea
- **Bottom**: "ARM & START TEST" button, full-width, `#00D4FF` background, `#1A1A2E` text, 56px tall
- **On-screen keyboard**: LVGL built-in keyboard appears from bottom when input focused, dark-styled

---

#### Screen 3: Test Active (1024x560)

The bottom action area changes based on the current state:

**During ARMING:**
```
┌─────────────────────────────────────────────────────────────────────────┐
│ Status Bar (40px)  [state: ARMING]                                      │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│                    ┌──────────────────────────┐                         │
│                    │                          │                         │
│                    │      LARGE ARC GAUGE     │                         │
│                    │       (full width)       │                         │
│                    │                          │                         │
│                    │        150.0°            │                         │
│                    │                          │                         │
│                    └──────────────────────────┘                         │
│                                                                         │
│    ┌─ STATE ─────────────────────────────────────────────────────┐      │
│    │   ● ○ ○ ○     ARMING — Lifting arm... Stand clear.          │      │
│    └─────────────────────────────────────────────────────────────┘      │
│                                                                         │
│                         [ ✗ ABORT TEST ]                                │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**During ARMED (arm at top, awaiting release):**
```
┌─────────────────────────────────────────────────────────────────────────┐
│ Status Bar (40px)  [state: ARMED]                                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│                    ┌──────────────────────────┐                         │
│                    │                          │                         │
│                    │      LARGE ARC GAUGE     │                         │
│                    │    (pulsing green glow)  │                         │
│                    │                          │                         │
│                    │        150.0°            │                         │
│                    │                          │                         │
│                    └──────────────────────────┘                         │
│                                                                         │
│    ┌─ STATE ─────────────────────────────────────────────────────┐      │
│    │   ● ● ○ ○     ARMED — Ready. Press RELEASE when ready.      │      │
│    └─────────────────────────────────────────────────────────────┘      │
│                                                                         │
│          [    🔓 RELEASE ARM    ]            [ ✗ ABORT ]                │
│           (large, red, prominent)                                       │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**During MEASURING & COMPLETE:**
```
┌─────────────────────────────────────────────────────────────────────────┐
│ Status Bar (40px)  [state: MEASURING → COMPLETE]                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│                    ┌──────────────────────────┐                         │
│                    │                          │                         │
│                    │      LARGE ARC GAUGE     │                         │
│                    │       (full width)       │                         │
│                    │                          │                         │
│                    │        127.3°            │                         │
│                    │                          │                         │
│                    └──────────────────────────┘                         │
│                                                                         │
│    ┌─ STATE ─────────────────────────────────────────────────────┐      │
│    │   ● ● ● ●     COMPLETE — Test complete. Review results.     │      │
│    └─────────────────────────────────────────────────────────────┘      │
│                                                                         │
│    ┌─ ANGLE ─────┐  ┌─ ENERGY ────┐  ┌─ SPECIMEN ───────────┐         │
│    │   127.3°    │  │   48.2 J    │  │  STEEL-043           │         │
│    └─────────────┘  └─────────────┘  │  Mild Steel 10×10×55 │         │
│                                       └──────────────────────┘         │
│                                                                         │
│          [ ✓ SAVE RESULT ]                [ ✗ DISCARD ]                 │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**Layout breakdown:**
- **Arc gauge** (center, dominant element, ~300px diameter):
  - During ARMING: arc fills as motor lifts arm, color `#FFC107` (amber)
  - During ARMED: static at release angle, color `#00E676` (green), pulsing glow
  - During MEASURING: arc animates rapidly tracking live angle, color `#00D4FF` (cyan)
  - After COMPLETE: arc settles at final angle, color `#00D4FF`
  - Center value: 48px bold, updates live

- **Progress/state indicator** (below gauge, full width):
  - 4-step dot indicator: `ARMING → ARMED → MEASURING → COMPLETE`
  - Filled dots = completed steps (`#00D4FF`), hollow = pending (`#4A5568`), pulsing = current
  - Status message text: 20px `#EAEAEA`
  - Context messages:
    - ARMING: "Lifting arm... Stand clear."
    - ARMED: "Armed — Ready. Press RELEASE when ready."
    - MEASURING: "Measuring swing... Hold steady."
    - COMPLETE: "Test complete. Review results."

- **Action area** (bottom row, content changes per state):

  - **ARMING state**: "ABORT TEST" button only (`#16213E` bg, `#E94560` border/text). Stops motor, returns to Specimen Entry.

  - **ARMED state**: Two buttons side-by-side:
    - "RELEASE ARM" button: **Large and prominent** (280x64px), `#E94560` (red) background, `#EAEAEA` text, 28px bold. Intentionally red to convey consequence — this fires the linear actuator and drops the pendulum. Requires deliberate press.
    - "ABORT" button: smaller, `#16213E` bg, `#E94560` border/text. Motor off, return to idle.
    - The release button triggers: actuator relay ON (brief pulse) → state transitions to MEASURING → high-speed AS5600 sampling begins.

  - **MEASURING state**: No buttons. Screen shows live angle tracking. User should stand clear.

  - **COMPLETE state**: Result cards appear + two buttons:
    - Result cards (3 side-by-side): Angle (`#0F3460` bg, 28px `#00D4FF`), Energy (`#0F3460` bg, 28px `#00E676`), Specimen (`#0F3460` bg, 20px `#EAEAEA`)
    - "SAVE RESULT": `#00E676` bg, `#1A1A2E` text
    - "DISCARD": `#16213E` bg, `#E94560` border/text

---

#### Screen 4: History (1024x560)

```
┌─────────────────────────────────────────────────────────────────────────┐
│ Status Bar (40px)                                                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   TEST HISTORY                                       [ ← Dashboard ]   │
│   ─────────────────────────────────────────────────────────────────────  │
│   ┌─────────┬────────────┬────────────┬─────────┬─────────┬──────────┐ │
│   │  DATE   │ SPECIMEN   │  MATERIAL  │  ANGLE  │ ENERGY  │  STATUS  │ │
│   ├─────────┼────────────┼────────────┼─────────┼─────────┼──────────┤ │
│   │ 04/20   │ STEEL-043  │ Mild Steel │ 127.3°  │  48.2 J │    ✓     │ │
│   │ 12:34   │            │            │         │         │          │ │
│   ├─────────┼────────────┼────────────┼─────────┼─────────┼──────────┤ │
│   │ 04/20   │ STEEL-042  │ Mild Steel │ 131.0°  │  45.8 J │    ✓     │ │
│   │ 11:20   │            │            │         │         │          │ │
│   ├─────────┼────────────┼────────────┼─────────┼─────────┼──────────┤ │
│   │ 04/19   │ ALUM-019   │ Aluminum   │ 142.5°  │  32.1 J │    ✓     │ │
│   │ 16:45   │            │            │         │         │          │ │
│   ├─────────┼────────────┼────────────┼─────────┼─────────┼──────────┤ │
│   │  ...    │   ...      │   ...      │  ...    │  ...    │   ...    │ │
│   └─────────┴────────────┴────────────┴─────────┴─────────┴──────────┘ │
│                                                                         │
│   Showing 47 tests                          [ ◀ Prev ]  [ Next ▶ ]     │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**Layout breakdown:**
- **Header**: "TEST HISTORY" (28px `#EAEAEA`), back button top-right
- **Table** (LVGL `lv_table` or custom list):
  - Header row: `#0F3460` bg, 16px bold `#8892A0` text
  - Data rows: alternating `#1A1A2E` / `#16213E` backgrounds for readability
  - Row height: 56px (comfortable touch target)
  - Angle column: `#00D4FF` text
  - Energy column: `#00E676` text
  - Scrollable vertically (LVGL scroll), shows ~7 rows visible at once
  - Tap row to see detail (optional future feature)
- **Footer**: record count (16px `#8892A0`) + pagination buttons if >50 records

---

#### Screen 5: Settings (1024x560)

```
┌─────────────────────────────────────────────────────────────────────────┐
│ Status Bar (40px)                                                       │
├──────────────────────────────┬──────────────────────────────────────────┤
│                              │                                          │
│  SETTINGS                    │  ┌─ Pendulum Configuration ──────────┐  │
│                              │  │                                    │  │
│  ┌──────────────────────┐   │  │  Mass (kg)      [ 3.950 ]         │  │
│  │  ⚙ Pendulum Config  │◄──│  │  Arm Length (m) [ 0.400 ]         │  │
│  ├──────────────────────┤   │  │  Release Angle  [ 150.0° ]        │  │
│  │  🔧 Calibration      │   │  │                                    │  │
│  ├──────────────────────┤   │  │  These values are used in the      │  │
│  │  🕐 Date & Time      │   │  │  energy calculation formula.       │  │
│  ├──────────────────────┤   │  │                                    │  │
│  │  💾 SD Card          │   │  └────────────────────────────────────┘  │
│  ├──────────────────────┤   │                                          │
│  │  🔊 Audio            │   │              [ Save to NVS ]             │
│  ├──────────────────────┤   │                                          │
│  │  ℹ️ About            │   │                                          │
│  └──────────────────────┘   │                                          │
│                              │                                          │
│         [ ← Dashboard ]     │                                          │
│                              │                                          │
└──────────────────────────────┴──────────────────────────────────────────┘
```

**Layout breakdown:**
- **Left sidebar** (280px wide, `#16213E` bg):
  - Navigation list of settings categories
  - Active item: `#0F3460` bg with `#00D4FF` left border accent (4px)
  - Items: 56px tall, icon + text, 20px `#EAEAEA` (active) / `#8892A0` (inactive)
  - Back button at bottom

- **Right content area** (744px, `#1A1A2E` bg): content changes based on selected category

  - **Pendulum Config panel**:
    - Numeric inputs for mass, arm length, release angle
    - Input: `#16213E` bg, `#2D3748` border, 20px `#EAEAEA` text
    - +/- stepper buttons on each input for fine adjustment
    - Help text below in 16px `#4A5568`
    - "Save to NVS" button: `#00D4FF` bg

  - **Calibration panel**:
    - Current raw AS5600 reading displayed live (16px `#8892A0`)
    - "Set Current Position as Zero" button: `#0F3460` bg, `#00D4FF` text
    - Current zero offset value shown
    - Instructions: "Hang pendulum vertically at rest position, then press calibrate"

  - **Date & Time panel**:
    - Current RTC datetime displayed large (28px `#EAEAEA`)
    - Spinbox/roller inputs for hour, minute, day, month, year
    - "Sync to RTC" button

  - **SD Card panel**:
    - Card status: "Mounted ✓" (green) or "Not detected ✗" (red)
    - Used/free space bar
    - File count on card
    - "Safely Eject" button

  - **Audio panel**:
    - Volume slider (0-100%, maps to WAV volume multiplier)
    - Sound on/off toggle
    - "Test Sound" button — plays a short beep to verify speaker works
    - Lists detected WAV files in `/sdcard/sounds/`

  - **About panel**:
    - App name, version, university/org logo
    - Hardware: CrowPanel ESP32-P4, firmware version
    - Build date

---

#### LVGL Implementation Notes

- **Theme init**: Call `lv_theme_default_init()` with `dark_bg=true`, then override palette colors to match above table
- **Screen transitions**: Use `lv_scr_load_anim()` with `LV_SCR_LOAD_ANIM_FADE_IN`, 200ms duration
- **Touch targets**: Minimum 48x48px for all interactive elements (follows HMI best practice for industrial use)
- **LVGL styles**: Define reusable styles for cards (`style_card`), inputs (`style_input`), buttons (`style_btn_primary`, `style_btn_secondary`, `style_btn_danger`)
- **Arc gauge**: Use `lv_arc` with `lv_anim` for smooth needle movement, update at 30fps from angle sampling task via `lv_msg` or shared atomic variable
- **Keyboard**: Use `lv_keyboard` attached to focused textarea, styled with dark colors
- **File structure**:
  ```
  main/ui/
    ui.h              — Screen declarations, theme init, shared styles
    ui.c              — Theme setup, style definitions, screen manager
    ui_dashboard.c    — Dashboard screen
    ui_specimen.c     — Specimen entry screen
    ui_test_active.c  — Test active screen
    ui_history.c      — History screen
    ui_settings.c     — Settings screen
  ```

---

### Phase 4: Integration

10. **Wire together in `main.c`** — *depends on all above*. Keep existing display/touch init. Add I2C device init for AS5600/DS3231 on shared bus. Mount SD card. Init I2S audio + amplifier control GPIO. Init relay/endstop GPIOs. Create FreeRTOS tasks: `test_manager_task`, `angle_sampling_task` (high priority during swing), `audio_task` (low priority, queue-based), `ui_update_task`. Load settings from NVS.

11. **Update build config** — *depends on step 10*. Update `main/CMakeLists.txt` with new sources and `REQUIRES` for new components (`bsp_audio`). Add new peripheral component directories to build.

---

**Relevant Files**

- `main/main.c` — Add peripheral init, FreeRTOS tasks, app startup logic
- `main/CMakeLists.txt` — Add new source files and dependencies (`bsp_angle`, `bsp_rtc`, `bsp_sdcard`, `bsp_audio`)
- `main/include/main.h` — Add new includes
- `peripheral/bsp_extra/bsp_extra.c` — Add relay and endstop GPIO functions
- `peripheral/bsp_i2c/bsp_i2c.c` — Reuse as-is (400kHz I2C on IO45/IO46)
- `peripheral/bsp_angle/` — **NEW**: AS5600 I2C driver
- `peripheral/bsp_rtc/` — **NEW**: DS3231 I2C driver
- `peripheral/bsp_sdcard/` — **NEW**: SD card FAT32 driver (based on Elecrow `bsp_sd`)
- `peripheral/bsp_audio/` — **NEW**: I2S audio driver (based on Elecrow Lesson12 `bsp_audio`)
- `main/test_manager.c` — **NEW**: Test state machine
- `main/audio_manager.c` — **NEW**: Audio task with FreeRTOS queue for non-blocking sound playback
- `main/data_logger.c` — **NEW**: CSV logging module
- `main/include/charpy_calc.h` — **NEW**: Energy calculation
- `main/ui/*` — **REPLACE**: All 5 new LVGL screens (hand-coded, replacing SquareLine-generated demo)
- `/sdcard/sounds/*.wav` — **NEW**: Notification sound files (16kHz, 16-bit, mono PCM WAV, ~1-2 sec each)

---

**Verification**

1. Read AS5600 angle on serial, rotate magnet, confirm degree output and I2C coexistence with touch + RTC
2. Set DS3231 time, power cycle, confirm persistence
3. Mount SD card, write CSV, verify file contents on PC
4. Toggle relay GPIOs, verify with multimeter/LED
5. Trigger endstop switch, confirm ISR + debounce
6. Unit test energy calc with known values (e.g. 4kg, 0.4m arm, α=150°, β=120°)
7. Run full test cycle end-to-end: specimen entry → arm lift → endstop → release → measure → display → log
8. Audio: init I2S, play test WAV from SD card, verify speaker output and amplifier on/off via IO30
9. Sound notifications: trigger each state sound event, verify correct WAV plays without blocking UI or state machine
10. Run 20+ consecutive tests — verify no memory leaks, stable I2C bus, correct SD writes, audio plays correctly each time

---

**Decisions**

- AS5600 confirmed on shared I2C bus — no address conflicts (0x36 vs 0x14 vs 0x68)
- Motor/actuator controlled via relay with digital override from ESP32-P4 (IO47/IO48)
- Manual trigger workflow — touchscreen triggers arming sequence, operator controls physical timing
- Extended CSV data logging with specimen info
- Energy calculated from angle using configurable pendulum parameters in NVS
- Existing demo UI (Gauges/Data) fully replaced — not relevant to Charpy testing
- IO34 avoided (conflicts with DSI_REXT)
- **UI hand-coded in C** (recommended over SquareLine Studio for data-driven screens)
- **Audio via I2S** — Built-in speaker with power amplifier (IO30 active-low control). I2S1 master mode, 16kHz/16-bit. Reference: Elecrow Lesson12 `bsp_audio` component. Non-blocking playback via dedicated FreeRTOS audio task with queue.
- **Sound files on SD card** — WAV format (PCM 16-bit mono 16kHz), stored in `/sdcard/sounds/`. Short notification clips (~1-2 sec). Users can replace WAV files to customize sounds.

---

**Further Considerations**

1. **Angle profile logging** — Currently excluded. The AS5600 can sample at ~1kHz during the swing. A future enhancement could buffer the full angle-vs-time curve in PSRAM and save it to SD card for detailed analysis.
2. **AS5600 calibration** — A one-time "Calibrate Zero" button in Settings that snapshots the current angle as the reference (pendulum hanging vertical) is needed. Should this be part of initial implementation? **Recommendation: yes, include it.**
3. **Physical start button vs. touchscreen-only** — Current plan uses touchscreen to trigger. If you want a hardwired emergency-stop or dedicated start button, we'd need one more GPIO. The screen-only approach keeps wiring simpler.
