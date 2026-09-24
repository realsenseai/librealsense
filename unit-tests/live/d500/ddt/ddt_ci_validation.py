#!/usr/bin/env python3
"""
DDT CI regression: does the published output follow the input, and only the input?

Corpus layout (built by --build, then kept under version control or on a share):

  <corpus>/gt/              ground truth
      bags/0000/depth_z16_full.bin(+.meta)
      config/<slot>.bin                     the configuration this case runs with
      expected/occg_out.bin, lpcl_out.bin   what the camera produced for it
  <corpus>/depth_changed/   same layout, one depth frame edited
  <corpus>/config_changed/  same layout, one configuration field edited

The run (ONE process, ONE camera connection - DDT_VALIDATION_FAST_PLAN_20260922.md R1-R6):
  a. upload the three cases to the camera ONCE (reused by every step below)
  1. gt             -> published outputs must MATCH expected/
  2. depth_changed  -> must DIFFER from the gt expectation
  3. config_changed -> must DIFFER from the gt expectation
  b. restore the saved configuration, whatever happened; ONE cleanup at the end

Each published slot (occg_out / lpcl_out) is its own pass, because the mapping endpoint
carries one profile at a time. Within a pass, gt and depth_changed share ONE stream open
(same configuration, so no rebuild is needed between them - play_frame() just re-freezes,
~1 ms, and reads the settled result straight off the open queue). config_changed writes
its own configuration first: a flash write only takes effect at the NEXT pipeline build
(measured on-camera 2026-09-22 - a live write while the stream stays open changes nothing),
so config_changed gets its own dedicated stream open, with the write landing while the
stream is closed. No dump, no download: play_frame()'s payload CRC is the exact same
pad-safe CRC a dump+download would give (compare.payload, byte-for-byte verified against
a golden captured the old way).
"""
import argparse, os, shutil, struct, subprocess, sys, time, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_TOOL = os.path.join(HERE, "ddt.pyz")           # self-contained: no deb, no repo needed
TOOL = DEFAULT_TOOL                                    # only used by --build (subprocess path)
SERIAL = None                                          # --serial: pick one camera on a multi-device rig
# ddt.pyz is a zipapp - importable straight off sys.path like any other zip - so the payload
# layout (MAP1 header size, frame size) comes from its own bundled ddt_core, not from a repo
# checkout: the 64-byte alignment padding the dump carries past the frame (stale DDR, never
# written by the firmware) is not compared.
sys.path.insert(0, DEFAULT_TOOL)
from ddt_core.compare import SLOT_KIND, payload        # noqa: E402

PUBLISHED = SLOT_KIND                                  # the published slots this test checks
FRAME_SLOT = "depth_z16_full"
CONFIG_SLOTS = ["safety_preset", "depth_calib"]
ROBOT_HEIGHT_OFF, HDR_CRC_OFF, SIZE_OFF = 60, 8, 4      # safety_preset record, per the FW asserts

# tag -> (case dir name under <corpus>/, qualified camera session, expected verdict)
CASES = (("gt", "gt", "match"),
         ("depth", "depth_changed", "differ"),
         ("config", "config_changed", "differ"))


TIMINGS = []   # (label, seconds), printed with --timing


def _timed(label, fn, *a, **kw):
    t0 = time.perf_counter()
    r = fn(*a, **kw)
    TIMINGS.append((label, time.perf_counter() - t0))
    return r


def crc(path, slot):
    with open(path, "rb") as fh:
        return zlib.crc32(payload(fh.read(), slot)) & 0xffffffff


def cfg_slots_in(case):
    d = os.path.join(case, "config")
    return [f[:-4] for f in sorted(os.listdir(d))] if os.path.isdir(d) else []


# ---------------------------------------------------------------------- --build (unchanged
# subprocess path: out of scope for the fast rewrite, R1-R6 target the run below)
def tool(*args, check=True):
    pre = ["--serial", SERIAL] if SERIAL else []
    r = subprocess.run([sys.executable, TOOL, *pre, *args], capture_output=True, text=True)
    if check and r.returncode != 0:
        print(f"    ! {' '.join(args)}\n{(r.stdout + r.stderr).strip()}")
        raise RuntimeError(f"step failed: {' '.join(args)}")
    return r.stdout


def build(corpus, res, bags):
    """Capture a ground truth from the live camera and derive the two changed cases."""
    gt = os.path.join(corpus, "gt")
    shutil.rmtree(corpus, ignore_errors=True)
    os.makedirs(gt, exist_ok=True)
    print(f"building corpus in {corpus}")
    tool("delete", "ci_build", check=False)
    tool("dump", "ci_build", "--slots", f"{FRAME_SLOT}," + ",".join(CONFIG_SLOTS), "--res", res)
    tool("download", "ci_build", "-o", gt)
    exp = os.path.join(gt, "expected")
    os.makedirs(exp, exist_ok=True)
    work = os.path.join(corpus, ".work")
    for slot in PUBLISHED:
        sess, run = "ci_gt", "ci_gt_r"
        tool("delete", sess, check=False); tool("delete", run, check=False)
        tool("upload", sess, gt)
        tool("freeze", sess, "--slots", ",".join([FRAME_SLOT] + cfg_slots_in(gt)), "--res", res)
        tool("dump", run, "--slots", f"{FRAME_SLOT},{slot}", "--bags", str(bags), "--res", res)
        out = os.path.join(work, slot)
        shutil.rmtree(out, ignore_errors=True)
        tool("download", run, "-o", out)
        bag = sorted(os.listdir(os.path.join(out, "bags")))[-1]
        shutil.copy(os.path.join(out, "bags", bag, f"{slot}.bin"), os.path.join(exp, f"{slot}.bin"))
        print(f"   expected/{slot}.bin  crc 0x{crc(os.path.join(exp, f'{slot}.bin'), slot):08x}")

    dc = os.path.join(corpus, "depth_changed")
    shutil.copytree(gt, dc)
    p = os.path.join(dc, "bags", "0000", f"{FRAME_SLOT}.bin")
    b = bytearray(open(p, "rb").read())
    for i in range(0, min(len(b), 640 * 60 * 2), 2):
        struct.pack_into("<H", b, i, 1500)
    open(p, "wb").write(bytes(b))
    print(f"   depth_changed: 1500 mm band, crc 0x{zlib.crc32(bytes(b)) & 0xffffffff:08x}")

    cc = os.path.join(corpus, "config_changed")
    shutil.copytree(gt, cc)
    p = os.path.join(cc, "config", "safety_preset.bin")
    d = bytearray(open(p, "rb").read())
    was = struct.unpack_from("<f", d, ROBOT_HEIGHT_OFF)[0]
    struct.pack_into("<f", d, ROBOT_HEIGHT_OFF, 0.5 if was > 0.9 else 1.0)
    size = struct.unpack_from("<I", d, SIZE_OFF)[0]
    struct.pack_into("<I", d, HDR_CRC_OFF, zlib.crc32(bytes(d[12:size])) & 0xffffffff)
    open(p, "wb").write(bytes(d))
    print(f"   config_changed: robot_height {was:.2f} -> "
          f"{struct.unpack_from('<f', d, ROBOT_HEIGHT_OFF)[0]:.2f} m")
    shutil.rmtree(work, ignore_errors=True)
    # Leave nothing of ours on the camera: a stale ci_* session shows up in the client's
    # case list and a replay opened on it then blocks the next Capture.
    for name in ("ci_build", "ci_gt", "ci_gt_r"):
        tool("delete", name, check=False)
    print("corpus ready")


# ---------------------------------------------------------------------- run (R1-R6)
def run(corpus, slot_filter, timing, serial=None):
    import contextlib
    from ddt_core import ops, flows
    from ddt_core.camera import DdtCamera, Streamer, camera_busy
    from ddt_core.files import FileService
    import ddt_core.protocol as P

    @contextlib.contextmanager
    def timed_stream(names, res, label):
        """Streamer.__enter__/__exit__ timed separately - close (stop()+close() on the
        sensor) is the dominant cost of a rebuild (~1.5-2 s, DDT_UI_ROUND3 measurement),
        so it must show in the table, not disappear inside an untimed `with`."""
        t0 = time.perf_counter()
        streams = Streamer(cam, names, res)
        streams.__enter__()
        TIMINGS.append((f"{label}: open", time.perf_counter() - t0))
        try:
            yield streams
        finally:
            t0 = time.perf_counter()
            streams.__exit__(None, None, None)
            TIMINGS.append((f"{label}: close", time.perf_counter() - t0))

    gt_dir = os.path.join(corpus, "gt")
    exp = {s: crc(os.path.join(gt_dir, "expected", f"{s}.bin"), s) for s in PUBLISHED
           if os.path.exists(os.path.join(gt_dir, "expected", f"{s}.bin"))}
    if not exp:
        sys.exit(f"{gt_dir}/expected/ has no golden outputs - run with --build first")
    if slot_filter:
        missing = [s for s in slot_filter if s not in exp]
        if missing:
            sys.exit(f"no golden for --slot {', '.join(missing)} in {gt_dir}/expected/ "
                     f"(present: {', '.join(sorted(exp)) or 'none'})")
        published = [s for s in sorted(exp) if s in slot_filter]
    else:
        published = sorted(exp)

    # R2: geometry comes from the cases themselves, and every case run together must agree.
    case_dirs = {tag: os.path.join(corpus, case) for tag, case, _ in CASES}
    geoms = {}
    for tag, d in case_dirs.items():
        try:
            geoms[tag] = ops.case_geometry(d)
        except ValueError as e:
            sys.exit(str(e))
    if len(set(geoms.values())) > 1:
        sys.exit("corpus cases disagree on geometry: " +
                 ", ".join(f"{tag} {w}x{h}" for tag, (w, h) in geoms.items()))
    w, h = next(iter(geoms.values()))
    res = f"{w}x{h}"

    # One camera consumer, checked before any flash write.
    busy = camera_busy()
    if busy:
        sys.exit("another process holds the camera - close it first, then rerun:\n" + busy
                 + "\n(e.g.  pkill -f \"[d]dt_client.py\"  for the DDT client)")

    cam = _timed("connect", DdtCamera, serial)

    # The package's backup/restore only proves "restored what was FOUND". Compare what is in
    # flash now with the corpus ground truth, so a preset left behind by an earlier run is
    # named up front instead of being preserved silently.
    import hashlib as _hl
    for _n in ("safety_preset", "depth_calib"):
        _gt = os.path.join(corpus, "gt", "config", f"{_n}.bin")
        if not os.path.isfile(_gt):
            continue
        try:
            _flash = cam.read_config(_n)
        except Exception as _e:
            print(f"   (could not read {_n} from flash for the sanity check: {_e})"); continue
        _hf, _hg = _hl.md5(bytes(_flash)).hexdigest()[:8], _hl.md5(open(_gt, "rb").read()).hexdigest()[:8]
        if _hf != _hg:
            print(f"   WARNING: flash {_n} = {_hf} but corpus gt = {_hg} - the camera does not hold the "
                  f"ground-truth configuration (a previous run may have left a test preset in flash). "
                  f"gt MATCH may fail, and the run will restore {_hf}, not {_hg}.")
    fs = FileService()
    print(f"connected: {cam.name} S/N {cam.serial} FW {cam.fw}  ({res})")

    session = {tag: P.qualify(f"ci_{tag}", P.AREA_IN) for tag in case_dirs}
    results = {}   # (tag, slot) -> crc
    try:
        print("a. uploading the three cases once")
        for tag, d in case_dirs.items():
            _timed(f"upload {tag}", fs.upload_session, session[tag], d)

        table = cam.slot_table()
        depth_id = table[FRAME_SLOT].id

        for n, slot in enumerate(published, 1):
            result_id = table[slot].id
            print(f"\n{n}. {slot}")

            # pass A: gt's configuration, ONE stream open shared by gt + depth_changed
            # (their configuration is identical, so no rebuild is needed between them).
            _timed(f"{slot}: gt config", flows.pull_config, cam, fs, session["gt"],
                  CONFIG_SLOTS, log=lambda _m: None)
            cam.enable([depth_id, result_id], freezable=True, replace=True)
            with timed_stream([FRAME_SLOT, slot], res, f"{slot}: gt+depth") as streams:
                for tag in ("gt", "depth"):
                    r = _timed(f"{slot}: play {tag}", ops.play_frame, cam, streams,
                              session[tag], 0, result_slot=slot)
                    results[(tag, slot)] = r[slot]["crc"]

            # pass B: config_changed's configuration only takes effect on a fresh build,
            # so it gets its own dedicated stream open (measured on-camera 2026-09-22).
            _timed(f"{slot}: config_changed config", flows.pull_config, cam, fs,
                  session["config"], CONFIG_SLOTS, log=lambda _m: None)
            cam.enable([depth_id, result_id], freezable=True, replace=True)
            with timed_stream([FRAME_SLOT, slot], res, f"{slot}: config") as streams:
                r = _timed(f"{slot}: play config", ops.play_frame, cam, streams,
                          session["config"], 0, result_slot=slot)
                results[("config", slot)] = r[slot]["crc"]
    finally:
        try:
            _timed("restore config", flows.restore_config, cam, fs, log=print)
        except Exception as e:
            print(f"b. RESTORE FAILED: {e}\n   restore by hand (backup session '_cfgbackup')")
        cam.realtime("all")
        for name in session.values():
            cam.delete(name)

    verdicts = []
    for tag, case, expect in CASES:
        print(f"\n{case}  (expect {expect})")
        ok_all = True
        for slot in published:
            got = results[(tag, slot)]
            same = (got == exp[slot])
            ok = same if expect == "match" else not same
            ok_all &= ok
            print(f"   {slot:9s} 0x{got:08x} vs gt 0x{exp[slot]:08x}  "
                  f"{'same' if same else 'differ':6s} -> {'OK' if ok else 'FAIL'}")
        verdicts.append((case, ok_all))

    if timing:
        print("\n=== timing (s) ===")
        for name, dt in TIMINGS:
            print(f"  {dt:6.2f}  {name}")
        print(f"  {sum(d for _, d in TIMINGS):6.2f}  TOTAL")
    print("\n=== summary ===")
    for case, ok in verdicts:
        print(f"  {case:16s} {'PASS' if ok else 'FAIL'}")
    bad = [c for c, ok in verdicts if not ok]
    print(("FAILED: " + ", ".join(bad)) if bad else "ALL CHECKS PASSED")
    return 1 if bad else 0


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("corpus")
    p.add_argument("--build", action="store_true", help="capture a ground truth and derive the changed cases")
    p.add_argument("--res", default=None,
                   help="depth-source geometry - ONLY valid with --build (default 640x360 there). "
                        "A run always takes its geometry from the case itself (R2).")
    p.add_argument("--bags", type=int, default=2, help="--build only")
    p.add_argument("--slot", action="append", choices=sorted(SLOT_KIND),
                   help="restrict the comparison to one published slot (repeatable). "
                        "Default: every slot with a golden in <corpus>/gt/expected/.")
    p.add_argument("--timing", action="store_true", help="print how long every step took")
    p.add_argument("--serial", default=None,
                   help="serial number of the camera to use; required on a rig with more "
                        "than one RealSense attached (the SDK CI passes the device under test)")
    a = p.parse_args()

    if not a.build and a.res is not None:
        sys.exit("--res is only for --build; a run takes the geometry from the case")

    global SERIAL
    SERIAL = a.serial

    if a.build:
        global TOOL
        TOOL = DEFAULT_TOOL
        from ddt_core.camera import camera_busy
        busy = camera_busy()
        if busy:
            sys.exit("another process holds the camera - close it first, then rerun:\n" + busy)
        build(a.corpus, a.res or "640x360", a.bags)
        return 0

    return run(a.corpus, a.slot, a.timing, a.serial)


if __name__ == "__main__":
    sys.exit(main())
