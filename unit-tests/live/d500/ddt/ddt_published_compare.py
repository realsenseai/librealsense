#!/usr/bin/env python3
"""
CI check: two cases in, one verdict out - does a published output change when the input
changes? One slot per run: --slot occg_out (default) or lpcl_out. Each case is uploaded, frozen, re-run and downloaded, so both grids come from a
deterministic pipeline and the only difference is the case itself.

  ddt_published_compare.py CASE_A CASE_B [--slot occg_out|lpcl_out] [--res 640x360] [--bags 2] [--expect differ|same]

A case directory is what `ddt_standalone.py download` produced: bags/NNNN/<slot>.bin plus an
optional config/<slot>.bin. Change a frame, a config file, or both - the flow is the same.

Exit 0 when the result matches --expect (default: differ), 1 otherwise. Nothing is compared
byte-for-byte on the wire: the published header carries timestamps and is skipped.
"""
import argparse, os, subprocess, sys, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_TOOL = os.path.join(HERE, "ddt.pyz")           # self-contained: no deb, no repo needed
TOOL = DEFAULT_TOOL                                    # overridden by --tool in main()
# Bytes of product header in front of the payload; they carry per-frame timestamps.
PUBLISHED_HEADER = {"occg_out": 52, "lpcl_out": 44}


def run(*args, quiet=True):
    r = subprocess.run([sys.executable, TOOL, *args], capture_output=True, text=True)
    if r.returncode != 0:
        print(f"  ! {' '.join(args)}\n{(r.stdout + r.stderr).strip()[:500]}")
        sys.exit(f"step failed: {' '.join(args)}")
    if not quiet:
        print("   " + (r.stdout.strip().splitlines() or [""])[-1])
    return r.stdout


def payload_crc(path, slot):
    data = open(path, "rb").read()
    return zlib.crc32(data[PUBLISHED_HEADER.get(slot, 0):]), len(data)


def one_case(case_dir, tag, slot, res, bags, out_root, frame_slot):
    """upload -> freeze -> re-run -> download; returns the settled grid CRC."""
    if not os.path.isdir(os.path.join(case_dir, "bags")):
        sys.exit(f"{case_dir} is not a case directory (no bags/)")
    has_cfg = os.path.isdir(os.path.join(case_dir, "config"))
    cfg_slots = [f[:-4] for f in sorted(os.listdir(os.path.join(case_dir, "config")))
                 if f.endswith(".bin")] if has_cfg else []
    sess, runname = f"ci_{tag}", f"ci_{tag}_run"
    out = os.path.join(out_root, tag)
    subprocess.run(["rm", "-rf", out], check=False)

    print(f"-- case {tag}: {case_dir}")
    run("delete", sess); run("delete", runname)
    run("upload", sess, case_dir)
    # The frozen frame fixes the input; the config slots go back to flash in the same command.
    run("freeze", sess, "--slots", ",".join([frame_slot] + cfg_slots), "--res", res, quiet=False)
    # A fresh stream: the camera set re-opens, so the configuration is re-read from flash.
    run("dump", runname, "--slots", f"{frame_slot},{slot}", "--bags", str(bags), "--res", res, quiet=False)
    run("download", runname, "-o", out)
    bag = sorted(os.listdir(os.path.join(out, "bags")))[-1]        # last bag = settled grid
    crc, size = payload_crc(os.path.join(out, "bags", bag, f"{slot}.bin"), slot)
    fcrc, _ = payload_crc(os.path.join(out, "bags", bag, f"{frame_slot}.bin"), frame_slot)
    print(f"   {slot} bag {bag}: payload crc 0x{crc:08x} ({size} B), input crc 0x{fcrc:08x}")
    return crc, fcrc


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("case_a"); p.add_argument("case_b")
    p.add_argument("--slot", default="occg_out", choices=sorted(PUBLISHED_HEADER))
    p.add_argument("--frame-slot", default="depth_z16_full")
    p.add_argument("--res", default="640x360")
    p.add_argument("--bags", type=int, default=2)
    p.add_argument("--out", default="/tmp/ddt_ci")
    p.add_argument("--expect", default="differ", choices=["differ", "same"])
    p.add_argument("--tool", default=DEFAULT_TOOL,
                    help="ddt.pyz (or ddt_standalone.py) to run each step through "
                         "(default: ./ddt.pyz next to this script)")
    a = p.parse_args()

    global TOOL
    TOOL = a.tool

    ca, fa = one_case(a.case_a, "a", a.slot, a.res, a.bags, a.out, a.frame_slot)
    cb, fb = one_case(a.case_b, "b", a.slot, a.res, a.bags, a.out, a.frame_slot)

    print(f"\ninput  A 0x{fa:08x}   B 0x{fb:08x}   {'same' if fa == fb else 'DIFFER'}")
    print(f"{a.slot:9s} A 0x{ca:08x}   B 0x{cb:08x}   {'same' if ca == cb else 'DIFFER'}")
    if fa == fb and ca == cb and a.expect == "differ":
        print("NOTE: the two cases fed the pipeline the same input, so an identical grid is "
              "expected - change a frame or a config file.")
    ok = (ca != cb) if a.expect == "differ" else (ca == cb)
    print(("PASS" if ok else "FAIL") + f": grids {'differ' if ca != cb else 'match'}, expected {a.expect}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
