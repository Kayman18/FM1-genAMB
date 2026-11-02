# genAMB 1.0 RC2-WET2 — true wet/dry GP5 mix

WET2 keeps the stronger WET1 return for all ten GP5 reverb models, but changes the master REVERB control from an additive send/return into a wet-dominant crossfade.

## Why

WET1 still left the complete dry mix at unity at every REVERB value. Even at REVERB 100 the output was 100% dry plus the boosted reverb return, which could still sound much too direct.

## WET2 mix

The shared curve is:

    wet_curve = REVERB * (1.35 - 0.35 * REVERB)
    wet_gain  = 1.45 * wet_curve
    dry_gain  = 1.0 - wet_curve

Approximate gains:

| REVERB | Dry | GP5 wet return |
| ---: | ---: | ---: |
| 0 | 1.000x | 0.000x |
| 25 | 0.684x | 0.458x |
| 50 | 0.413x | 0.852x |
| 65 | 0.270x | 1.058x |
| 75 | 0.197x | 1.184x |
| 100 | 0.000x | 1.450x |

At REVERB 100 there is no direct dry path. This shared mix affects Air, Room, Hall, Church, Plate L, Plate, Spring, N-Star, Deepsea and Sweet Space equally. Model decay/tone/damping/modulation are unchanged.

The existing clean peak guard remains after the mix stage.

## Validation

- Host build: PASS
- FM1 target build: PASS
- Application: 303,872 bytes
- Package: 610,108 bytes
- RC2 package validator: PASS
- Package SHA-256: 7a98f8a7a6c87ec11438877d744c41a687163beb1651333d2ad4ab716ab17fe0
