#!/usr/bin/env python3
"""Host tests: compile each effect's C with gcc on the PC and run it.

    python3 tests/run.py            # all
    python3 tests/run.py eugate     # tests whose name starts with "eugate"

They check the DSP logic (patterns, levels, no NaN), not the pedal. They do not replace
listening on hardware. Needs gcc and Python 3. Generated param headers go to tests/_gen.
"""
import os, subprocess, sys, json
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(ROOT / "build"))
sys.path.insert(0, str(ROOT / "src" / "airwindows" / "common"))
from manifest_params import write_param_header  # noqa: E402
from linker import RT_PCR_S21, _patch_pcr_s21, _reloc_addend  # noqa: E402

# Linker checks (both froze the pedal when wrong). Branches count from the 32-byte fetch packet:
code = bytearray(4)
_patch_pcr_s21(code, 0, 0x100, 0x1C)
assert int.from_bytes(code, "little") >> 7 == 0x40, "PCR_S21 not counted from the fetch packet"
# A call from .text (sec 2) into .audio (sec 1) targets the symbol itself; within .text it adds the offset.
rel = {'type': RT_PCR_S21, 'offset': 0x48, 'addend': 0x48}
assert _reloc_addend(rel, 1, 2) == 0 and _reloc_addend(rel, 2, 2) == 0x48 and _reloc_addend(rel, 0, 2) == 0
print("linker checks: ok")

GEN = HERE / "_gen"
GEN.mkdir(exist_ok=True)
for folder, prefix in [("wavefold", "WAVEFOLD"), ("dualshft", "DUALSHFT"), ("formant", "FORMANT"),
                       ("eugate", "EUGATE"), ("dubsiren", "DUBSIREN"), ("sgnl", "SGNL"),
                       ("scrub", "SCRUB"), ("breather", "BREATHER"),
                       ("dirtbox", "DIRTBOX"), ("sweep", "SWEEP"), ("griddly", "GRIDDLY"), ("looper", "LOOPER"), ("louder", "LOUDER"), ("djfilter", "DJFILTER"), ("washout", "WASHOUT"), ("takeoff", "TAKEOFF"), ("gooey", "GOOEY"), ("gooeymb", "GOOEYMB"),
                       ("../probes/tempoprb", "TEMPOPRB"), ("../probes/dryprb", "DRYPRB")]:
    m = json.loads((ROOT / "src" / "custom" / folder / "manifest_pedal.json").read_text(encoding="utf-8"))
    write_param_header(m, GEN / f"{Path(folder).name}_params.h", prefix)
    if folder in ("louder", "gooey", "gooeymb"):   # the meter layout + cover data (build.py writes the same)
        from custom_covers import meter_layout
        (GEN / f"{folder}_layout.h").write_text(meter_layout(folder, m)[1])

want = sys.argv[1:]
failed = []
for src in sorted(HERE.glob("*.c")):
    if want and not any(src.stem.startswith(w) for w in want):
        continue
    # Windows: load low (the pedal code keeps 32-bit pointers) and run the .exe itself
    exe = GEN / (src.stem + (".exe" if os.name == "nt" else ""))
    print(f"\n=== {src.stem}")
    if subprocess.run(["gcc", "-O1", "-no-pie", f"-I{GEN}"] + ([] if src.stem.endswith("_arena") else ["-DZA_NO_SEAL"]) + ([f"-I{HERE / 'win'}", "-Wl,--image-base=0x400000"] if os.name == "nt" else []) + [ str(src), "-lm", "-o", str(exe)]).returncode != 0:
        failed.append(src.stem); continue
    if subprocess.run([str(exe)]).returncode != 0:
        failed.append(src.stem)
print("\nFailed:", ", ".join(failed) if failed else "none")
sys.exit(1 if failed else 0)
