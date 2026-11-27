# SmplTrek 3.2.174 firmware: what this project learned

Purpose: everything we know that is needed to extend this patch, in one place (facts, addresses, how each was checked).
Confidence tags: [HW] seen on the real unit, [EMU] seen in the emulator on stock firmware, [STATIC] read from
disassembly only, [REPO] taken from github.com/seawaffle/smpltrek-re (not independently verified by us).
Target: SmplTrek SYSTEM 3.2.174 (sha256 3b6848b0f041bcf23c73d4b0357d2f3d6dc9e7e03752b32ff7322e91e6814e53).
Test unit: BOOT 1.0.17, PRESET 2.0.22, MCU 1.0.6 (one SmplTrek).

## 1. How an update image works
- Container 'cmtd' (outer) holding 'mmtd' (system) sections; raw, unencrypted. Header CRCs are zlib CRC-32 with
  per-product seeds, children first then outer. [HW: resealed modified images accepted by Lofi-12 XT and SmplTrek]
- Tools: vendor/sonicware_integrity.py (verify / reseal) [REPO, MIT]; tools/build_image.py (ours).
- A section can be appended (we use 0xC3F00000, ~1.8 KB). Region 0xC16FFA98..0xC4000000 is unclaimed; heap starts at
  0xC4000000. [HW: works for our payload]
- Recovery: HOME + power = BOOT MODE (bootloader reads HOME before loading the system image) -> SYSTEM UPDATE -> official
  file. The unit also needs AC power / batteries OUT when risky code may hang. [REPO, HW partly]
- Update procedure (SmplTrek): file named SmplTrek.bin in card root; hold HOME + power; SYSTEM UPDATE -> OK; YES -> OK. [HW]

## 2. Hooking method (what we do)
- Redirect vtable words so the page calls our wrapper, which calls the stock function first. No stock code is edited.
- C6000 EABI: args A4,B4,A6,B6,A8,B8; callee-saved A10-A15,B10-B15; B14 = data page (DP), B15 = SP.
  Our code must not write B14. Fixed-offset DP reads/writes are done with inline asm (ldw .D2T1 *+B14(n)).
- Divide: use firmware __c6xabi_divu 0xC1690640 (no libgcc, no `%`).

## 3. Class / vtable facts
- AppLoopTrackView (clip EDIT page, also hosts the START POS sub-page): vptr 0xC16C5978 (RTTI table 0xC16C5970).
  slot 7 handleEncoder(this, enc, delta) = word 0xC16C5994 (stock 0xC1560350); draw(this, canvas) = word 0xC16C59B0 (stock 0xC1561554).
  Slot index = (word address - vptr)/4, 0-based. Controller render calls slot 12 (0xC15D3638); title is slot 15.
- AppAudioTrackPositionSetting (vptr 0xC16C64B8) exists but is NOT the START POS page (dead end we followed first).
- Scene pages: AppSceneSetting, LoopSceneSettingTop, Drum/Inst/MIDI/Shots SceneSettingTop, AppSceneMenu, SceneManagerMenu,
  DialogSceneCopy/Clear. [STATIC names only; not analysed yet]

## 3b. Scene settings page (SCN SETTING) [EMU+STATIC, ]
- Reached from the home grid: select the scene header (UP from the track rows), then OK. Shows SCNn SETTING with four knob items
  A BAR, B TIME SIG, C REPEAT, D ENABLE (page 1/2; page indicator dots and a softkey '2/2'); arrows move the highlighted item.
- Class AppSceneSetting, vptr 0xC16C5838 (table 0xC16C5830). Words: slot 4 (button) 0xC15C380C, slot 7 (encoder) 0xC15C30A0,
  slot 12 render 0xC15D3638 (shared base), slot 14 0xC15D3A84 (shared with AppLoopSceneSetting). Encoder 4 (VALUE) moves the selection
  through an item list (index at this+... +52, count +48); the page looks table-driven (generic parameter items), so a new item may be an
  extra table entry - NOT yet confirmed.
- Scene data: 4 bytes/scene in PJSC [ENABLE][REPEAT][BAR][TIME SIG], see section 6. No spare bytes.
- Object layout [STATIC, constructor 0xC15C3C74 + emulator]: ONE static global instance at 0xC14EB1A0, size 1528 B =
  base part (+0..+75) + 2 page records x 28 B at +76 + 6 item records x 232 B at +136 (BAR, TIME SIG, REPEAT, ENABLE, COPY TO, CLEAR).
  It ends EXACTLY at 0xC14EB798 = g_loop_tracks: there is NO slack, so a 7th item cannot be added in place.
  Only 3 code sites build its address (0xC151CC00 startup ctor, 0xC151F1D4 and 0xC1520090 users), so relocating a bigger copy into
  our own memory is possible in principle but needs item ctor/label/draw/count constants to be replicated: a big job.
- Cheaper UI for a scene-wide macro: reuse the START POS page hooks we already own (e.g. FUNC held + knob A; FUNC = bit 19 of
  g_control_switch_state 0xC1501DF0).

## 4. START POS sub-page (clip EDIT > push B) [EMU+HW]
- State: *(DP+300)==1 while the sub-page is open (0 after OK). *(DP+296) = track index of the clip. *(DP+304)==1 right after B
  toggled the page to an 'END:' read-out (timed), reset to 0 by the next knob-A turn. *(DP+1240)=60 after an edit (timer).
- Pending START = view object 0xC1503810 word +24 (frames @48 kHz). Knob A (encoder 0) edits START only; B shows END (read-out).
- Stock click = file_frames/128; stock max = file_frames - 3000; file_frames = 0xC15C56E4(track).
- Stock commit sequence per click: STORE 0xC155FF50(value) [-> 0xC15C5740, table set 0xC15BC938] ; view+24 = value ;
  obj = refresh 0xC155F36C(track) [RETURNS the clip object] ; apply 0xC1569680(obj,value,1) ; refresh ; preview 0xC1568970(obj,value,0.25f).
  Passing the track index where obj is expected faults (unaligned load at 0x21). Skipping STORE leaves the next click's base stale.
- OK commits and closes; CLR does nothing (stock). [HW]
- Clip region in the project file is START/END (END = file end when untrimmed). END does not move with START. [EMU]

## 5. Playback behaviour [EMU+HW]
- Clip plays from START; at the end of its wave it does NOT wrap: silence until the scene (BAR) ends. [HW]
- On scene REPEAT or next pass the clip restarts at START. [EMU]
- Header top-right 'nnn/BBB' = sequencer bar / scene length (BAR). Independent of clip START. [HW]
- Drum track (sequence) is unaffected by clip START. Patched START=0 audio == stock audio (corr 1.000). [EMU]
- Grid used by the patch: beat = file_frames/(bars*4) with bars = nearest whole bars at project tempo; falls back to project
  tempo if >2% off. 4/4 assumed. Tempo = project_get_tempo_x10 0xC1611428 (BPM*10; global 0xC1500CF4).

## 5b. Scene length vs START (tester observations on the unit) [HW]
- With 128-bar loops: scene BAR 64 -> playback cuts the 128-bar loop at 64 and loops; with a clip START set (single clip or v0.2 shift),
  the 64-bar window is offset from the START position within the original 128-bar wave. Scene length = window length, START = window offset.
- v0.2 shift worked on the unit: track 2's START synced with track 1's.
- NOT yet seen: a physical loop SHORTER than the scene (none available yet) - behaviour at the wave end for a naturally short loop is unknown
  (a shifted clip that runs out goes silent; no wrap). Idea parked: optional 'tape-loop wrap' (continue from the wave start) - needs the playback
  engine's end-of-stream handling (not located).

## 5c. Time signature (reasoning, not tested on other signatures)
- TIME SIG is a per-scene setting that mostly affects the sequencer grid; the wave plays back as recorded (no stretching), so a loop recorded in
  one signature plays unchanged under another. Effect on our patch: the BEAT grid comes from the wave/project tempo, so beat steps stay right;
  only the bar GROUPING (the 'B<bar>.<beat>' readout, the bar-sized jump on long loops, set-all rounding to bars) assumes 4 beats per bar.
  Fix later if needed: read the scene's TIME SIG record byte instead of the constant 4.

## 6. Project file format (Config) [partly STATIC]
- 'STPJ' header (16 B) then chunks tag(4)+size(4, includes header)+data: PJST (136), PJSC (scenes), TRDR (drum, 11484),
  TRLP (loop, 4756; 16 x 'LPDT' 292 B), TRGL, TRSQ.
- PJSC (88 B) = 8-byte chunk header + 'SCDT' sub-chunk (size 80): 8 bytes (0,1) then 16 scene records x 4 bytes, ALL BYTES USED:
  [ENABLE][REPEAT][BAR][TIME SIG code]  (for example 01 01 08 02 = ON, repeat 1, 8 bars, 4/4; 01 02 03 02 = ON, repeat 2, 3 bars, 4/4).
  Byte order inferred from values; time-sig code 2 = 4/4. [probable]
- LPDT per-clip START/END in frames @48 kHz (END = frames-1 when untrimmed).
- Runtime clip table: base 0xC13E0000 + scene*4416 + track*276 (+23432 / +23436 / +23440 fields) and a second table near 0xC13DC048
  (START 0xC13DC048, END +4 for scene 0 / track 1). Accessors 0xC15A5CD4 / 0xC15A5D58 / 0xC15A5DF8 / 0xC15A5E24.

## 7. Display
- 128x128, 4-bit grey. draw_text(canvas, const char *s, const int xy[2], grey, mode) at 0xC166F4E0; canvas = 0xC14B8400.
  xy[1] is the glyph top minus ~4; ~6 px per character; START line is at y=100, free band x>=92. [EMU]

## 8. Emulator (smpltrek-re/emulator) on macOS
- Needs patched binutils 2.42 (compact-RS fix), --with-system-zlib, an `nproc` shim, host clang. Clear work/emulator/decode-cache
  after swapping objdump. Panel keys: 17 up, 25 down, 24 left, 18 right, 23 OK, 30 CLR, 22 PLAY, 29 STOP, 19 FUNC, 27 HOME,
  26 REC, 20 PROJECT, 21 MIXER, 28 EDIT, 0 = push B, 1 = push A (encoders 0-3 = A-D, 4 = VALUE). [EMU]
- Cards: media.build_card(files={path: local}, size_mb<=1024, long_names=True). Project load needs a Project.lst. A project with
  1.7 GB of audio hung at the boot logo in the emulator (also on stock) - use smaller projects.
- Audio: 65,536-word rolling capture; poll every 60M cycles and stitch by transmitted_words; compiled blocks make it ~10x faster.
- Breakpoint tracing: cpu.breakpoint(addr) + cpu.run(); step over by disabling the breakpoint.

## 9. Toolchain on macOS (arm64)
- binutils 2.42 + repo patch (tools/toolchain/patches), GCC 13.3.0 tic6x-elf C-only; both need --with-system-zlib (bundled zlib
  fails against the macOS SDK). Compile payload with -O2 -ffreestanding -fno-pic -nostdlib; link with src/link.ld.
- Our early disassembly with binutils 2.43.1 mis-decoded ~573 compact instructions (wrong registers); re-done with the patched tool.

## 10. Pitfalls / lessons
- Verify the right page class in the emulator before hooking (we hooked the wrong class first).
- Compare patched vs stock in the emulator with the same input: caught two bugs (wrong object pointer, missing STORE call).
- The firmware stores state in DP-relative globals; find them by diffing memory before/after one input.
- Real-unit surprises can be hardware (a damaged button looked like a firmware fault). Always A/B against the official firmware first.

## 10a. Clip table (measured [EMU], a factory demo project with four loop tracks)
- Per clip record at 0xC13DAF08 + TRACK*4416 + SCENE*276 : [+0 START frames][+4 END frames, 0 = empty slot][+8 ?][+12 ?].
  (Track stride is 4416, scene stride 276; the accessor 0xC15BC938/0xC15BC9A4 take (track, scene).) Empty clip: END == 0.
- Engine copy per loop track: objects at 0xC14A63B4 + 224*track (START at +0) - updated by apply 0xC1569680(obj,...); the table by STORE 0xC155FF50.
- A macro writes another track's clip by setting *(DP+296)=track, STORE(v), obj=refresh(track), apply(obj,v,1), refresh, then restoring DP+296.
- file length for the grid: 0xC15C56E4(track) (works for any track); in the test project it was 269,120 while END+1 = 269,474.

## 10b. v0.2 scene-wide macros
- Design: SHIFT (every other loop clip in the scene moves by the same beats, keeping offsets) is the main action; SET ALL (every loop clip, this one
  included, to the same bar of ITS OWN grid; clips shorter than the bar are skipped) is on a second key combo. FUNC (bit 19 of the 64-bit word at
  0xC1501DF0) held + knob A = shift; + knob B = set all. Per-track clamping is deliberate: a clip pinned at 0 or its last valid position does not stop the others.
- Loop tracks are found with g_track_types 0xC16ED768 (int[15]): LOOP = 1, DRUM = 4. Empty clip slot = END 0 in the clip table (section 10a).
- Reset a scene to zero: bring one clip to 0 with knob A (no FUNC), then FUNC + knob B.
- Emulator (a factory demo project, 4 loop tracks, 171 BPM): shift moved 3 clips together and back, offsets kept after an independent edit, set-all put all on one bar,
  other scenes untouched, the engine's START copies matched the saved table. Hardware: the shift synced a second clip to the first.

## 10c. Extending the firmware: new pages and DSP (research notes, nothing built yet)
- A page is an ApplicationController descendant (177 in 3.2.174). Instances are static objects built at start-up; vtable slots we know:
  0 open, 1 close, 4 buttons, 7 encoders (enc, delta), 12 render (shared base 0xC15D3638), 14/15 draw. Opening a page = push call
  0xC15F69C8(parent, child) (seen in the switch at 0xC1679380 that opens a page object). So a NEW page = our own vtable + object in our
  section + a hook that calls the push (e.g. a key chord caught in an existing wrapper). Not tried yet; a minimal test page is the first experiment.
  Constraint: stock page objects are packed (AppSceneSetting ends exactly at g_loop_tracks), so new things must live in OUR memory.
- DSP: the stock mixer already has per-track EQ (types, FREQ, AMOUNT/GAIN; manual p.144-147), master effects and insert effects. We have NOT
  located the stock EQ/mixer/audio-render code. A 'legit' new EQ = find the per-track or master processing point, run biquads there
  (C674x, 48 kHz; the cost of e.g. 4 bands x 10 tracks is small), keep it real-time safe (no allocation/files in the audio path;
  Enaga's audio callback is every 64 frames = 1.33 ms), store its settings OUTSIDE the project file (no spare bytes) e.g. a sidecar on the SD.
  The emulator's audio capture can verify filter response before any unit test.
- Enaga (github.com/seawaffle/enaga, MIT) is a clean-room replacement firmware/app platform for the same hardware (display, keys, encoders, SD, audio
  callback, C++20 apps). It is a different route (a whole new app, no stock sequencer/loops) but is useful reference documentation for hardware APIs.
  Separate community apps (for example a groovebox) are built on Enaga; they are unrelated to this patch.

## 10d. Audio engine facts that matter for an EQ [REPO docs/06-audio-engine.md; not yet verified by us]
- Render callback audio_render_main 0xC159AC24 (B14+904), every 128-frame block (2.67 ms) in a Swi; blocks are FLOAT planes (+4 frames, +16 L plane, +528 R plane).
  Mix order: input -> audio_mix_main 0xC159A0C0 -> per track audio_track_render 0xC15A9E6C -> sends/effects -> master effect -> out.
- Per-track EQ today = ONE biquad per channel: g_mixer_channels 0xC14D60F0, 30 biquads x 120 bytes (entry 2*track = dry L, 2*track+1 = dry R),
  applied in audio_track_render AFTER the track render/insert effect and BEFORE meters/balance; sends leave before the EQ.
  Coefficients come from 0xC15C5C2C(track, type, freq, amount, ...): types 1 HP, 2/5 shelf-like, 3 BP, 4 peaking, 6 LP (names inferred); fields
  +36/+40 a1,a2 and +44/+48/+52 b0,b1,b2. Setters track_set_eq_type/freq/amount 0xC154E54C/0xC154DBB8/0xC154DB34. There is no Q control.
- Master effect rack g_master_effects 0xC14C2400: AnalogRecord, CassetteTape, MultiBandCompressor, EQandLimiter, Maximizer. Send rack g_send_effects 0xC14C421C.
- Ideas this opens (nothing built): (a) replace/extend the coefficient design 0xC15C5C2C for true parametric peaking with Q;
  (b) a wrapper after audio_track_render that runs extra cascaded biquads on the dry block (more bands per track);
  (c) reuse Enaga's MIT DSP library (include/enaga/dsp/filter.hpp: Biquad, Audio-EQ-Cookbook peak/shelves, C API enaga_biquad_*) for the maths.
- Enaga 0.3.0: UI toolkit (Grid, PadMap, FileBrowser, ParamPage), USB drive + USB audio working on a unit, SDK zip + prebuilt toolchain.
  smpltrek-re added static profiles of other Sonicware products (docs 14-17). 

## 11. Open questions
- Where the sequencer maps scene time -> clip position (not located). Needed for any scene-level start feature.
- Time-signature handling beyond 4/4 (field is in the scene record; code not read).
- Whether the stock Config parser/writer tolerates extra chunks or longer scene records (needed for new persistent scene fields).
- CLR revert, readout hiding (cosmetic options). END snapping dropped: the sample-edit screen has no editable END; the length lives on the scene page (BAR).

## Added with v0.3
- Live clip record (loop settings), track t 0..9, scene s 0..15: 0xC143A6E8 + t*140 + s*1400. Byte +2 = SEQ LEN (1..128), byte +4 = NOTE code
  (0=1/1, 1=1/2, 2=1/4., 3=1/4, 4=1/8., 5=1/4T, 6=1/8, 7=1/16., 8=1/8T, 9=1/16, 10=1/32); byte +5 is a different field. The unit's SAVE writes
  these to the project. [EMU; LEN/NOTE meaning also HW]
- Project file: TRSQ loop chunk (2512 B) = 16-byte header + 16 scene records of 156 B; LEN at chunk byte 34+156*scene, NOTE at 36+156*scene.
  The loop wraps playback at LEN x NOTE regardless of the audio length. [EMU]
- Scene words (data page DP = 0xC16F3320): DP+680 = scene auto-advance (0 = hold; entering EDIT during playback clears it until STOP), DP+684 = scene
  cursor (follows the song while playing), DP+688 = pass counter of the scene REPEAT. [EMU, hold/release HW]
- Song loop range: last three int32 words of the 136-byte PJST chunk = on, first scene, last scene (0-based). Unit: PROJECT > SETTINGS > PLAYBACK. [EMU+HW]
- Playback objects: two banks per track, consecutive scenes alternate. g_loop_tracks (0xC14EB798 + 72*n): word +8 = play state (2 idle, 32 playing,
  64 clip ended; bank B entry = track + 10), word +36 = pointer to the track's playback record; bank A object = record+68, bank B = +180
  ([0] start, [1] end, [2] position, about 4096 frames ahead of the audio). Records are not always in track order. [EMU]
- Clip table: 0xC13DAF08 + track*4416 + scene*276 = [START][END]. [EMU]
- AppLoopTrackView slot 1 (word 0xC16C597C, stock 0xC15608B4) runs when the page closes. [EMU+HW]
- A payload of 7,952 bytes at 0xC3F00000 has run on a unit. [HW]

