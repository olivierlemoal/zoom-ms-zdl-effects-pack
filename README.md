<p align="center"><img src="docs/images/banner.jpg" alt="MATUJUICE EFFECT PACK v2.0.1 on the screen of a Zoom MultiStomp" width="720"></p>

<p align="center"><img alt="effects pack cover v2 0" src="https://github.com/user-attachments/assets/2f8fa485-b7cb-44c6-ad30-2468fd3668cd" width="720"></p>

# Matujuice Zoom MS ZDL effects pack

Free custom effects for Zoom MultiStomp pedals, with source code.

| Effect | What it is |
|---|---|
| **WaveFold** | Buchla-style wavefolder with automatic level matching. |
| **DualShft** | Two tempo-synced pitch shifters, each with its own echo time (free or synced), and an LFO that bends them in opposite directions. |
| **Choral** | Listens to the note your synth plays and sings it as a choir: men, women, kids or giants in 15 combinations plus monks and gospel, 1 to 6 singers, in unison, intervals or chords, on vowels, syllables, swells or a canon in time. For mono synth lines. |
| **EuGate** | Euclidean rhythm gate. Steps and Notes up to 64 each, so polymeters are possible. Swing, Gap and Soft too. |
| **DubSiren** | A dub siren on the footswitch with a tape-style echo. The rate can sync to tempo. |
| **S.GN_L** | A broken digital line: packets drop out and get replaced by silence, a buzzing replay, a fade or hiss, with codec damage on top. |
| **Scrub** | Records the last 7.9 seconds and scrubs the last 6; a knob moves a read head through them, and where you stop, the grain under the head loops forever (a freeze). |
| **GRDELAY** | One tempo-synced delay with six engines on a Type knob (clean, tape, dub, reverse, multitap, lo-fi), Duck, and a Tail switch so repeats ring out when you switch it off. |
| **Breather** | Tempo-synced pump with a built-in reverb: duck, gate or swell your sound, the reverb, both, or what feeds the reverb, on every beat. |
| **DIRTBOX** | A distortion box for a 303: seven real pedal circuits (TS9, Distortion+, DS-1, RAT 2, Big Muff, Super-Fuzz, Metal Zone), a 3-band acid EQ and an automatic noise reducer. |
| **Sweep** | Phaser, flanger and resonant filter in one effect: Type picks the engine, one LFO sweeps it, from a 1/32 note up to 8 bars, locked to the bar. |
| **LOUDER** | Transparent look-ahead mastering limiter for the end of the chain: Gain, Ceil, Zoom (cover meter), Rels (AUTO or 1-1000 ms), Style (CLEAN, PUNCH, LOUD), Mix. One gain for both sides, hard clip at the ceiling. |
| **DJFilter** | The DJ mixer filter you ride by hand, plus a kick kill: a one-knob DJ filter (left low-pass, right high-pass), Reso that follows the sweep, Kill (24 dB/oct low cut, 20-300 Hz), Slope 12/24, Glide, Level, Mix. The footswitch is the kick kill. |
| **Washout** | A tempo-synced stereo delay whose loop smears the repeats into a reverb-like cloud (4 all-pass diffusers in the feedback): Time, Wash, Fdbk up to 120, HiCut, LoCut, Width, Send (a crossfade-send, no Mix knob). The footswitch is on/off with trails. |
| **Takeoff** | Risers and downlifters that only add sound on top: mono noise sweeps with swelling volume, Dir UP/DOWN, Length in bars or seconds, Color, Wind and Grain to shape the noise, Range, Var so no two takeoffs sound the same, Tail to fade a riser out over bars or seconds. A footswitch flip fires a sweep at once, a hold rises while held, and on MIDISTOMP v2.0.1 the LED fades from on to off over the run. |
| **Gooey** | SSL G-bus style mix bus compressor: Thrsh (0.1 dB steps, 0 = off), Ratio, Zoom (cover meter), Attck, Rels (AUTO is program-dependent), MkUp (AUTO adds back the average reduction), Mix (parallel), SC HP (sidechain high-pass). One gain for both sides. |
| **GooeyMB** | Gooey in two bands, for bass: a Linkwitz-Riley crossover (XOver 60-400 Hz) splits low and high, each with its own threshold (LoThr, HiThr) and ratio, shared Attck and Rels, a fixed AUTO makeup per band, an output Level and a -1 dBFS ceiling, Mix (parallel) and Gooey's live meter on the low band. Tested on a stock MS-50G 3.10. |
| **GRDLOOP** | A bar-synced stereo looper on the song grid: the footswitch records, stops, plays and (double press) deletes, on the next beat, bar or cycle (Next); Sync OWN or SLAVE; latched beat Roll; the loop stays on the grid when the tempo changes. Made for MIDISTOMP v2.0.1: its looper services give 8 bars stereo from 83 BPM, a footswitch that never flips the effect and the LED state; on stock firmware about 9 s mono. |

Every knob is explained in [docs/EFFECTS.md](docs/EFFECTS.md).

**Tempo and MIDI sync:** the tempo effects (DualShft, Choral, EuGate, DubSiren, Scrub, Breather, GRDELAY, Sweep) are made for the custom MS firmware [MIDISTOMP](https://github.com/matujuice/zoom-ms-midistomp) (v1.0 or later). They have no Tempo knob: they follow the pedal's tempo (MIDI clock, tap or patch tempo), restart on MIDI Start and stay on the clock's grid; after Stop they keep running at the last tempo. On stock firmware they follow the tap tempo and the patch TEMPO, without the MIDI clock. Release v1.5 has the older builds with their own Tempo knob and the iPhone (Mozaic) bar sync. Details: [docs/TEMPO-SYNC.md](docs/TEMPO-SYNC.md) section 10.

**Tested on:** a Zoom MS-60B running MS-50G firmware. The MS-50G, MS-70CDR and other MS pedals are untested; if you try one, please open an issue and say whether the effects load and work.

Unofficial. Not affiliated with or endorsed by Zoom. Use at your own risk.

## Install

1. Download the latest pack from the [Releases](../../releases) page and unzip it.
2. Load the `.ZDL` files with Zoom Effect Manager ("Read Effects from folder"). Step by step: [docs/INSTALLING-ZDLS.md](docs/INSTALLING-ZDLS.md).

Release v2.0.1 has all seventeen effects, made for MIDISTOMP v2.0.1 and working on stock firmware too. To try newer changes on `main`, build them from source (below).

Each effect has its own ID in the Delay category (480 DualShft, 481 LOUDER, 485 DubSiren, 486 Choral, 487 WaveFold, 488 EuGate, 489 S.GN_L, 490 Scrub, 491 DirtBox, 492 Washout, 493 Breather, 494 Takeoff, 495 GRDELAY, 496 Sweep, 497 GRDLOOP, 501 Gooey, 502 DJFilter, 947 GooeyMB). If another custom effect on your pedal uses one of these numbers, change `fxid` in that effect's `manifest_pedal.json` and rebuild.

## What's next

- **Modwerk integration:** the effects (and the [MIDISTOMP](https://github.com/matujuice/zoom-ms-midistomp) firmware mods) inside Modwerk, so you pick them and load them onto the pedal from the browser, without Zoom Effect Manager or a build.

## Build from source

You need Python 3 and the TI C6000 compiler (`ti-cgt-c6000_8.5.0.LTS`, free from Texas Instruments). The build scripts look in the usual install places (`C:\ti`, your Downloads folder, `/Applications/ti`); otherwise set `TI_CGT_ROOT` to the folder that contains `bin` and `include`.

```
py build_all.py            # build them all into dist/
py build_all.py eugate     # build one
py make_release.py         # zip dist/*.ZDL with the readme and licence into release/
```

Effect names for `build_all.py`: `wavefold`, `dualshft`, `formant` (Choral), `eugate`, `dubsiren`, `sgnl` (S.GN_L, file `SGNL.ZDL`), `scrub`, `breather`, `dirtbox`, `sweep`, `griddly` (GRDELAY), `looper` (GRDLOOP), `louder`, `djfilter`, `washout`, `takeoff`, `gooey`, `gooeymb`. Probes (`tempoprb`, `dryprb`, `routeprb`) are built only when named.

## Repository layout

```
src/custom/<effect>/   DSP source (.c), manifest_pedal.json (knobs, defaults, ID), make_cover.py, build.py
src/custom/common/     zmt.h, the custom firmware's MIDI transport block (tempo, Start, clock count)
src/airwindows/common/ shared cover and parameter helpers; covers/*.json are the generated covers
build/                 the ZDL linker and tools (from ZoomMultistompZDL, see Credits)
docs/                  knob reference, install guide, DSP rules
graphics/              each effect's cover as the pedal shows it (py build/make_gallery_png.py)
tests/                 host tests: python3 tests/run.py
tools/                 zoom_sysex.py, a USB connection test script
```

To change a cover, edit that effect's `make_cover.py` and run it; it rewrites the cover JSON and a preview image. The pedal's pixels are 1.4 times taller than wide, so round shapes are drawn squashed.

## Credits

- Built on the toolchain from [themanro/ZoomMultistompZDL](https://github.com/themanro/ZoomMultistompZDL) (MIT, Copyright (c) 2026 Roman Rusinov). The `build/` folder and the install and DSP-rule docs come from there; the upstream repo has much more documentation.
- Alexander Lim. The live screens and the zvis kit, the pedal's file-size cap and DSP limit, and linker pitfalls.
- WaveFold's fold curve is from "Virtual Analog Buchla 259 Wavefolder" (Esqueda, Pontynen, Valimaki, Parker; DAFx-2017).
- DIRTBOX's circuit models follow published analyses and open models (guitarix, ElectroSmash, DS1.lv2, Proco-Rat, MetalTone); links are in [EFFECTS.md](docs/EFFECTS.md).
- The effects themselves are original work.
- Runs best on [MIDISTOMP](https://github.com/matujuice/zoom-ms-midistomp), the custom firmware for the Zoom MS pedals.
- Testers: Jannik Aßfalg, Roman Rusinov, nilsjesper and Snozaz. Thank you for running betas on your pedals.

## Licence

MIT, see [LICENSE](LICENSE).
