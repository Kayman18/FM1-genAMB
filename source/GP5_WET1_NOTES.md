# genAMB 1.0 RC2-WET1 — stronger GP5 reverb return

This test build keeps the full ten-model PolyWavetable GP5 replacement from RC2 and changes only the shared wet-return stage.

## Change

All ten models (Air, Room, Hall, Church, Plate L, Plate, Spring, N-Star, Deepsea and Sweet Space) now use a stronger, more useful master REVERB curve.

The dry mix remains at unity. Model decay, tone, damping, modulation, per-layer sends and tank topology are unchanged.

Old return mapping:

    wet_gain = 1.10 * REVERB

RC2-WET1 mapping:

    wet_curve = REVERB * (1.35 - 0.35 * REVERB)
    wet_gain  = 1.45 * wet_curve

Approximate wet-return gains:

| REVERB | RC2 | RC2-WET1 |
| ---: | ---: | ---: |
| 25 | 0.275x | 0.458x |
| 50 | 0.550x | 0.852x |
| 65 | 0.715x | 1.058x |
| 75 | 0.825x | 1.184x |
| 100 | 1.100x | 1.450x |

This is roughly +48% wet return at the default value 65 and +32% at maximum. The lower/middle part of the knob becomes wetter sooner.

The reverb level meter now tracks the post-boost audible wet return instead of the raw tank output.

## Validation

- Host build: PASS
- FM1 target build: PASS
- Application: 303,984 bytes
- Package: 610,108 bytes
- RC2 package validator: PASS
- Package SHA-256: 1c1469cbaf78ad1021ae1af7f90e6fd28eebef765a043bd682bc8e72bbbb963f

The existing clean peak guard remains after the dry+wet summing point to protect exceptional overloads.
