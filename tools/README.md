# tools
gate.ps1 | push gate: lanetools slot_build (VS env, box token, slot, sccache port 4241, cache C:\sccache\kyra_bd) + ctest, kyra smoke (slot_build prints sccache hits/misses) | `pwsh -NoProfile -File tools/gate.ps1 [-Preset msvc-x64-release]`
