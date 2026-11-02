#!/usr/bin/env python3
"""Compile only native Rings adapters; compare retained models against a release tree."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
UNITS = [
    "rings/resources.cc", "rings/dsp/part.cc", "rings/dsp/resonator.cc",
    "rings/dsp/string.cc", "stmlib/dsp/units.cc", "stmlib/utils/random.cc",
]
AFFECTED = [
    "app/rings_port/rings_models.h", "app/rings_port/rings_port.h",
    "app/rings_port/rings_port.cc", "app/vendor/rings/rings/dsp/part.h",
    "app/vendor/rings/rings/dsp/part.cc", "app/vendor/rings/rings/dsp/plucker.h",
    "app/fm1.toml",
]


def run(command):
    command = [str(value) for value in command]
    result = subprocess.run(command, capture_output=True)
    if result.returncode:
        raise RuntimeError(
            f"Command failed ({result.returncode}): {command}\n"
            f"{result.stdout[:3000].decode(errors='replace')}\n"
            f"{result.stderr[:6000].decode(errors='replace')}"
        )
    return result.stdout


def validate(roots, output):
    output.mkdir(parents=True, exist_ok=True)
    summary = []
    provenance = {}
    for label, root in roots.items():
        app = root / "app"
        flags = [
            os.environ.get("CXX", "c++"), "-std=c++11", "-O2",
            "-ffp-contract=off", "-DPLAT_HOST", "-fno-exceptions", "-fno-rtti",
            "-fno-threadsafe-statics", "-Wno-unused-variable", "-Wno-unused-parameter",
            f"-I{app / 'rings_port'}", f"-I{app / 'vendor/rings'}",
        ]
        sources = [app / "vendor/rings" / name for name in UNITS]
        if label == "before":
            sources += [app / "vendor/rings/rings/dsp/fm_voice.cc",
                        app / "vendor/rings/rings/dsp/string_synth_part.cc"]
        else:
            flags += ["-DCURRENT"]
        run(flags + [HERE / "adapter_test.cc", *sources, "-lm", "-o", output / label])
        summary.append(f"{label}: {run([output / label]).decode().strip()}")
        print(summary[-1], flush=True)
        files = set(AFFECTED)
        files.update(str(source.relative_to(root)) for source in sources)
        provenance[label] = {
            "source_tree": str(root),
            "source_sha256": {
                name: hashlib.sha256((root / name).read_bytes()).hexdigest()
                for name in sorted(files) if (root / name).is_file()
            },
        }

    summary.append(run([output / "after", "checks"]).decode().strip())
    print(summary[-1], flush=True)
    run([os.environ.get("CC", "cc"), "-std=c99", "-Wall", "-Wextra", "-Werror",
         f"-I{roots['after'] / 'app/rings_port'}", HERE / "models_c_test.c",
         "-o", output / "models_c_test"])
    summary.append(run([output / "models_c_test"]).decode().strip())
    print(summary[-1], flush=True)

    cases = []
    for model in (0, 1, 2):
        for polyphony in (1, 2, 3, 4):
            arguments = [model, polyphony, 0]
            before = run([output / "before", *arguments])
            after = run([output / "after", *arguments])
            if before != after:
                raise AssertionError(f"Retained-model audio mismatch: {arguments}")
            cases.append({
                "model": model, "polyphony": polyphony, "fx": 0,
                "bytes": len(after), "sha256": hashlib.sha256(after).hexdigest(),
            })
    summary.append(
        f"PASS all {len(cases)} retained-model/polyphony cases: baseline and pending "
        "stereo + reverb-send float samples byte-identical, finite, and nonzero"
    )
    print(summary[-1], flush=True)

    for legacy_id, fallback in ((3, 2), (4, 1), (5, 2), (6, 2)):
        for polyphony in (1, 2, 3, 4):
            legacy = run([output / "after", legacy_id, polyphony, 0])
            retained = run([output / "before", fallback, polyphony, 0])
            if legacy != retained:
                raise AssertionError(
                    f"Legacy fallback audio mismatch: ID{legacy_id}, polyphony{polyphony}"
                )
    summary.append(
        "PASS all four legacy model fallbacks render byte-identically to baseline "
        "replacement models at all four polyphony settings"
    )
    print(summary[-1], flush=True)

    if run([output / "before", "cloud"]) != run([output / "after", "cloud"]):
        raise AssertionError("Global Clouds reverb audio mismatch")
    summary.append("PASS global Clouds reverb float samples byte-identical to release baseline")
    print(summary[-1], flush=True)

    (output / "results.txt").write_text("\n".join(summary) + "\n")
    (output / "audio-hashes.json").write_text(json.dumps(cases, indent=2) + "\n")
    (output / "source-provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path,
                        help="Unmodified genAMB-v17.99.2-FM16 source tree with all seven Rings models")
    parser.add_argument("--candidate", type=Path, default=HERE.parents[2],
                        help="Pending three-model source tree (default: repository containing this script)")
    parser.add_argument("--out", type=Path,
                        help="Keep isolated test binaries and logs here; omitted uses a temporary directory")
    args = parser.parse_args()
    roots = {"before": args.baseline.resolve(), "after": args.candidate.resolve()}
    for root in roots.values():
        if not (root / "app/rings_port/rings_port.cc").is_file():
            parser.error(f"Not a genAMB source tree: {root}")
    if args.out:
        validate(roots, args.out.resolve())
    else:
        with tempfile.TemporaryDirectory(prefix="genamb-rings-three-") as directory:
            validate(roots, Path(directory))


if __name__ == "__main__":
    main()
