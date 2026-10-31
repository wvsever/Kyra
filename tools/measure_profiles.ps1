#Requires -Version 7.0
# Kyra measure: every entity profile -> FC CSV -> PMR -> Yena (lanetools pmr_yena.ps1 script, no lt); prints per profile
# executed/unsat and the union (rules executed in >= 1 profile / reachable). Reachable = rules not gated on the V_00.01
# filing indicator (TRAPS T1, owner decision pending). Usage:
#   pwsh -NoProfile -File tools/measure_profiles.ps1 -Kyra <kyra.exe> -Agu <agu> [-Profiles demo] [-Out dir]
param([Parameter(Mandatory)][string]$Kyra, [Parameter(Mandatory)][string]$Agu, [string[]]$Profiles = @('demo'),
      [string]$Out = "$env:TEMP/kyra_profiles", [string]$PmrYena = 'F:/wvs_git_sources/lanetools/scripts/pmr_yena.ps1')
$ErrorActionPreference = 'Stop'
$executed = @{}; $reach = @{}; $unsat = @{}
foreach ($p in $Profiles) {
    $d = "$Out/$p"
    & $Kyra run --out $d --module aml_eligibility --entity $p | Out-Null
    if ($LASTEXITCODE) { throw "kyra failed for $p" }
    $csv = "$d/factsconverter/fc_import_AML_ELIGIBILITY.csv"
    & pwsh -NoProfile -File $PmrYena $csv --agu $Agu --out "$d/y" | Out-Null
    $pre = "$d/y/fc_import_AML_ELIGIBILITY"
    $gated = @{}
    foreach ($l in Get-Content "$pre.diag.jsonl") {
        if (-not $l.StartsWith('{')) { continue }
        $j = $l | ConvertFrom-Json
        if ($j.t -ne 'assertion') { continue }
        if ($j.reason -eq 'FILING_INDICATOR_FALSE' -and ($j.preconditions | Where-Object { $_.refs -contains 'v_V_00.01' })) { $gated[$j.id] = 1 }
        if ($j.reason -eq 'NO_FACT_FOR_VARIABLE' -and ($j.failed_variable.filters | Where-Object { $_.concept -like '*qCQQ' })) { $gated[$j.id] = 1 }   # qCQQ is a V_00.01 cell
    }
    $n = 0; $e = 0; $u = 0
    foreach ($r in Import-Csv "$pre.rules.csv") {
        $reach[$r.rule] = $true
        if ($gated.ContainsKey($r.rule)) { $reach[$r.rule] = $false }
        $n++
        if ([int]$r.satisfied + [int]$r.unsatisfied -gt 0) { $e++; $executed[$r.rule] = 1 }
        if ([int]$r.unsatisfied -gt 0) { $u++; $unsat[$r.rule] = 1 }
    }
    "{0}: executed {1}/{2}, unsatisfied rules {3}" -f $p, $e, $n, $u
}
$reachable = ($reach.Values | Where-Object { $_ }).Count
$ux = ($executed.Keys | Where-Object { $reach[$_] }).Count
"union: executed {0}/{1} reachable ({2} in scope), unsatisfied rules {3}" -f $ux, $reachable, $reach.Count, $unsat.Count
