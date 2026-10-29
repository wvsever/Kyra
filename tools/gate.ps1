<#
  Kyra push gate. pwsh -NoProfile -File tools/gate.ps1 [-Preset msvc-x64-release]
  1. configure + build + ctest via lanetools slot_build.ps1 (VS env, box token, slot, sccache server)
  2. smoke: kyra --version-json, kyra run on the demo entity writes fc_import_AML_ELIGIBILITY.csv and fc_import_AML_RA.csv
  sccache hits/misses: the '[slot_build] ... sccache hits' line (per-repo server; port fixed once lanetools lists kyra)
  Exit 0 only when every check ran and passed. Never claims a check it did not run.
#>
param([string]$Preset = 'msvc-x64-release')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$slot = 'F:/wvs_git_sources/lanetools/scripts/slot_build.ps1'
if (-not (Test-Path $slot)) { Write-Output "[gate] FAIL: $slot not found (Kyra builds through lanetools)"; exit 1 }

Write-Output "[gate] slot_build $Preset (configure, build, ctest)"
& pwsh -NoProfile -File $slot -Worktree $root -Preset $Preset -Test
if ($LASTEXITCODE -ne 0) { Write-Output "[gate] FAIL: slot_build exit $LASTEXITCODE"; exit 1 }

$exe = Join-Path $root "build/$Preset/engine/kyra.exe"
if (-not (Test-Path $exe)) { Write-Output "[gate] FAIL: $exe missing"; exit 1 }
$v = & $exe --version-json
if ($LASTEXITCODE -ne 0 -or "$v" -notmatch '"product":"kyra"') { Write-Output "[gate] FAIL: --version-json: $v"; exit 1 }
$out = Join-Path $root "build/$Preset/gate_smoke"
& $exe run --out $out | Write-Output
$csv = Join-Path $out 'factsconverter/fc_import_AML_ELIGIBILITY.csv'
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $csv)) { Write-Output "[gate] FAIL: kyra run (exit $LASTEXITCODE)"; exit 1 }
& $exe run --out $out --module aml_ra | Write-Output
$csv = Join-Path $out 'factsconverter/fc_import_AML_RA.csv'
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $csv)) { Write-Output "[gate] FAIL: kyra run aml_ra (exit $LASTEXITCODE)"; exit 1 }
Write-Output "[gate] smoke: PASS ($v)"

Write-Output '[gate] PASS'
exit 0
