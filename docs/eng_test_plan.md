# `examples/eng_test` Engineering Test Program Plan

## Summary
- Add a new standalone entry point `examples/eng_test` under `examples/`, dedicated to engineering acceptance and quick hardware checks. `examples/factory` stays untouched and remains the existing demo and debug program.
- The UI is a Chinese-first black-and-white EPD test interface. The main flow is a "guided sequential test", with a "deep verification" entry point kept alongside it, covering both needs: quick acceptance plus in-depth testing of external links.
- Supports the case where some modules are not fitted: before testing starts, the operator unchecks the modules that are absent, and those items are skipped automatically for that session. During testing, every page still offers a temporary `Skip`.
- Results are shown on screen and on the serial port at the same time. Manual items explicitly record `PASS/FAIL`, and every untested item is uniformly recorded as `SKIP`.

## Implementation Changes
- Architecture: add a lightweight test framework defining a common `TestCaseId`, `TestStatus`, `TestMode` and `TestCaseDescriptor`. Each test item is organised around `setup / poll / render / handle_input / teardown` callbacks.
- State model: use the five states `NOT_RUN / RUNNING / PASS / FAIL / SKIP`. "Not fitted" and "manually skipped" are no longer distinguished — both fall under `SKIP` — but the serial log can carry the reason as text for troubleshooting.
- Pre-configuration flow: on boot, enter the "module selection page", which lists the testable modules with everything selected by default. Once the operator unchecks the modules that are not fitted, the real test run begins. Unselected modules are recorded as `SKIP` and skipped outright in the sequential flow.
- Runtime flow: every test page provides a fixed set of `Pass / Fail / Retry / Skip / Next` controls. Modules already marked as not fitted do not open a test page at all, so the runtime `Skip` is mainly for abnormal conditions or items that cannot currently be verified.
- Reuse strategy: `v1` does no repo-wide shared-layer refactor. The already stable board-level initialisation, touch adaptation, peripheral wrappers and EPD/LVGL refresh logic are copied and trimmed from `factory` directly into `examples/eng_test`, prioritising delivery speed.
- Hardware scheduling: the test runner centrally manages the XL9555 power domains, A7682E/ES8311 audio switching, the amplifier enable, and chip-select mutual exclusion on the SPI bus shared by EPD/SD/LoRa. By default, power is only enabled for the module currently under test, reducing interference.
- Page structure: includes the "module selection page", "power-on self-test page", "Quick Test sequential page", "Deep Diagnostics menu", "Final Summary page" and "Sleep Resume page". Page transitions use a full refresh; status updates use a partial refresh.

## Interfaces And Test Items
- The new core interfaces are confined to the test program itself: the test registry, the test runner, the result recorder, the module enable bitmap, and the RTC/NVS resume state structure. Existing board-level public interfaces are unchanged.
- The module selection page can enable/disable the following entries: EPD, Touch, Keypad, GPS, ES8311, Battery, Motor, IMU, Wi-Fi, SD, A7682E, LoRa, BLE, Sleep.
- Mandatory Quick Test items:
  - Display: black/white/inverted patterns plus a partial-refresh region counter. The operator confirms there is no image corruption, no dead lines, and that partial refresh works.
  - Touch: targets at the four corners and the centre; passes automatically once all are hit.
  - Keypad: the quick test only requires pressing a representative set of keys covering every row and column. Full matrix verification is left to the deep test page.
  - Battery: initialise the SY6970/BQ27220, read voltage, charge level and charging status, and check the values are within a sensible range.
  - SD: mount, create a temporary file, read it back and verify, then delete the temporary file.
  - Audio: switch to the ES8311 path, enable the amplifier and play the built-in WAV. The operator confirms the sound is audible.
  - Motor: play 2-3 haptic effects in sequence. The operator confirms the vibration is clearly noticeable.
  - IMU: prompt the operator to tilt the device; passes automatically once the orientation/gyroscope change exceeds a threshold.
  - Wi-Fi: complete one scan call and display the number of APs found. A successful scan returning 0 results is not treated as a failure.
  - GPS basic: confirm the module powers up, the UART works, and NMEA/UBX activity is received within the timeout.
  - 4G basic: the A7682E powers up and responds to `AT`. `CPIN` and `CSQ` are shown for information only and are not a precondition for passing the quick test.
  - LoRa basic: SX1262 initialisation, parameter configuration, and a working transmit-complete interrupt.
  - BLE basic: start a fixed test advertisement and complete one scan start/stop cycle, verifying only the BLE stack and its basic capability.
  - Sleep: write the test progress to RTC/NVS and enter deep sleep. After waking, identify the wake reason, restore to the summary page and mark this item as passed.
- Deep Diagnostics items:
  - GPS: satellite count, time, latitude/longitude, speed and fix status.
  - 4G: SIM status, signal, registration status, an AT console, and optional dial/call testing.
  - LoRa: joint transmit/receive testing against a peer board, displaying RSSI/SNR.
  - BLE: display nearby scan results and prompt the operator to verify the fixed advertising name and advertising data with a phone.
  - Audio: add SD WAV playback and audio path switching tests.
  - Keypad: full key matrix verification.
  - Shutdown: treated as a separate destructive test, kept out of the default quick test loop.

## Test Plan
- Add an optional `src_dir = examples/eng_test` in PlatformIO without replacing the current default `examples/factory`. Decide whether to switch the default entry point once the new program is stable.
- Verify that the "module selection page" correctly affects the sequential flow: unselected modules run no initialisation or action tests and are recorded as `SKIP` in the final summary.
- Verify that the quick test flow completes within 3 minutes on a working sample, and requires no phone, no LoRa peer, no outdoor GPS fix and no cellular registration.
- Verify that every test item emits start/end logs on the serial port, and that the final summary includes the totals for tests run, passed, failed and skipped, plus the list of failed/skipped items.
- Verify state restoration after the sleep test, making sure earlier test results are not lost after waking.
- Regression-verify the shared hardware constraints: EPD/SD/LoRa SPI mutual exclusion, touch reset routed through the XL9555, correct audio routing, and the A7682E only entering the real test when the battery is connected.

## Assumptions
- `v1` targets a primarily Chinese operator interface, with serial logs kept stable and concise so an automated collection tool can be attached later.
- `factory` is kept as the old demo/debug program, while `eng_test` focuses on engineering acceptance. No large-scale extraction of shared code happens in this round.
- Modules that are not fitted are unchecked manually before testing starts. That selection stays valid for the current session and is not required to persist across reboots.
- The basic A7682E test requires the battery to be connected by default. The GPS deep test requires outdoor use or a good view of the sky. The LoRa deep test needs a second device. The BLE deep test can use a phone to verify external visibility.
- The shutdown test is not part of the default quick-test pass criteria, because it interrupts the session. The sleep test is included in the default quick test, because it can wake, restore and continue automatically.
