# Brew Machine MkIV — Pi 5 wiring guide

Everything you need to wire one of these from scratch. Covers the Pi 5
40-pin pinout, the safety chain (the most important bit), per-subsystem
hookups, and a bill of materials.

If you're new to this rig: read [Safety chain](#safety-chain) first. The
software fail-safes only catch software faults. Hardware fail-safes catch
the rest.

## Contents

1. [GPIO assignment summary](#gpio-assignment-summary)
2. [Pi 5 40-pin header](#pi-5-40-pin-header)
3. [Safety chain](#safety-chain)
4. [DS18B20 temperature sensors](#ds18b20-temperature-sensors)
5. [HLT level switches](#hlt-level-switches)
6. [Limit switches](#limit-switches)
7. [Flow sensor](#flow-sensor)
8. [Discrete valves + pumps + mill (relay board)](#discrete-loads)
9. [Crane + boil valve (H-bridge)](#h-bridge-loads)
10. [HLT SSR](#hlt-ssr)
11. [Boil SSR (hardware PWM)](#boil-ssr)
12. [External hardware watchdog](#external-hardware-watchdog)
13. [Power supplies](#power-supplies)
14. [Bill of materials](#bill-of-materials)
15. [Boot config](#boot-config)

---

## GPIO assignment summary

| BCM | Pin | Signal              | Dir  | External device                          |
|-----|-----|---------------------|------|------------------------------------------|
| 2   | 3   | `BOIL_VALVE_OPEN`   | OUT  | H-bridge relay 1 (motorised valve)       |
| 3   | 5   | `BOIL_VALVE_CLOSE`  | OUT  | H-bridge relay 2                         |
| 4   | 7   | `1-WIRE` bus        | bus  | DS18B20 temperature sensors (HLT + MASH) |
| 5   | 29  | `HLT_VALVE`         | OUT  | Solenoid valve (drain)                   |
| 6   | 31  | `MASH_VALVE`        | OUT  | Solenoid valve (mash → boil routing)     |
| 7   | 26  | `HLT_LEVEL_MID` ⚠   | IN   | HLT mid-level float (NC to GND)          |
| 8   | 24  | `HLT_LEVEL_HIGH` ⚠  | IN   | HLT high-level float (NC to GND)         |
| 9   | 21  | `BOIL_VALVE_OPENED` | IN   | Boil valve open-limit switch             |
| 10  | 19  | `BOIL_VALVE_CLOSED` | IN   | Boil valve closed-limit switch           |
| 11  | 23  | `CRANE_UPPER_LIMIT` | IN   | Crane up limit switch                    |
| 12  | 32  | `BOIL_SSR PWM` ⚠    | OUT  | Boil heater SSR (1 Hz hardware PWM)      |
| 13  | 33  | `CRANE_LOWER_LIMIT` | IN   | Crane down limit switch                  |
| 14  | 8   | `HOP_DROPPER_LIMIT` | IN   | Hop dropper rotary limit                 |
| 15  | 10  | `BOIL_LEVEL`        | IN   | Boil kettle low-level float (optional)   |
| 16  | 36  | `INLET_VALVE` ⚠     | OUT  | Solenoid valve (mains water inlet)       |
| 17  | 11  | `CHILLER_VALVE`     | OUT  | Solenoid valve (chiller water)           |
| 18  | 12  | `FLOW_PULSE`        | IN   | Hall-effect flow sensor                  |
| 19  | 35  | `HOP_DROPPER`       | OUT  | Hop dropper motor relay                  |
| 20  | 38  | `MASH_PUMP`         | OUT  | Mash pump relay                          |
| 21  | 40  | `CHILLER_PUMP`      | OUT  | Chiller pump relay                       |
| 22  | 15  | `MILL`              | OUT  | Grain mill motor relay                   |
| 23  | 16  | `HLT_SSR` ⚠         | OUT  | HLT heater SSR (digital on/off)          |
| 24  | 18  | `CRANE_UP`          | OUT  | Crane H-bridge relay 1                   |
| 25  | 22  | `CRANE_DOWN`        | OUT  | Crane H-bridge relay 2                   |
| 26  | 37  | `WATCHDOG_KICK` ⚠   | OUT  | External WDT IC (TPS3823 / ATtiny)       |
| 27  | 13  | `STIR`              | OUT  | Stir motor relay                         |

⚠ = safety-critical. The four `null` ones in `pinmap.ts` (`HLT_SSR`,
`INLET_VALVE`, `HLT_LEVEL_HIGH`, `HLT_LEVEL_MID`) refuse to boot until
assigned. Recommended BCM is shown above; copy those into `pinmap.ts`
once you've wired them.

**Power / ground:**

- 3V3 rails on pins **1** and **17** (~50 mA budget each)
- 5V rails on pins **2** and **4** (limited by the Pi 5 PSU; ~1 A spare)
- GND on pins **6, 9, 14, 20, 25, 30, 34, 39** — use multiple

---

## Pi 5 40-pin header

```
                ┌─────────────────────┐
        3V3 ──● │  1   │   2  │ ●── 5V
   SDA  BCM2 ─● │  3   │   4  │ ●── 5V          ← BOIL_VALVE_OPEN
   SCL  BCM3 ─● │  5   │   6  │ ●── GND         ← BOIL_VALVE_CLOSE
        BCM4 ─● │  7   │   8  │ ●── BCM14       ← 1-Wire bus | HOP_DROPPER_LIMIT
        GND ──● │  9   │  10  │ ●── BCM15       ← (gnd)      | BOIL_LEVEL
       BCM17 ─● │ 11   │  12  │ ●── BCM18       ← CHILLER_VALVE | FLOW_PULSE
       BCM27 ─● │ 13   │  14  │ ●── GND         ← STIR
       BCM22 ─● │ 15   │  16  │ ●── BCM23       ← MILL | HLT_SSR ⚠
        3V3 ──● │ 17   │  18  │ ●── BCM24       ←      | CRANE_UP
       BCM10 ─● │ 19   │  20  │ ●── GND         ← BOIL_VALVE_CLOSED
       BCM9  ─● │ 21   │  22  │ ●── BCM25       ← BOIL_VALVE_OPENED | CRANE_DOWN
       BCM11 ─● │ 23   │  24  │ ●── BCM8        ← CRANE_UPPER_LIMIT | HLT_LEVEL_HIGH ⚠
        GND ──● │ 25   │  26  │ ●── BCM7        ←      | HLT_LEVEL_MID ⚠
       ID_SD ─● │ 27   │  28  │ ●── ID_SC       ← DO NOT USE — HAT EEPROM
       BCM5  ─● │ 29   │  30  │ ●── GND         ← HLT_VALVE
       BCM6  ─● │ 31   │  32  │ ●── BCM12       ← MASH_VALVE | BOIL_SSR ⚠ (PWM)
       BCM13 ─● │ 33   │  34  │ ●── GND         ← CRANE_LOWER_LIMIT
       BCM19 ─● │ 35   │  36  │ ●── BCM16       ← HOP_DROPPER | INLET_VALVE ⚠
       BCM26 ─● │ 37   │  38  │ ●── BCM20       ← WATCHDOG_KICK ⚠ | MASH_PUMP
        GND ──● │ 39   │  40  │ ●── BCM21       ←      | CHILLER_PUMP
                └─────────────────────┘
```

> **BCM 0/1 (pins 27/28)** are the HAT EEPROM ID lines. Never use them as
> GPIO; you can brick the Pi's HAT auto-detection.

> **GPIO 14/15 (UART)** are repurposed as GPIO here (`HOP_DROPPER_LIMIT`
> and `BOIL_LEVEL`). You lose the serial console — use SSH or HDMI
> instead.

---

## Safety chain

**This section is mandatory.** A 5 kW heating element with
`writeOutput(0)` as its only off-switch is indefensible. Wire an
electromechanical kill path that the Pi cannot override.

Four NC (normally-closed) contacts and one watchdog-IC OK line in
series, all driving the contactor coil that gates mains to your SSRs:

```
                          ┌──── 230 V L (mains in) ─────┐
                          │                              │
                          │     Mains contactor          │
                          │      ┌──┐    ┌──┐            │
                          │   ───┤  ├────┤  ├─── L (load to SSRs)
                          │      └──┘    └──┘            │
                          │                              │
                          │                              │
                       Coil A1                        Coil A2
                          │                              │
              ┌───────────┴────── chain in series ───────┴──────────┐
              │                                                       │
              ●─── E-stop button (NC, mushroom, twist-release) ────●
              │                                                       │
              ●─── HLT low-level float (NC: opens when dry) ────────●
              │                                                       │
              ●─── Klixon thermal cutoff on HLT (NC, 90-95 °C) ─────●
              │                                                       │
              ●─── External WDT IC OK output (TPS3823 RESET) ───────●
              │                                                       │
              └────────── 24 V or 230 V coil supply ─────────────────┘
                          (matches your contactor coil)
```

**Behaviour:**

- Any one contact opens → contactor coil drops → mains is disconnected
  from BOTH heater SSRs simultaneously. The Pi cannot override.
- The Pi only controls the SSRs **downstream** of the contactor (PWM
  gating for the boil, on/off for the HLT). When the contactor is open
  they're dead regardless of GPIO state.
- The WDT IC's OK line stays high **only while the Pi keeps toggling**
  `WATCHDOG_KICK` (BCM 26) every ≤ ~500 ms. If the Pi crashes,
  deadlocks, GC-stalls, or has its USB-C yanked, the WDT IC drops the
  chain after ~500 ms.

**Wiring tips:**

- NC contacts in *series*, not parallel. "Any one failure breaks the
  chain" is what you want.
- Float switches: buy NC-type (some vendors label "horizontal NC" or
  "level → high opens"). NO contacts wire the same shape but invert the
  semantics — avoid.
- Klixon: a ~$5 thermal cutoff bonded to the HLT wall. 90–95 °C bimetal
  NC is the brewing sweet spot.
- E-stop: 22 mm mushroom button, NC, twist-to-release. Mount where
  you'd reach during a panic — typically right next to the boil kettle.
- Contactor: pick coil voltage to match your control supply. 24 V DC is
  clean (small DC PSU + DPST-NO contactor).

---

## DS18B20 temperature sensors

Wired in 3-wire mode (more reliable than parasite-power):

```
   Pi 3V3 (pin 1) ───────●────────────────────●───────────────●
                         │                    │               │
                       ┌─┴─┐ 4.7 kΩ           │               │
                       │   │                   │               │
                       └─┬─┘                   │               │
                         │                    │               │
   BCM 4 (pin 7) ────────●────────────────────●───────────────●─── ... more
                                              │               │
                                          ┌───┴───┐       ┌───┴───┐
                                          │ HLT   │       │ MASH  │
                                          │ probe │       │ probe │
                                          └───┬───┘       └───┬───┘
   Pi GND (pin 6) ──────────────────────────●─────────────────●
```

- All DS18B20s share **one** bus. Just gang them onto BCM 4. The kernel
  enumerates by ROM code.
- **One** 4.7 kΩ pull-up on the whole bus, between BCM 4 and 3V3 — not
  per-sensor.
- Bus length: ≤ 5 m with normal cable; longer runs need proper twisted
  pair.
- **Identify each probe in the UI**: Diagnostics tab → "Scan bus" →
  drop a probe in warm water → see which ROM's temperature jumps → click
  **Assign to HLT** (or MASH). Persists to `data/onewire-overrides.json`.

---

## HLT level switches

Float switches: NC contacts to GND, internal pull-up enabled in
software. "Wet" = float lifted = contact closed = pin reads LOW.

```
   Pi BCM 7 (pin 26) ────►─── HLT_LEVEL_MID ──── float switch (NC) ─── GND
   Pi BCM 8 (pin 24) ────►─── HLT_LEVEL_HIGH ─── float switch (NC) ─── GND
```

**The HLT LOW float** also wires into the safety chain (above) as an NC
contact. That is the *hardware* interlock against dry-firing. The Pi
also reads `HLT_LEVEL_MID` / `HLT_LEVEL_HIGH` for the *software*
interlock + fill control, but the contactor-coil chain is what catches
"Pi misbehaves while the tank is empty".

---

## Limit switches

All wire identically to HLT level — NC to GND with software pull-up:

| Pin       | Signal              | Where                                |
|-----------|---------------------|--------------------------------------|
| BCM 11    | `CRANE_UPPER_LIMIT` | Top of crane travel                  |
| BCM 13    | `CRANE_LOWER_LIMIT` | Bottom of crane travel               |
| BCM 9     | `BOIL_VALVE_OPENED` | Boil valve full-open detent          |
| BCM 10    | `BOIL_VALVE_CLOSED` | Boil valve full-closed detent        |
| BCM 14    | `HOP_DROPPER_LIMIT` | Hop dropper rotary cup-gap sensor    |
| BCM 15    | `BOIL_LEVEL`        | Boil kettle low-level (optional)     |

Use mechanical microswitches or proximity sensors with NC contacts.

---

## Flow sensor

Hall-effect (YF-S201, FS300A, similar) — three wires (V+, GND, signal):

```
   Sensor +5V  ──── Pi 5V (pin 2 or 4)
   Sensor GND  ──── Pi GND
   Sensor OUT  ──── BCM 18 (pin 12) — FLOW_PULSE
```

**5V signal warning:** the YF-S201 is open-collector but some clones
drive the output to V+ (5 V). The Pi 5 is **NOT 5V-tolerant on GPIO**.
If yours drives 5 V, put a level shifter or a 1 kΩ + 2 kΩ divider on
the OUT line. Easiest to verify with a meter before connecting.

Counted in a worker thread (`platform/hal/workers/flow-worker.ts`) so
Node GC pauses can't drop pulses. Default calibration is ~430 pulses/L
(see `flow.ts:LITRES_PER_PULSE_LOW`). Recalibrate by passing 1 L through
and comparing the reported volume.

---

## Discrete loads

Single-action loads (valves, pumps, mill, stir, hop dropper, HLT SSR)
drive an **opto-isolated relay board** from the Pi GPIO. Buy one with at
least 16 channels (e.g. SainSmart 16-relay, or 2 × ELEGOO 8-relay) for a
uniform setup.

```
   Pi BCM x   ────► IN x   ┌────────────────┐
   Pi 5V      ────► VCC    │  Relay board   │
   Pi GND     ────► GND    │ (opto-isolated)│
                            │                │
                            │  COM/NO/NC ────┼──── load
                            └────────────────┘
```

**Active-low vs active-high:** check your board. Most opto-isolated
boards are **active-LOW** (writing 0 energises the relay). The code's
`writeOutput(name, 1)` opens / starts the load. If your board is
active-LOW the easiest fix is to invert at the board (most have a
JD-VCC / VCC jumper that swaps logic) rather than touching the code.

| BCM | Signal          | Switching                                          |
|-----|-----------------|----------------------------------------------------|
| 5   | `HLT_VALVE`     | 12/24 V solenoid coil to GND                       |
| 6   | `MASH_VALVE`    | 12/24 V solenoid coil                              |
| 16  | `INLET_VALVE` ⚠ | 12/24 V solenoid coil — overflow risk if stuck on  |
| 17  | `CHILLER_VALVE` | 12/24 V solenoid coil                              |
| 19  | `HOP_DROPPER`   | DC motor (12 V typ.) — flyback diode!              |
| 20  | `MASH_PUMP`     | Pump motor                                         |
| 21  | `CHILLER_PUMP`  | Pump motor                                         |
| 22  | `MILL`          | Mill motor (often 230 V AC — wire to its own relay)|
| 23  | `HLT_SSR` ⚠     | SSR DC control input (3-32 V DC, ~10 mA)           |
| 27  | `STIR`          | Stir motor (often 230 V AC)                        |

> **DC motor flyback diodes:** every DC motor coil (pumps, hop dropper,
> crane motor, boil-valve actuator) needs a flyback diode across its
> terminals (cathode to V+, anode to GND-side) to clamp the inductive
> spike when the relay opens. Without it your relay contacts arc and
> the GPIO can see a voltage spike. Most relay boards have these on the
> load side already; verify.

---

## H-bridge loads

The crane and the motorised boil valve are bidirectional DC actuators
driven by **two** relays each. The rule: never assert both at once. The
code enforces this (`crane.ts#drive()`, `boilValve.ts#drive()`), but
also wire the relays so the at-rest state brakes the motor:

```
   Crane motor:    M+   M-
                   │     │
           ┌───────┘     └───────┐
           │                     │
   +24V ── CRANE_UP relay COM    CRANE_DOWN relay COM
           │                     │
       NO  │                  NO │
           └──── M+              └──── M-
       NC  │                  NC │
           └──── GND              └──── GND
```

Both relays at rest: M+ and M- to GND → motor braked.
`CRANE_UP` asserts: M+ → +24V, M- → GND → motor up.
`CRANE_DOWN` asserts: M+ → GND, M- → +24V → motor down.
Both asserted (bug): +24V on both → motor doesn't turn (still safe-ish).

Boil valve actuator wires identically (`BOIL_VALVE_OPEN` +
`BOIL_VALVE_CLOSE`).

---

## HLT SSR

Solid-state relay for the HLT element. Digital on/off — no PWM. The
original C code didn't PWM the HLT either; it's a thermostat loop with
hysteresis.

```
   Pi BCM 23 (pin 16) ──── SSR + (3-32 V control input)
   Pi GND             ──── SSR -

   SSR output (mains side) wired in series with the safety contactor:

     230 V L ─── contactor (NC of) ─── SSR ─── HEATER ─── N
```

The contactor is **upstream** of the SSR. If the safety chain breaks,
the contactor opens, and the SSR has nothing to switch. If the SSR
fails shorted (a real failure mode), the contactor is the second line
of defence.

---

## Boil SSR

Same physical wiring as HLT SSR but driven by **hardware PWM**
(BCM 12 / pin 32). Requires the `pwm-2chan` dtoverlay (see
[Boot config](#boot-config)).

The PWM period is 1 Hz (very slow — fine for resistive heaters; thermal
mass averages it out). 60 % duty = 600 ms on, 400 ms off per second.
The code **refuses to fall back to software PWM** on real hardware
because event-loop jitter would cause 10–20 % duty error per cycle
(Architect 4's call from the design review).

Verify hardware PWM is actually claimed:

```bash
ls /sys/class/pwm/pwmchip0           # should exist
echo 0 | sudo tee /sys/class/pwm/pwmchip0/export
ls /sys/class/pwm/pwmchip0/pwm0      # period, duty_cycle, enable
```

If `pwmchip0` doesn't exist, the dtoverlay isn't loaded. Reboot after
editing `/boot/firmware/config.txt`.

---

## External hardware watchdog

```
   Pi BCM 26 (pin 37) ───► WDI input   ┌─────────────┐
   Pi 3V3             ───► VCC          │   TPS3823   │
   Pi GND             ───► GND          │  (or ATtiny)│
                                         │             │
                                         │ /RESET ─────┼──── safety chain
                                         │ (logic high │
                                         │  while being│
                                         │  kicked)    │
                                         └─────────────┘
```

**Picking the chip:**

- **TPS3823-33** (TI, ~$0.50): simple supervisor with 200 ms watchdog
  timeout, push-pull output. Direct drop-in.
- **MAX6369**: programmable timeout, similar.
- **ATtiny85** (~$1.50): flash a 6-line Arduino sketch if you want
  flexibility — overkill but very debuggable.

The Pi-side software (`platform/hal/watchdog.ts`) toggles BCM 26 every
250 ms (half the WDT timeout). The chip's RESET / OK output is what's
in series with the contactor coil chain — not the WDI input. Be careful
which polarity the chip's output uses (some are active-low RESET, some
active-high OK).

---

## Power supplies

| Rail                     | Source                          | Loads                                                |
|--------------------------|---------------------------------|------------------------------------------------------|
| **5 V / 3 A**            | Pi 5 official 27 W USB-C PSU    | Pi 5 only. Don't share with motors.                  |
| **5 V (logic)**          | Pi 5V rail (pins 2 + 4)         | Relay board logic, opto-isolators, sensors           |
| **12 V or 24 V**         | Bench DC PSU (10 A)             | Solenoid valves, DC pumps, hop dropper, crane motor  |
| **230 V AC**             | Mains via the safety contactor  | HLT element, boil element, mill, stir (if AC)        |
| **24 V (contactor coil)**| Small DC PSU                    | Safety chain — coil energised only when chain intact |

**Star grounding.** The Pi 5 GND, the 12/24 V GND, and mains earth bond
at *exactly one point* (your distribution block). No ground loops.

---

## Bill of materials

Minimum viable build:

| Item                                          | Qty | Notes                                            |
|-----------------------------------------------|-----|--------------------------------------------------|
| Raspberry Pi 5 (4 GB)                         | 1   | Plenty for this load                             |
| Pi 5 27 W USB-C PSU                           | 1   | Official; don't underpower                       |
| Pi 5 official 7" DSI touchscreen              | 1   | Or any HDMI + USB touch                          |
| microSD or NVMe                               | 1   | 32 GB+, Pi OS Bookworm 64-bit                    |
| 16-channel opto-isolated relay board (5 V)    | 1   | Or 2 × 8-channel                                 |
| DS18B20 temperature sensors                   | 2+  | Waterproof, ~1 m lead                            |
| 4.7 kΩ resistor (1/4 W)                       | 1   | 1-Wire bus pull-up                               |
| YF-S201 hall flow sensor                      | 1   | Or G1/2" equivalent                              |
| Float switches NC                             | 2   | HLT mid + high, stainless brewery-grade          |
| Klixon over-temp NC (90 °C)                   | 1   | Bonded to HLT outer wall                         |
| E-stop button (22 mm, NC, mushroom)           | 1   | Twist-release                                    |
| Mains contactor (DPST-NO, 25 A+)              | 1   | Coil voltage matches your control supply         |
| Solid-state relays (3-32 V DC control)        | 2   | Sized for HLT + boil elements (40–100 A typical) |
| SSR heat sinks                                | 2   | With thermal compound                            |
| TPS3823-33 + 0.1 µF cap                       | 1   | External hardware watchdog                       |
| 12/24 V DC PSU (10 A)                         | 1   | For valves, pumps, motors                        |
| Solenoid valves                               | 4   | HLT, MASH, INLET, CHILLER                        |
| Mash pump + chiller pump                      | 2   | March MD-630 or food-grade equivalent            |
| Crane DC motor + worm drive                   | 1   | + 2 mechanical limit switches                    |
| Boil valve motorised actuator                 | 1   | Or 1/2" motorised ball valve                     |
| Hop dropper rotary mechanism                  | 1   | Custom or 3D-printed                             |
| Grain mill (1ph 230 V or 12 V motor)          | 1   | Monster Mill, Crankenstein, or equivalent        |
| Stirrer motor                                 | 1   | Geared, slow                                     |
| DIN-rail terminal blocks                      | -   | For panel wiring                                 |
| Schottky diodes (1N5819 or similar)           | 6+  | Flyback on every DC motor coil                   |
| Hookup wire, ferrules, glands                 | -   | Panel build standards                            |

---

## Boot config

Add to `/boot/firmware/config.txt` (Pi OS Bookworm) and reboot:

```
# 1-Wire on BCM 4 for DS18B20
dtoverlay=w1-gpio

# Hardware PWM on BCM 12 for boil SSR (1 Hz, slow PWM).
# NOTE: pwm-2chan claims both PWM channels. We only enable pwm0 in
# software (boil SSR). The second channel sits idle. If you want to
# fully free BCM 13 (CRANE_LOWER_LIMIT here), use the single-channel
# overlay instead:    dtoverlay=pwm,pin=12,func=4
dtoverlay=pwm-2chan,pin=12,func=4,pin2=13,func2=4

# Disable I2C (we recovered BCM 2/3 for the boil valve outputs).
# dtparam=i2c_arm=off    # default

# Disable SPI (we recovered BCM 9/10/11 for limit switches).
# dtparam=spi=off        # default

# Enable kernel watchdog (Pi-side. The Node app kicks it.)
dtparam=watchdog=on
```

Verify after reboot:

```bash
ls /sys/bus/w1/devices         # should list 28-... directories
ls /sys/class/pwm/pwmchip0     # should exist
ls /dev/watchdog               # should exist
groups pi | grep gpio          # pi user in the gpio group
```

If the brew app refuses to boot with *"Refusing to boot: safety-critical
pins are unmapped"* — that's the assertion catching `HLT_SSR`,
`INLET_VALVE`, `HLT_LEVEL_MID`, or `HLT_LEVEL_HIGH` still set to `null`
in `pinmap.ts`. Edit them to the recommended values (23, 16, 7, 8
respectively, matching the table above), rebuild (`npm run build`), and
re-run.

---

## Build order

What I'd actually wire and verify first, in order:

1. **Bench-test the safety chain** standalone, no Pi involved. E-stop +
   floats + Klixon + WDT IC OK output through the contactor coil.
   Trigger each one in turn and verify the contactor drops.
2. **Pi alone.** Boot Pi 5, install the brew app, run
   `MOCK_HARDWARE=1 npm run mock`. Confirm the UI works.
3. **DS18B20s.** Wire the 1-Wire bus + pull-up + 2 probes. Use
   Diagnostics → Scan bus to identify and assign each.
4. **Float switches.** Wire HLT mid + high; verify Diagnostics shows
   level transitions.
5. **One discrete output** (e.g. CHILLER valve). Wire one relay channel
   end-to-end; verify the Valves tab toggles it.
6. **The rest of the discrete outputs.** Same pattern.
7. **Crane + boil valve.** H-bridges + limit switches. Verify limits
   stop the motor.
8. **Flow sensor.** Wire it, dribble water through, watch the litres
   counter on Diagnostics.
9. **HLT SSR.** Wire its DC control side only. Test with a low-power
   bulb in place of the heater first. Then move to the real element
   *after* the safety chain has been verified.
10. **Boil SSR.** Same caution.
11. **Hop dropper, mill, stir.** Mechanical bits.
12. First test brew with the recipe builder, mash water only, no grain.

Don't skip steps 1 and 9's "test with a bulb" — finding wiring errors
at 60 W is much cheaper than at 5 kW.
