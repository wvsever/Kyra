"""End to end: kyra run -> FactsConverter CSV equals the golden file, per module.
Usage: test_cli.py <kyra.exe> <tests/data> <work dir>. Re-pin after an intended change: KYRA_UPDATE_GOLDEN=1."""
import os, shutil, subprocess, sys
from pathlib import Path

exe, data, work = sys.argv[1], Path(sys.argv[2]), Path(sys.argv[3])
shutil.rmtree(work, ignore_errors=True)
fails = []

def check(ok, what):
    if not ok:
        fails.append(what)

r = subprocess.run([exe, "--version-json"], capture_output=True, text=True)
check(r.returncode == 0 and '"product":"kyra"' in r.stdout, "--version-json")
check(subprocess.run([exe, "run"], capture_output=True).returncode == 2, "run without --out exits 2")
check(subprocess.run([exe, "run", "--out", str(work / "x"), "--module", "nope"], capture_output=True).returncode == 2,
      "unknown module exits 2")

counts = []
for module, family, workbook, sheets in (("aml_eligibility", "AML_ELIGIBILITY", "AGU_AML_ELIGIBILITY_v100_i.xltx", 6),
                                         ("aml_ra", "AML_RA", "AGU_AML_RA_v110_i.xltx", 34)):
    r = subprocess.run([exe, "run", "--out", str(work / module), "--module", module], capture_output=True, text=True)
    check(r.returncode == 0, f"{module}: run exit {r.returncode}: {r.stderr}")
    out = work / module / "factsconverter" / f"fc_import_{family}.csv"
    golden = data / "golden" / f"fc_import_{family}.csv"
    if not out.exists():
        check(False, f"missing {out}")
        continue
    body = out.read_bytes()
    lines = body.decode("utf-8").split("\r\n")[:-1]
    check(lines[0].endswith(f'2026";"12";"Individual";"";"x1";"2026-12-31T00:00:00.0000000";"{workbook}";"'),
          f"{module}: header line")
    keys = {l.split("|")[0] for l in (data / f"{module}_regkeys.txt").read_text(encoding="utf-8").splitlines()
            if l and l[0] != "#"}
    seen = set()
    for l in lines[1:]:
        f = l.split('";"')
        seen.add(f[0])
        check(len(f) == 7 and f[1] in keys and f[4] not in ("", "0", "0.00"), f"{module}: row {l}")
    check(len(seen) == sheets, f"{module}: {len(seen)} templates, expected {sheets}")
    if os.environ.get("KYRA_UPDATE_GOLDEN") == "1":
        golden.parent.mkdir(parents=True, exist_ok=True)
        golden.write_bytes(body)
    check(golden.exists() and golden.read_bytes() == body, f"{module}: differs from {golden} (KYRA_UPDATE_GOLDEN=1 re-pins)")
    counts.append(f"{module} {len(lines) - 1} facts")

print("\n".join(fails) or "ok (" + ", ".join(counts) + ")")
sys.exit(1 if fails else 0)
