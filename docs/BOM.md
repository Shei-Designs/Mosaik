# Bill of Materials

Everything needed for one 32x32 panel (320 x 320 mm). See
[BUILD.md](BUILD.md) for assembly.

## Electronics

| Qty | Part | Notes |
| ---: | --- | --- |
| 4 | WS2812B 8x32 flexible RGB LED matrix | 5 V, 256 LEDs, 10 mm pitch, 320 x 80 x 1.8 mm. The reference build uses AZ-Delivery's *RGB LED Panel WS2812B 8x32 Flex*; any 8x32 WS2812B flex panel with the same pitch and serpentine layout fits. |
| 1 | Mean Well LRS-200-5 | Enclosed power supply, 5 V / 40 A (200 W), 215 x 115 x 30 mm. |
| 1 | ESP32-S3-DevKitC-1 | The printed clip is sized for this board. The data line is GPIO 12 by default (configurable). |
| 1 | IEC C14 panel-mount mains inlet | Screw-mount type with two flange holes. |

## Fasteners

The screws thread directly into the printed plastic. No inserts are
needed. Only the mains inlet uses nuts.

| Qty | Part | Used for |
| ---: | --- | --- |
| 16 | M3 x 12 mm, countersunk head (DIN 7991 / ISO 10642) | Pixel grids to the base, 4 each |
| 4 | M3 x 12 mm, countersunk head | Back plate to the base |
| 2 | M3 x 12 mm, countersunk head | IEC C14 inlet |
| 2 | M3 locking nut (nylon insert, DIN 985) | IEC C14 inlet |
| 8 | M3 x 12 mm, round head (button or pan head, e.g. ISO 7380) | Diffusers to the pixel grids, 2 each |
| 4 | M3 x 12 mm, round head | PSU cover and ESP32 cover |
| 3 | M3 x 12 mm, round head | Wall hanger |
| 2 | M3 x 6 mm, round head | ESP32 clip |
| 6 | M4 x 8 mm, round head | Power supply to the back plate |

Shopping totals: 22x M3x12 countersunk, 15x M3x12 round head, 2x M3x6
round head, 6x M4x8 round head and 2x M3 locking nuts.

## Consumables

- **Plastic glue** suitable for PLA, to bond the four base tiles together

## 3D-printed parts

STL files are in [`3D_print_assets/`](../3D_print_assets). A `_x2` or
`_x4` suffix means print that many copies. Positions are as seen from the
front.

| Qty | File | Purpose |
| ---: | --- | --- |
| 1 | `Base_A_bottom_left.stl` | Base tile (letter A embossed). The four tiles are glued into one 320 x 320 mm base; it has cutouts for the LED panel connectors. |
| 1 | `Base_B_top_left.stl` | Base tile (B) |
| 1 | `Base_C_top_right.stl` | Base tile (C) |
| 1 | `Base_D_bottom_right.stl` | Base tile (D) |
| 2 | `Pixel_Grid_Bottom_x2.stl` | Grid that gives each LED its own square cell, for tiles A and D. Clamps the LED panels against the base. |
| 2 | `Pixel_Grid_Top_x2.stl` | The same, for tiles B and C |
| 4 | `Diffuser_x4.stl` | Front diffuser, screwed onto each pixel grid |
| 1 | `Back_Plate.stl` | Stiffens the glued base and carries the power supply |
| 1 | `PSU_Cover.stl` | Covers the mains terminals of the power supply; holds the IEC C14 inlet |
| 1 | `ESP32_Clip.stl` | Holds the ESP32-S3-DevKitC-1 |
| 1 | `ESP32_Cover.stl` | Covers the ESP32 board |
| 1 | `Cable_Cover_Long.stl` | Covers the connector cutouts and LED wiring on the back |
| 1 | `Cable_Cover_Short_A.stl` | 〃 |
| 1 | `Cable_Cover_Short_B.stl` | 〃 |
| 1 | `Cable_Cover_Square.stl` | 〃 |
| 1 | `Wall_Hanger.stl` | Wall hanger |

The reference build was printed in Prusament PLA on a Prusa MK4S with
the stock PrusaSlicer profile; no special settings are needed. The
largest part is the back plate at 220 x 180 mm; the bases, pixel grids
and diffusers are about 170 x 170 mm each.

For the best diffusion, print the diffusers in **white** PLA on a
**textured** build plate; the textured face helps spread the light.

## Not included above

You will also need:

- **IEC C13 mains cable** (standard "kettle lead") to plug into the inlet.
- **Mains-rated wire** (e.g. 0.75 mm², with insulated crimp terminals)
  from the C14 inlet to the supply's L / N / PE terminals.
- **Power wire** from the supply to each panel's 5 V / GND leads. Use at
  least 1 mm² (18 AWG) per panel.
- **Hook-up wire** for the data line (ESP32 GPIO 12 to the first panel's
  DIN) and the shared ground.
- A **USB cable** for the first firmware flash.

Recommended for signal integrity (see the wiring notes in the
[README](../README.md#wiring-notes)):

- 300-500 Ω resistor in series with the data line
- ≥ 1000 µF / 10 V electrolytic capacitor across 5 V / GND at the panel input
- 74AHCT125 level shifter (only if you see flicker; the ESP32-S3 drives
  3.3 V logic)
