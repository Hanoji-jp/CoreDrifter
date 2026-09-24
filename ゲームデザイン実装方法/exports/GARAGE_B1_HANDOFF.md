# Garage room B1 "Night Pit" — implementation brief

Build the 3D garage room behind the GARAGE / CAR SELECT screen.
All numbers live in `drift_garage_scene.h` (namespace `GarageScene`). Use that header as the single source of truth; do not hard-code values again.

## What exists already
- The 2D UI overlay for this screen: `drift_garage.cpp` (title, spec list, stat bars, thumbnails, keycaps). Keep it; it draws on top of the 3D view.
- Shared palette / draw API: `drift_common.h`, `drift_kit.h`.
- The mock this came from was fake 3D (one tilted floor plane, flat painted wall, 2D car image, drawn shadow). Only colors, placement, proportions and motion carry over. Everything spatial must be real geometry.

## Coordinate system
- Meters. Origin = turntable center on the floor.
- +X right, +Y up, +Z toward the camera.
- Room: 14.0 W × 10.0 D × 5.0 H. Floor X −7..+7, Z −5..+5. Back wall at Z = −5.

## Build list
1. **Floor** — plane 14 × 10, color `FLOOR` #33322F, roughness 0.85.
2. **Back wall** — plane at Z −5, 14 × 5, color `WALL` #262624, roughness 0.9.
3. **Shutter** — centered (0, 1.7, −4.95), 6.0 × 3.4. Horizontal slats every 0.12 m, faces #3B3A37, gaps #2C2B28, frame #4A4945. Real depth on the slats (a few mm), not a texture only.
4. **Side door** — (−5.8, 1.05, −4.95), 1.2 × 2.1, frame only, recessed 0.1 m.
5. **Fluorescent tubes × 5** — Y 4.6, Z −4.9, X = −4.8, −2.4, 0, 2.4, 4.8. Each 1.8 long, 0.06 thick. Emissive #F1F0EC. They do NOT cast shadows.
6. **Wall text "PIT 02"** — (4.6, 3.1, −4.98), cap height 0.6, #F1F0EC at 18% opacity. Painted decal, Archivo Black.
7. **Floor lines** — paint decals, width 0.12, #F1F0EC at 55%: two along Z at X = ±5.5 (full depth), one across at Z = −3.2 from X −5.5 to +5.5.
8. **Wheel stop** — box (0, 0.06, −3.4), size 3.6 × 0.12 × 0.25, matte acid #CFE021.
9. **Turntable** — disc at (0, 0.01, 0), outer diameter 5.6. Rim band 0.14 wide, emissive acid #CFE021.
10. **Car** — on the turntable, starting yaw −35°, rotates with it.

## Camera (fixed)
- Position (0, 1.6, 7.5), look at (0, 0.7, 0), vertical FOV 38°, 16:9.
- No shake, no idle drift.

## Lighting
- Ambient low, tinted #1C1C1A.
- Key: one rectangular area light straight above the turntable at Y 4.8. This is the only shadow caster.
- Tubes: emissive only.
- Bloom threshold high (0.9). The ring must never bloom past #CFE021 into white; if it does, the acid stops matching the UI.

## Motion
- Turntable auto-spins at 15°/s (one turn per 24 s).
- Q / E rotates manually; on release, ease out over 0.4 s back into auto-spin.
- Changing car reuses the existing panel-wipe transition (`drift_transition.h`).

## Things that decide whether it looks right
- **Contact shadow + AO under the car.** Without it the car floats. This matters more than any other item.
- **Keep the wall/floor value gap.** The room reads from #262624 wall vs #33322F floor. If it looks too dark, adjust exposure globally rather than lifting one of the two.
- **No giant background title.** The old "DRIFT PROJECT" backdrop text overlapped the car name "S15". The only wall text is the small "PIT 02".
- UI text over this scene is #F1F0EC; only numbers and the selected item use acid.

## Done when
- A screenshot from the fixed camera matches `garage_b1_reference.png` in layout: shutter centered behind the car, tubes along the top of the wall, turntable ring filling roughly the lower middle third, bay lines converging toward the wall.
- The ring stays acid (not white) with bloom on.
- The car sits on the floor with a visible contact shadow.
