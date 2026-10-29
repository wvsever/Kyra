# Kyra decisions (K-numbers)

One line each: id | decision | source. "Assumed" = not yet checked against the published text.

K001 | Kyra is a separate product: synthetic AML customer/transaction/controls data + aggregation into aml_ra and aml_eligibility. Elara stays accounting-only. | owner decision 2026-10-29; F:/Temp/eba44/RECOMMENDATION_2026-10-29.txt
K002 | The demo entity is its own designated reporting obliged entity (V_00.01 c0120 true), so V_00.02 and V_01.01-V_04.01 are reportable. | AMLAR (EU) 2024/1620 Art. 12 and the eligibility RTS under Art. 12(7) (assumed); VR v904883-v904888
K003 | Demo profile: BE credit institution, NL EU parent, LU branch, FR payment-institution subsidiary. V_02.02 (non-EU head office) needs a separate profile. | VR v904920_m, v904921_m (parent and non-EU head office are exclusive in practice)
K004 | FPS = customer resident in another EU state than the booking unit. Branch FPS counts for the entity (a branch is part of the legal entity); subsidiaries report their own. V_01.01 c0050 = distinct states over head office + branches. | CRD 2013/36/EU Art. 39; VR v905231_m (1..27)
K005 | Risk factors per AMLR Annexes I-III and EBA ML/TF risk factor GL (EBA/GL/2021/02): customer, product, channel, geography. Risk class mix 70/25/5 low/medium/high, PEP 0.4%. | mix assumed
K006 | Amounts lognormal (median 1.1k EUR, high risk 4.9k), floor 10 EUR, cents; 1-12 transactions per customer. | assumed, no calibration source
K007 | Blank only where a rule makes a cell conditional ("(Other)" columns, c0150 when FPS false, V_02.02 without non-EU head office); all other cells filled, so V_00.04 comments are not needed. | VR v904894_m, v904897-v904899_m, v905245_m+
K008 | V_04.01 c0020 = state where services are provided, c0030 = state of the reported establishment (column headers win over the DPM dimension names). Revisit after the first FC/Yena run. | annotated table layout 4.4; assumed
K009 | V_00.01 is not exported: the FC Regkey map has no V_00.01 keys. Entity LEI travels in the FC header line. | M:/WIP/EBA/EBA 4.4 fase 1/aml_eligibility_v100/Regkey
K010 | Measure = Yena assertions fired and unsatisfied rate (reachable denominator) on FC CSV -> PMR -> Yena; vrule_eval is not a measure. | owner rulings (project_priority_fired_and_unsat_rate)
K011 | Own splitmix64 PRNG and golden CSV: identical output per (profile, seed) on every compiler. | Kyra
K012 | Enumerations are written as the FC workbook dropdown labels (e.g. "BELGIUM", "BE_NBB", "Credit institution"); booleans true/false. | AGU_AML_ELIGIBILITY_v100_i.xltx sharedStrings; Vera FC CSV
