# Kyra architecture

Kyra generates synthetic AML data for one reporting obliged entity and aggregates it into the EBA 4.4 phase 1 AMLA
modules `aml_eligibility_v100` (9 templates, 113 rules) and `aml_ra_v110` (34 templates, 414 rules). Output is the
FactsConverter regkey CSV; the route is FC CSV -> PMR -> Yena, as for Vera. Offline, no DB, synthetic data only.

## Pipeline
| stage | code | output |
|---|---|---|
| 1 generate | `engine/src/generate.cpp` `generate(profile, ref_date, seed)` | `Book` in memory, `<out>/book/{units,customers,transactions}.csv` |
| 2 aggregate | `engine/src/eligibility.cpp` `aggregate_eligibility`; `aggregate_ra` (stub, exit 3) | `Module` = list of `Fact{sheet, regkey, ordinal, value}` |
| 3 export | `engine/src/export.cpp` `export_fc_csv` | `<out>/factsconverter/fc_import_<FAMILY>.csv` |

CLI: `kyra run --out <dir> [--entity demo] [--ref-date 2026-12-31] [--seed 42] [--module aml_eligibility|aml_ra]`,
`kyra generate ...`, `kyra --version-json`. Exit 0 ok, 1 failure, 2 usage, 3 stage not implemented.
Deterministic for (profile, seed): own splitmix64 PRNG (K011).

## Data model (`engine/include/kyra/book.hpp`)
| table | fields | feeds |
|---|---|---|
| entity profile / units | LEI, national code, names, AML entity type, member state, supervisor, additional supervisor, group structure, FPS flag; unit 0 = the reporting obliged entity | V_00.x, V_01.01, V_03.01, W_01.01 |
| parent | EU parent or non-EU head office identification, AML supervised | V_02.01, V_02.02 |
| customer | id, booking unit, residence, risk class (low/medium/high), product, channel, PEP | V_04.01, W_02.01, W_03-W_06 |
| transaction | customer, direction, amount EUR, counterparty country | V_04.01, W_03-W_05, W_04.14 |
| controls (planned) | governance, CDD, monitoring, sanctions, group framework answers and counts | W_07.01-W_07.07 |
| qualitative answers (planned) | yes/no and enumerations per question; comments | W_01.02, W_01.03, V_00.03, V_00.04 |
| balance sheet (planned) | total assets; later from Vera/Elara | W_08.01 |

Risk factors (K005): customer (risk class, PEP), product (account, savings, loans, mortgage, payment services, and per W_03/W_04
service line), channel (branch, online, intermediary), geography (residence, counterparty country).

## aml_eligibility map (implemented)
| template | source |
|---|---|
| V_00.01 | units[0]; not exported, no regkeys in the FC map (K009, TRAPS T1) |
| V_00.02 | units[0] (designated reporting obliged entity is the entity itself, K002) |
| V_00.03, V_00.04 | none: every unconditional cell is filled, no comments needed (K007) |
| V_01.01 | parent flags, units count, FPS states over head office + branches (K004) |
| V_02.01 | parent |
| V_02.02 | parent when the head office is outside the EU; empty in the demo profile |
| V_03.01 | units[1..] (one open row each); c0150 = FPS states of that unit |
| V_04.01 | customers resident in another EU state than their booking unit, per (unit, state): count, incoming and outgoing sums (K004, K008) |

## aml_ra map (planned, one slice per wave)
| templates | source |
|---|---|
| W_01.01 basic information | units, parent, customer and transaction totals |
| W_01.02, W_01.03 comments | qualitative answers |
| W_02.01 customers | customers by risk class, PEP, residence, legal form |
| W_03.01-W_03.06 products | customers and transactions per product (accounts, virtual IBANs, prepaid cards, lending, life insurance, cash exchange) |
| W_04.01-W_04.14 services | transactions per service line (investment, remittance, wealth, correspondent, trade finance, e-money, TCSP, crypto, UCITS/AIF, custody, crowdfunding, cash) |
| W_05.01 geographies | customers and transactions by residence / counterparty country |
| W_06.01 distribution channels | customers by channel |
| W_07.01-W_07.07 controls | controls tables |
| W_08.01 balance sheet | balance sheet |

Each entity profile carries only the lines of its business model (no crypto in a car-lease bank).

## Tests and measure
- `tests/unit_tests.cpp` (ctest `unit`): LEI check digits, determinism, every fact has a regkey, no zeros, VR-derived
  checks (v904889, v904892, v904895/6, v904921, v905231).
- `tests/test_cli.py` (ctest `e2e_eligibility`): runs `kyra run`, checks the FC CSV layout and the golden
  `tests/data/golden/fc_import_AML_ELIGIBILITY.csv` (re-pin: `KYRA_UPDATE_GOLDEN=1`).
- Product measure (K010): Yena assertions fired and unsatisfied rate on the FC -> PMR -> Yena route. Blocked until
  Yena loads the 4.4 sets and FC converts aml_eligibility (TRAPS T2, T3).
