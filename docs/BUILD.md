# Mosaik: Build Instructions

How to build a Mosaik. Get all the parts in the
[Bill of Materials](BOM.md) first; part names below match the STL
filenames in [`3D_print_assets/`](../3D_print_assets).

> **Mains voltage.** The LRS-200-5 is wired directly to mains power. Only
> do this if you are competent with mains wiring. Do all wiring with the
> supply unplugged, connect protective earth (PE), and make sure no mains
> terminal can be touched once the PSU cover is on.

## Overview

The build is a sandwich, from back to front:

```
  back  │ PSU cover / ESP32 cover / cable covers / wall hanger
        │ Back Plate (stiffener, carries the PSU)
        │ Base: tiles A-D glued together, with connector cutouts
        │ 4x LED panels
        │ 4x Pixel Grid (one square cell per LED)
 front  │ 4x Diffuser
```

The LED panels have no fixings of their own. They are clamped between
the base and the pixel grids, and the grid cells keep every LED in
place. The panel connectors pass through cutouts in the base, so the
panels are daisy-chained and wired up on the back. The cable covers hide
those cutouts and the wiring.

## 1. Print the parts

Print every part listed under *3D-printed parts* in the [BOM](BOM.md).
Files ending in `_x2` or `_x4` need that many copies (2 + 2 pixel grids,
4 diffusers). Print everything else once.

Standard PLA settings work; the reference build used Prusament PLA on a
Prusa MK4S with the stock profile. Print the diffusers in **white** on a
**textured** build plate for the best diffusion.

The screws thread directly into the plastic. If a hole is too tight, run
a screw through it once before final assembly.

## 2. Flash the firmware

It's easiest to flash the ESP32-S3 over USB on the bench, before it's
built in. After that, all updates can go over WiFi (see
[OTA updates](../README.md#ota-updates)).

```sh
source ~/esp/esp-idf/export.sh     # ESP-IDF v5.4
idf.py build
idf.py -p /dev/ttyACM0 flash
```

You don't need to configure WiFi at build time; you can set it from the
web UI later (step 8).

## 3. Build the base

1. **Glue the four base tiles together** with a plastic glue suitable for
   PLA. Seen from the front, they go:

   ```
   +---------+---------+
   |    B    |    C    |
   +---------+---------+
   |    A    |    D    |
   +---------+---------+
   ```

   The letters are embossed on the tiles. Let the glue cure fully. This
   is the only glued joint; everything else is screwed.
2. Screw the **Back Plate** to the back of the base with **4 M3 x 12 mm
   countersunk** screws. It stiffens the glued base.
3. Mount the **LRS-200-5** on the back plate with the
   **6 M4 x 8 mm round-head** screws.

## 4. Fit the LED panels

The four 8x32 LED panels stand **vertically, side by side** on the front
of the base. Each one is 8 LEDs wide and 32 tall, so it spans a bottom
tile and the tile above it: the left two cover A/B and the right two
cover D/C.

- The data chain runs **left to right**: the leftmost panel is first, and
  its DIN goes to the ESP32.
- The short connectors between panels force **every second panel to be
  rotated 180°**. On panels 1 and 3, data enters at the bottom-left; on
  panels 2 and 4, at the top-right.
- Feed each panel's connectors and power leads through the cutouts in the
  base to the back, and connect each panel's DOUT to the next panel's DIN
  there.

This is the layout the firmware's default mapping expects. If you build it
differently, the mapping options in `menuconfig` can compensate (see
[Panel topology](../README.md#panel-topology-and-mapping-verification)).

## 5. Fit the pixel grids and diffusers

1. Lay a **Pixel_Grid_Bottom** over tiles A and D, and a
   **Pixel_Grid_Top** over tiles B and C.
2. Line up the square cells with the LEDs so each LED sits centred in its
   own cell, then screw each grid to the base with **4 M3 x 12 mm
   countersunk** screws (16 in total). Tighten gently; the grids are what
   clamp the LED panels in place.
3. Screw a **Diffuser** onto each pixel grid with **2 M3 x 12 mm
   round-head** screws (8 in total).

## 6. Wire it up

The power supply sits on one side of the back plate. Mains comes in
through the IEC inlet on the PSU cover. 5 V runs across to the other side,
where the ESP32 and the LED panel connections are.

```
 mains ──► IEC C14 ──► LRS-200-5 ──► 5 V / GND ──┬──► panel 1 power leads
           (PSU cover)                           ├──► panel 2 power leads
                                                 ├──► panel 3 power leads
                                                 ├──► panel 4 power leads
                                                 └──► DevKitC-1 5V / GND pins

 ESP32-S3 GPIO 12 ──[330 Ω]──► panel 1 DIN
 panel 1 DOUT ──► panel 2 DIN ──► … ──► panel 4
```

1. Mount the **IEC C14 inlet** in the **PSU Cover** with the **2 M3 x 12
   mm countersunk** screws and **M3 locking nuts**.
2. **Before connecting mains**, check the input voltage selector switch on
   the LRS-200-5 is set for your local mains (115 V or 230 V).
3. Wire the inlet's live, neutral and earth to the supply's **L**, **N**
   and **PE** (⏚) terminals with mains-rated wire and insulated crimp
   terminals. All mains connections stay under the PSU cover.
4. Run 5 V and GND from the supply's output terminals to **every** panel's
   power leads. Don't rely on the thin data connectors to carry power
   down the chain.
5. Put the ESP32-S3-DevKitC-1 in the **ESP32 Clip** and screw the clip to
   the back with the **2 M3 x 6 mm round-head** screws.
6. Power the DevKitC-1 from the same supply: 5 V into its **5V** pin, GND
   to **GND**. This also gives the board and the LEDs the **shared
   ground** they must have. Don't plug in USB while the supply is on
   unless you're sure your board won't back-feed it.
7. Connect **GPIO 12** to the first panel's DIN. Optionally, add the
   series resistor on the data line and the capacitor across 5 V / GND at
   the first panel (see the [wiring notes](../README.md#wiring-notes)).

### Current budget

The firmware limits brightness so that each frame stays within
`PANEL_MAX_CURRENT_MA` (default **10 A**). Full white on all 1024 LEDs
would need about 61 A, more than the LRS-200-5's 40 A rating, so the
limiter must stay enabled. If your power wiring is sized for it, you can
raise the budget in `idf.py menuconfig` → *Mosaik Configuration*. Keep
it well below 40 A.

## 7. Close up the back

1. Fit the **cable covers** (long, short A, short B and square) over the
   connector cutouts and the LED wiring.
2. Fit the **PSU Cover** over the supply's terminals and the **ESP32
   Cover** over the board with **4 M3 x 12 mm round-head** screws.
3. Attach the **Wall Hanger** with the remaining **3 M3 x 12 mm
   round-head** screws.

## 8. First power-on and WiFi setup

1. Power up. The panel starts the last-selected animation (Plasma on first
   boot).
2. Because no WiFi is configured yet, it opens an access point called
   **`Mosaik`** (password `mosaik123`).
3. Join it from your phone and open `http://192.168.4.1`.
4. Under **WiFi**, enter your network name and password, then tap
   *Save & restart panel*.
5. Reconnect your phone to your own WiFi and open
   **`http://mosaik.local`**.

## 9. Check the mapping

In the web UI's **Pixel Art** editor, draw something asymmetric, for
example an "F" in the top-left corner, and tap *Send*. It should appear
the same way round on the panel. If it doesn't, see the troubleshooting
table in the [README](../README.md#panel-topology-and-mapping-verification).
