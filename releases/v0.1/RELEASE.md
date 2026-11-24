# v0.1 - beat/bar snap and readout (START POS page)
- Base: official SmplTrek 3.2.174 only (source sha256 3b6848b0f041bcf23c73d4b0357d2f3d6dc9e7e03752b32ff7322e91e6814e53).
- Expected output image: 1,955,453 bytes, sha256 `e1e5aabc72047bbe238756ff8ba2641b61ab80272b042e9a58cd75a416c97f0b`.
- payload.bin sha256: see SHA256SUMS in this folder. Differences from the official file: 23 header/pointer bytes plus the appended section.
- Verified on hardware (one SmplTrek, BOOT 1.0.17): boots, projects load, playback normal, START snaps to beats/bars, the readout shows.
