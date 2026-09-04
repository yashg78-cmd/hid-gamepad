# ESP32-S3 USB HID Gamepad

A custom USB HID gamepad controller built around the ESP32-S3, running bare-metal C over ESP-IDF with a TinyUSB HID backend. Two hall effect analog joysticks (each with a clickable Z-axis switch), 8 face/shoulder/menu buttons, and a 4-direction D-pad are read directly and packed into a standard TinyUSB gamepad HID report — no custom host-side driver needed. Hardware is a custom KiCad PCB (`hid_controller`).

## How It Works

1. **Joysticks (`joystick_read`)** — each of the two sticks reports X/Y over the ADC (`adc_oneshot`) and a click (Z) via its own GPIO switch. Raw 12-bit ADC readings (0–4095) are recentered and scaled to the signed 8-bit range HID expects: `(raw - 2048) * 127 / 2048`.
2. **Face/shoulder/menu buttons (`buttons_read`)** — A, B, X, Y, LB, RB, Start, and Select are each a pulled-up GPIO, active-low, packed into an 8-bit bitmask. The two joystick clicks are OR'd in afterward as bits 8 and 9.
3. **D-pad (`dpad_read`)** — four directional GPIOs are combined into a single HID hat-switch value, including the four diagonals (up-right, up-left, down-right, down-left).
4. **`controller_send`** assembles all of the above into one `tud_hid_gamepad_report()` call (triggers unused, always 0), sent over TinyUSB.
5. **TinyUSB HID** — configured as `TUD_HID_REPORT_DESC_GAMEPAD`, exposed over a full-speed USB port, so the host enumerates it as a stock USB gamepad.
6. **Main loop** polls and sends a report every 10ms once the USB device is mounted (`tud_mounted()`).

## Controls

| Input | Pins / Channels |
|---|---|
| Left joystick X / Y | ADC_CHANNEL_2 (GPIO3) / ADC_CHANNEL_8 (GPIO9) |
| Left joystick click | GPIO5 |
| Right joystick X / Y | ADC_CHANNEL_0 (GPIO1) / ADC_CHANNEL_1 (GPIO2) |
| Right joystick click | GPIO13 |
| A / B / X / Y | GPIO38 / GPIO39 / GPIO40 / GPIO21 |
| LB / RB | GPIO7 / GPIO41 |
| Start / Select | GPIO48 / GPIO15 |
| D-pad Up / Down / Left / Right | GPIO16 / GPIO8 / GPIO18 / GPIO17 |

All buttons and D-pad pins are configured as active-low inputs with internal pull-ups — no external pull-up resistors needed.

## Hardware

- Custom PCB, designed in KiCad (`hid_controller.kicad_pcb` / `.kicad_sch`)
- ESP32-S3 as the MCU and native USB HID device
- Two hall effect joysticks with integrated push-switches
- 12 discrete buttons (8 face/shoulder/menu + 4 D-pad)

## Software

- **C** on **ESP-IDF**, using `adc_oneshot` for stick input and `driver/gpio` for buttons
- **TinyUSB** (`tinyusb.h`, `class/hid/hid_device.h`) for the USB HID gamepad class and descriptor set
- Single-file application (`joystick.c`) — no RTOS task beyond the default `app_main` loop

## Setup

1. Build and flash with ESP-IDF: `idf.py build flash monitor`.
2. Wire the joysticks and buttons per the pin table above (or use the provided PCB).
3. Plug into a host PC over USB — it enumerates as a standard HID gamepad, no drivers required.
4. Test in any gamepad-aware app (e.g. a browser gamepad tester or game).

## Design Notes

- ADC scaling recenters the joystick's 0–4095 raw range around a 2048 midpoint and maps it into HID's signed 8-bit axis range, so no separate calibration step is needed as long as the stick is centered at rest.
- Joystick clicks and D-pad diagonals are handled entirely at the bit/logic level rather than through extra hardware — the D-pad's 4 GPIOs produce 8 possible hat-switch positions.
- Left/right triggers are wired into the report structure but currently unused (sent as 0), leaving room to add analog trigger input later without changing the report format.
