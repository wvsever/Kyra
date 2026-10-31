# tools
gate.ps1 | push gate: lanetools slot_build (VS env, box token, slot, sccache port 4241, cache C:\sccache\kyra_bd) + ctest, kyra smoke (slot_build prints sccache hits/misses) | `pwsh -NoProfile -File tools/gate.ps1 [-Preset msvc-x64-release]`
measure_profiles.ps1 | every profile -> FC CSV -> PMR -> Yena (lanetools pmr_yena.ps1), per-profile and union executed/reachable (V_00.01-gated rules excluded) | `pwsh -NoProfile -File tools/measure_profiles.ps1 -Kyra <kyra.exe> -Agu <agu> [-Profiles demo]`
