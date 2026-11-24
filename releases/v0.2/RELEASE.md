# v0.2 - scene-wide START macros (builds on v0.1)
- Base: official SmplTrek 3.2.174 only. Expected output image: 1,956,893 bytes, sha256 `7bef98e8fe9f66ec33de9b30e96de901e072be25a97db72ead6df924e3c75f58`.
- FUNC + knob A = shift every other loop clip in the scene by the same beats; FUNC + knob B = set all loop clips (this one included) to one bar.
- Hardware-verified: v0.1 behaviour and the shift. Emulator-only: set all. Not yet checked anywhere: persistence of the macro-written values after saving and
  power cycling; loops shorter than their scene; scenes that are not 4/4 (bar numbers assume 4 beats).
- set all overwrites each clip's individual offset: try it on a copy of a project.
