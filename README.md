# 818 APRS Transceiver

APRS transceiver for Flipper Zero using DRA818V/SA818V external VHF/UHF radio modules. Full TX and RX with Bell 202 AFSK at 1200 baud.

By [LU3ARN](https://www.qrz.com/db/LU3ARN). Based on [flipper-ham](https://github.com/yo3gnd/flipper-zero-aprs-tx) by [YO3GND](https://www.qrz.com/db/YO3GND).

## What it does

- **TX**: Sends APRS messages, status packets, bulletins, and position reports on VHF/UHF
- **RX**: Receives and decodes APRS packets in real-time with on-screen display
- **Supported modules**: DRA818V (VHF), DRA818U (UHF), SA818V, SA818U — any module with the standard AT command interface
- **Preset frequencies**: 144.390 (NA), 144.800 (EU), 145.175 (AU), 144.640 (JP), 144.660 (CN), 145.525 (NZ), 432.500 (70cm)

## Hardware required

- Flipper Zero
- DRA818V/SA818V module (or U variant for UHF)
- A few passive components (resistors, capacitors)
- VHF/UHF antenna appropriate for your frequency
- External 3.3-5V power supply for the module (draws ~400mA on TX)
- **Ham radio license** for your jurisdiction

## Wiring

```
Flipper Zero GPIO              DRA818V/SA818
============================   ================
TX  (PB6)  ─────────────────── RXD
RX  (PB7)  ─────────────────── TXD
B3  (PB3)  ─────────────────── PTT  (active LOW)
B2  (PB2)  ─────────────────── PD   (HIGH = power on)
GND        ─────────────────── GND  (common ground)

Audio TX (Flipper to module):
A4  (PA4)  ──[10k]──[100nF]── MIC+
                                MIC- ── GND

Audio RX (module to Flipper):
                     ┌─[10k]── 3V3
SPK+ ──[100nF cap]──┤
                     ├──────── A6 (PA6)
                     └─[10k]── GND
                     SPK- ──── GND

Power:
External 3.3-5V ────────────── VCC
GND (shared)    ────────────── GND
H/L pin         ────────────── leave open (1W) or GND (0.5W)
```

The 10k/10k voltage divider on the RX audio biases the ADC input at 1.65V. Use 10k resistors (not higher) to keep the source impedance within the ADC's specification.

## Build

```sh
pip install ufbt
ufbt update
ufbt
ufbt launch    # deploy and run on connected Flipper
```

The app appears under **Tools** on the Flipper as **818 APRS Transceiver**.

## Menu

- **Send** — compose and transmit messages, positions, status, bulletins
- **RX Listen** — live APRS packet decoder with scrollable message history
- **Settings** — VHF frequency, APRS path, volume, squelch, repeat count, lead-in/preamble timing, debug TX/RX
- **Callbook** — destination callsign list
- **Ham Radio** — your callsign and SSID for TX
- **About** — version and credits

## Settings

| Setting | Range | Description |
|---------|-------|-------------|
| VHF Freq | Presets | APRS frequency for your region |
| APRS Path | None/WIDE1-1/WIDE2-2/etc | Digipeater path |
| Repeat TX | 1-5 | Number of transmission repeats |
| Lead-in | 0-1000 ms | Mark tone before preamble |
| Preamble | 0-1000 ms | Flag bytes before data |
| Volume | 1-8 | DRA818V audio output level |
| Squelch | 0-8 | RX squelch threshold (0=open) |
| Debug TX | Yes/No | Show packet details during TX |
| Debug RX | Yes/No | Show ADC/CRC/flag stats during RX |

## RX navigation

- **Up/Down** — scroll within a long message
- **Left/Right** — navigate between received messages
- **Back** — exit RX mode

Green LED flashes on successful decode, red on CRC failure.

## Ham usage

Create `/ext/ham/my-callsigns.txt` on the SD card:

```
LU3ARN-9,12345
```

Format: `CALLSIGN[-SSID],PASSCODE` — one per line. The SSID and IS passcode authenticate your transmissions. Select your callsign under **Ham Radio** in the menu.

## How it works

**TX**: The Flipper generates the AX.25 packet (addressing, bit stuffing, NRZI, CRC) and outputs the AFSK waveform as a square wave on GPIO pin A4. The DRA818V's FM modulator transmits it on the configured VHF/UHF frequency.

**RX**: TIM2 hardware timer triggers ADC conversions at exactly 13200 Hz via TRGO. An ISR collects samples into a circular buffer. A worker thread runs a delay-and-multiply discriminator with IIR low-pass filtering, clock recovery, and AX.25 frame assembly. Decoded APRS packets are displayed on screen.

## Notes

- Only transmit where you are legally allowed to do so
- The DRA818V/SA818V draws significant current on TX — power it from an external supply, not the Flipper
- Keep antennas close for bench testing; 0.5-1W is enough for local APRS with a proper antenna
- RX decode rate is ~100% with a clean signal and correct settings

## Credits

- [YO3GND](https://github.com/yo3gnd/flipper-zero-aprs-tx) — original flipper-ham APRS TX app, AFSK waveform generator, AX.25 packet construction
- [LU3ARN](https://github.com/reynico/flipper-818-aprs) — DRA818V integration, VHF/UHF support, RX demodulator, UI overhaul

## License

See [LICENSE](LICENSE) for terms.
