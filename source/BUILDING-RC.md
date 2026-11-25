# Building genAMB Nature VA 1.0

Set JIELI_TOOLCHAIN to the JieLi Linux toolchain root containing pi32v2/ and common/. From the source directory, run `sh build-rc.sh --buffer-512` for 17.99.47 or `sh build-rc.sh` for 17.99.46. The build uses the included SDK resources and fonts. It requires Python with Pillow on x86-64 Linux or WSL2.

After each build run `python3 tests/nature/package.py VERSION` with its numeric updater version. Run `python3 tests/nature/run-va.py` for host tests and `node tests/nature/editor.cjs` for editor snapshot tests.

The included ADPCM assets are the authoritative build inputs. To regenerate their C arrays:

```sh
python3 assets/nature/generate.py
python3 assets/nature/generate.py --check
```

For an edited recording, prepare a 16 kHz mono PCM16 WAV, run `python3 assets/nature/encode.py edited.wav assets/nature/<name>.adpcm`, then update the matching manifest length, size and digest with the values printed by the encoder. Run the generator and rebuild. The manifest's offset records the original app location for provenance. The regenerated arrays compile without an original firmware image.

The prepared Water and Rain masters and their original conversion metadata are in `assets/nature/masters/`. Those masters describe the original reconstruction. TEST1 normalizes Rain offline. Run `python3 assets/nature/normalize_rain.py` followed by `python3 assets/nature/generate.py` to regenerate the new Rain payload and bank from the saved master. For the birds, the raw source recordings and the final encoded streams are available. The original intermediate bird preprocessing script and its per-recording settings were not recovered.


## External wave partition

The checked-in `assets/nature/animal-wave.bin` is packaged by the normal device build. No external bank uploader or full-flash image is needed to install it. The matching loader writes and verifies the application and 28 animal-bank sectors.

To re-encode the three source WAVs, use Python 3 and FFmpeg, then rebuild. `--check` regenerates the bytes in memory and compares them with the committed bank, asset manifest and generated C layout.

```sh
python3 assets/nature/animal_wave.py --check
python3 tests/nature/package.py
```

Run the loader simulation with paths to TEST6 and TEST7 firmware, in that order, from the source directory:

```sh
cc -O2 -std=c99 -include app/dsp/animal_wave_layout.h base/tests/ldr_test.c -o app/build/ldr_wave_test
app/build/ldr_wave_test /path/to/genAMB-Nature-TEST6-17.99.39.fwsc app/build/amb-17.99.40.fwsc
```


## Repeat the 512-frame checks

Run from the source directory after building the host dependencies described above.

```sh
python3 tests/nature/test_capture_512.py
sh build-rc.sh --buffer-512 --host
mkdir -p app/build/test512
for scenario in pad-timing controls seq-pages fm-route-controls migration lfo-speed-seq; do
    app/build/host/amb_host "tests/nature/$scenario.amb" app/build/test512 || exit 1
done
sh tests/fm/run_worker_test.sh
```

The USB timing test builds the production 44.1 kHz capture ring against a simulated packet clock, verifies both channels frame by frame and checks for underflow and overflow. The worker test uses a virtual timer to verify timeout and wraparound behaviour.

## M-Flasher clean-install image

After building the desired buffer size, use its freshly built app/build/amb.bin and the [1 MiB clean-install reference image](https://raw.githubusercontent.com/Kayman18/FM1-NovationBassStation/main/recovery/FM1-BassStation-1.0-CLEAN-INSTALL-M-Flasher.bin). The packer checks the reference SHA-256, encrypts the application with its flash key, writes the Nature wave bank and verifies both payloads.

```sh
python3 tools/make_nature_mflasher.py /path/to/FM1-BassStation-1.0-CLEAN-INSTALL-M-Flasher.bin app/build/amb.bin genAMB-Nature-M-Flasher.bin
```
