# Notices

Thanks to Sonicware, who designed and built the SmplTrek and did the heavy lifting this project depends on, and to seawaffle for the open reverse-engineering
work credited below. This is an independent hobby project; it ships no Sonicware firmware, and rights holders are welcome to raise any concern through an issue.

- **No Sonicware code is distributed here.** `releases/*/payload.bin` is machine code compiled from `src/payload.c` in this repository
  (rebuild it with `scripts/build_payload.sh`; it is byte-identical). The firmware update you flash is produced on your machine from your own official file.
- From v0.3 the payload also contains two small register save/restore routines from GCC's libgcc (GPL with the GCC Runtime Library Exception).
- `vendor/sonicware_integrity.py` is from [seawaffle/smpltrek-re](https://github.com/seawaffle/smpltrek-re), MIT, copyright (c) 2026 seawaffle.
  Its licence text is kept in `vendor/LICENSE-smpltrek-re.txt`.
- The firmware addresses, structure names and behaviours in `src/` and `docs/` come from our own analysis of the public 3.2.174 update file,
  from emulator and hardware observation, and from the symbol files and write-ups in smpltrek-re (credited where used).
- Our tests used the emulator from smpltrek-re; the beat-grid maths is our own. [seawaffle/enaga](https://github.com/seawaffle/enaga) (MIT) was read for
  hardware reference only; nothing is copied from it.
- Sonicware, SmplTrek, Lofi-12 XT and related names are trademarks of their owners. This project is unofficial and unaffiliated.
