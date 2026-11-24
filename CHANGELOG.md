# Changelog

## 1.00b-r1

- First Switch release of Touhou 17 on the eagler-touhou TH17 runtime
  (`ports/th17` at the commit in `UPSTREAM_COMMIT`).
- One game tick per vsync; no catch-up bursts after slow frames.
- The next stage's files are prefetched on another core: the stage change
  frame is 38% shorter.
- `th17-switch.log` records start-up, memory allocated per stage and slow
  frames (over 25 ms, at most 20 lines a minute).
- Music from `thbgm.dat`; text from `msgothic.ttc` kept in memory.
- Fixed for native GLES: the staged renderer's vertex stream offsets (the
  gameplay screen was black), a reserved GLSL name Mesa rejects, and a GCC
  aggregate assignment.
