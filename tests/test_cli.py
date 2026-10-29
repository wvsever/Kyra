"""End to end: kyra run -> FactsConverter CSV equals the golden file. Usage: test_cli.py <kyra.exe> <tests/data> <work dir>
Re-pin after an intended change: KYRA_UPDATE_GOLDEN=1."""
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
check(subprocess.run([exe, "run", "--out", str(work / "ra"), "--module", "aml_ra"], capture_output=True).returncode == 3,
      "aml_ra stub exits 3")

r = subprocess.run([exe, "run", "--out", str(work / "elig")], capture_output=True, text=True)
check(r.returncode == 0, f"run exit {r.returncode}: {r.stderr}")
out = work / "elig" / "factsconverter" / "fc_import_AML_ELIGIBILITY.csv"
golden = data / "golden" / "fc_import_AML_ELIGIBILITY.csv"
if out.exists():
    body = out.read_bytes()
    lines = body.decode("utf-8").split("\r\n")[:-1]
    check(lines[0].endswith('2026";"12";"Individual";"";"x1";"2026-12-31T00:00:00.0000000";"AGU_AML_ELIGIBILITY_v100_i.xltx";"'),
          "header line")
    keys = {l.split("|")[0] for l in (data / "aml_eligibility_regkeys.txt").read_text().splitlines() if l and l[0] != "#"}
    for l in lines[1:]:
        f = l.split('";"')
        check(len(f) == 7 and f[1] in keys and f[4] not in ("", "0", "0.00"), f"row {l}")
    if os.environ.get("KYRA_UPDATE_GOLDEN") == "1":
        golden.parent.mkdir(parents=True, exist_ok=True)
        golden.write_bytes(body)
    check(golden.exists() and golden.read_bytes() == body, f"differs from {golden} (KYRA_UPDATE_GOLDEN=1 re-pins)")
else:
    check(False, f"missing {out}")

print("\n".join(fails) or f"ok ({len(lines) - 1} facts)")
sys.exit(1 if fails else 0)
