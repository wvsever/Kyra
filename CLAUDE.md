# Kyra - CLAUDE.md

Kyra (AML): synthetic AML data -> EBA 4.4 aml_eligibility and aml_ra -> FactsConverter CSV -> PMR -> Yena.
Detail: `docs/ARCHITECTURE.md` (data model, pipeline, template maps, tests), `docs/RULES.md`, `docs/TRAPS.md`,
`docs/DECISIONS.md` (K-numbers). Scripts: `tools/README.md`, suite `F:/wvs_git_sources/lanetools/scripts/README.md`.

## Hard rules
- Synthetic data only; offline; C++ production code, Python only for tests.
- No zeros as data; blank only where a rule makes the cell conditional.
- Gate: `lt g` (until lanetools knows kyra: `pwsh -NoProfile -File tools/gate.ps1`). Push: `lt q submit`, no PR.
- New method choice: K line in docs/DECISIONS.md with the regulatory source.
