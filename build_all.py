#!/usr/bin/env python3
"""Build the effects into ./dist/.

    py build_all.py              # all of them
    py build_all.py eugate       # one (wavefold, dualshft, formant, eugate, dubsiren, sgnl, scrub, breather, dirtbox, sweep, griddly, looper, louder, djfilter, washout, takeoff, gooey, gooeymb)
    py build_all.py tempoprb     # a hardware probe (src/probes/); only built when named
    py build_all.py dryprb       # likewise
    py build_all.py routeprb     # likewise (Mixer Mode bus probe)

Needs the TI C6000 compiler (see README). formant is Choral, sgnl is S.GN_L (SGNL.ZDL).
"""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
EFFECTS = ["wavefold", "dualshft", "formant", "eugate", "dubsiren", "sgnl", "scrub", "breather", "dirtbox", "sweep", "griddly", "looper", "louder", "djfilter", "washout", "takeoff", "gooey", "gooeymb"]
PROBES = ["tempoprb", "dryprb", "routeprb"]

want = sys.argv[1:] or EFFECTS
bad = [n for n in want if n not in EFFECTS + PROBES]
if bad:
    sys.exit("Unknown effect: " + ", ".join(bad) + ". Choose from: " + ", ".join(EFFECTS + PROBES))
failed = []
for name in want:
    script = ROOT / "src" / ("probes" if name in PROBES else "custom") / name / "build.py"
    if subprocess.run([sys.executable, "-B", str(script)]).returncode != 0:
        failed.append(name)
print("\nBuilt:", ", ".join(n for n in want if n not in failed) or "nothing")
if failed:
    sys.exit("Failed: " + ", ".join(failed))
