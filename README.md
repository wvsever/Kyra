# Kyra

AML reporting data product: generates synthetic AML data for a reporting obliged entity and aggregates it into the
EBA 4.4 AMLA modules aml_eligibility and aml_ra, exported as FactsConverter CSV.

    pwsh -NoProfile -File tools/gate.ps1          # build (sccache), tests, smoke
    build/msvc-x64-release/engine/kyra.exe run --out <dir>

Status: aml_eligibility end to end for the demo entity; aml_ra planned. Design: `docs/ARCHITECTURE.md`.
