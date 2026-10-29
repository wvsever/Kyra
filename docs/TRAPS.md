# Kyra traps

T1 | V_00.01 has no regkeys in the AGU_AML_ELIGIBILITY_v100 Regkey map; its 12 cells and the V_00.01 rules (e905378_e, v904861_m, v904883+ via c0120, v905245_m+) cannot fire through FC until the map gets them (FC fix, not Kyra).
T2 | FC TaxLooper fails on aml_eligibility_v100 (NullReferenceException in XBRLImporter.ConvertContextToConcatTyped, convert.log line 211). Fix in FactsConverter.
T3 | Yena validates the 4.4 AMLA xBRL-CSV with no code change (formula linkbases come from the entry point). PMR needs an AGU that holds the workbook: AGTEST1_20261014 has neither AML module ("Regkey not found" for every row); the owner AGU AGTEST1_20261029_NRC_Full.agu has AML_RA v110 (F:/Temp/eba44/YENA_44_CHECK.txt).
T4 | `lt g` knows kyra (slot build under C:/wc/bslot/kyra<i>).
T5 | Enumeration values are FC dropdown labels, not DPM member codes; country labels are upper case. A wrong label is an FC import error, not a Yena finding.
T6 | Supervisor must match the member state (v904924_m, v905233_m-v905241_m): BE -> BE_NBB/BE_FSMA/BE-FPS Economy.
T8 | PMR accepts W_01.02 / W_01.03 rows without the country key (c0030) but drops them from the xBRL-CSV export; the filing indicator stays and e904551_e_0 then fails. Write comment rows with all three keys (template, column, country), i.e. on W_05.01 / W_06.01 cells.
T7 | ctest runs from the slot path C:/bslot/kyra<i>, not the worktree; tests take paths from arguments, never from cwd.
