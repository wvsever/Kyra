# Kyra traps

T1 | V_00.01 has no regkeys in the AGU_AML_ELIGIBILITY_v100 Regkey map; its 12 cells and the V_00.01 rules (e905378_e, v904861_m, v904883+ via c0120, v905245_m+) cannot fire through FC until the map gets them (FC fix, not Kyra).
T2 | FC TaxLooper fails on aml_eligibility_v100 (NullReferenceException in XBRLImporter.ConvertContextToConcatTyped, convert.log line 211). Fix in FactsConverter.
T3 | Yena does not load the 4.4 sets yet (engine + family table = Yena core change, owner approval).
T4 | `lt g` and `lt q` print "unknown repo 'kyra'" until lanetools registers the kind; run `pwsh -NoProfile -File tools/gate.ps1` meanwhile.
T5 | Enumeration values are FC dropdown labels, not DPM member codes; country labels are upper case. A wrong label is an FC import error, not a Yena finding.
T6 | Supervisor must match the member state (v904924_m, v905233_m-v905241_m): BE -> BE_NBB/BE_FSMA/BE-FPS Economy.
T7 | ctest runs from the slot path C:/bslot/kyra<i>, not the worktree; tests take paths from arguments, never from cwd.
