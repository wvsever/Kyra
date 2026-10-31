# Kyra architecture

Kyra generates synthetic AML data for one reporting obliged entity and aggregates it into the EBA 4.4 phase 1 AMLA
modules `aml_eligibility_v100` (9 templates, 113 rules) and `aml_ra_v110` (34 templates, 414 rules). Output is the
FactsConverter regkey CSV; the route is FC CSV -> PMR -> Yena, as for Vera. Offline, no DB, synthetic data only.

## Pipeline
| stage | code | output |
|---|---|---|
| 1 generate | `engine/src/generate.cpp` `generate(profile, ref_date, seed)` | `Book` in memory, `<out>/book/{units,customers,transactions}.csv` |
| 2 aggregate | `engine/src/eligibility.cpp` `aggregate_eligibility`; `engine/src/ra.cpp` `aggregate_ra` | `Module` = list of `Fact{sheet, regkey, ordinal, value}` |
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
| V_02.02 | parent when the head office is outside the EU; non-EU head office, filed in the demo next to the EU parent (K023) |
| V_03.01 | units[1..] (one open row each); c0150 = FPS states of that unit |
| V_04.01 | customers resident in another EU state than their booking unit, per (unit, state): count, incoming and outgoing sums (K004, K008) |

## aml_ra map (implemented, `engine/src/ra.cpp` `aggregate_ra`; 34/34 templates)
Scope = head office + branches (K004). Customer flags are quotas from the second PRNG stream (`enrich_aml`, K013).
| template | source |
|---|---|
| W_01.01 | units[0]; every product/service flag true (K020) |
| W_01.02, W_01.03 | one comment and two not-applicable rows on W_06.01 / W_05.01 blanks (TRAPS T8) |
| W_02.01 | customers: legal form, PEP, onboarding flags, complex structure, high-risk activity/BO, non-EEA counterparties, FIU requests; walk-in from `AmlActivity` |
| W_03.01 | transfers of current-account / payment-services / vIBAN customers |
| W_03.02 | transfers of vIBAN master customers; re-issued vIBANs from `AmlActivity` |
| W_03.03 | prepaid holders (1 card, 4 for multi-card holders, average load 240 EUR) |
| W_03.04 | consumer loan / mortgage customers (`aml.loan`), 12 repayments a year; factoring customers |
| W_03.05 | insured customers' premiums; surrender value from `AmlActivity` |
| W_03.06 | CurrencyExchange transactions (outgoing = sell) |
| W_04.01, W_04.03 | rto / custody / portfolio flags, MiFID professional, AUM; non-EEA corporates (K016) |
| W_04.02, W_04.05, W_04.06, W_04.14 | Remittance, TradeFinance, EMoney, Cash transactions (K014) |
| W_04.04 | `AmlActivity.respondents` |
| W_04.07, W_04.09, W_04.10, W_04.12 | tcsp, crypto_custody, crypto (value per customer, operation counts K015), safe_box flags |
| W_04.08, W_04.13 | `AmlActivity` counts |
| W_04.11 | fund investor buckets, fund investors only (about 5% of natural persons, 3% of legal entities, K018); fund AUM from `AmlActivity` |
| W_05.01 | per country: customers by residence, transactions and remittances by counterparty country, respondents, establishments (K021) |
| W_06.01 | `AmlActivity.distribution` |
| W_07.01-W_07.07 | controls (K022): risk classes 4-scale (K017), CDD flags, alerts from transactions, outbound transfers |
| W_08.01 | total assets = outstanding loans / 0.62 (K019) |

Each entity profile carries only the lines of its business model (no crypto in a car-lease bank); the demo
profile is the coverage demo and declares every line (K020).

## Tests and measure
- `tests/unit_tests.cpp` (ctest `unit`): LEI check digits, determinism, every fact has a regkey, no zeros, VR-derived
  checks (v904889, v904892, v904895/6, v904921, v905231); aml_ra: 34 templates, regkeys and types
  (`tests/data/aml_ra_regkeys.txt`), percentages in (0,1], dates <= ref date, cross-template rules (v903768,
  v904930, v904964, v904976-v904983, v905230, v903846, v903919, v904974, v904958, v904960, v903822, v903748).
- `tests/test_cli.py` (ctest `e2e`): runs `kyra run` per module, checks the FC CSV layout, regkeys, template count
  and the goldens `tests/data/golden/fc_import_{AML_ELIGIBILITY,AML_RA}.csv` (re-pin: `KYRA_UPDATE_GOLDEN=1`).
- Product measure (K010): Yena assertions fired and unsatisfied rate on the FC -> PMR -> Yena route (TRAPS T3).
  aml_ra demo seed 42, 2026-10-29: PMR accepted 368/368, Yena 383 evaluated, 383 satisfied, 0 unsatisfied.
