# Port pitfalls: bug classes we hit in plethron and stadion

[Back to the porting notes](README.md)

Each class has: **Symptom** (what you see on screen or in a log), **Find** (how to list every instance, not just the
one you tripped over), **Fix** (what we did) and **Example** (a real case, with the PR). Plethron PR numbers are
`plethron#N`, libultraship fork PRs are `libultraship#N`.

The general rule from all of these: when you fix one instance, write or run a tool that finds the whole class, then
fix or triage every hit. A single fix by hand usually leaves twenty more.

Contents:

1. [Struct layout: N64 offsets on host structs](#1-struct-layout-n64-offsets-on-host-structs)
2. [Pointers stored in 32-bit integers](#2-pointers-stored-in-32-bit-integers)
3. [Prototype and alias mismatches](#3-prototype-and-alias-mismatches)
4. [Byte order](#4-byte-order)
5. [The data lifter (data that is not in C)](#5-the-data-lifter-data-that-is-not-in-c)
6. [Missing return values](#6-missing-return-values)
7. [Buffers sized for the N64](#7-buffers-sized-for-the-n64)
8. [Code that got lost on the way](#8-code-that-got-lost-on-the-way)
9. [Hardware registers and timing](#9-hardware-registers-and-timing)
10. [Renderer (libultraship / Fast3D) differences](#10-renderer-libultraship--fast3d-differences)
11. [Windows, MinGW and macOS only bugs](#11-windows-mingw-and-macos-only-bugs)
12. [Uninitialised values the N64 got "for free"](#12-uninitialised-values-the-n64-got-for-free)
13. [Things that are not bugs](#13-things-that-are-not-bugs)
14. [Test harness artefacts](#14-test-harness-artefacts)

---

## 1. Struct layout: N64 offsets on host structs

The decompiler (m2c) often writes field access as a byte offset from a `s8*`/`u8*` or `s32` base:
`*(u16*)(arg0 + 0x7C2)`. On the N64 that offset is right. On a 64-bit host every pointer in the struct is 8 bytes,
so every field after the first pointer moves, and the offset now lands in a different field.

**Symptom.** Flags or values that never "take": a state machine that waits forever, a cursor that never shows, a
screen that ignores the first press. Nothing crashes. This was the most common cause of softlocks.

**Find.**
- `tools/raw_offsets.py collect` then `report` (plethron#31). It lists byte-pointer parameters read at raw offsets
  and compares them with the struct types the callers pass (`MISMATCH`), plus byte views of host-order words
  (`NARROW`, see byte order below). Rerun after every decomp sync.
- `tools/decomp_audit/offsets.py` asserts every `M2C_FIELD` rewrite against a MIPS32 big-endian layout.
- grep for local structs the decomp declared "partial" (`Layouts below are partial and local`): they carry N64
  offsets too.
- `typed_ptr + 0xNN` is a variant: it scales by `sizeof(*ptr)`. Replace with the field.

**Fix.** Use the real struct type and its field. If the struct is not typed yet, type it.

**Examples.**
- Dugtrio's Dig softlock (plethron#31): `ActivePokemon_SetStateFlags` wrote `state.flags` at N64 offset 0x7C2 through
  `s8*`. The host field is at 0x8B6, so every `ACTIVE_*` flag landed in `pokeBall` and the second Dig turn waited
  forever. Not an endianness bug at all.
- Lab screens (plethron#51): いちらん ignored the first A (species read at N64 byte 8 of `LabPCEntry`), no row cursor,
  items all blue: three local structs with N64 offsets.
- GB Player (plethron#19): N64 byte offsets 0x9D-0x9F on `GbPakChannel` corrupted the mapper state, 0x99/0x9A hung
  the C-Up menu. Any `(u8*)ch + 0x..` in GB code is a suspect.
- Battle 64-bit cleanup (plethron#3/#6): strides through `s8*` bases into `ActivePokemon`, `Particle`,
  `CameraNode`, `ComBattler` (0x90), `Controller` (0x24).
- Audio Nas driver (plethron#62): `Note +0x80/+0x90` via `s8*`. That code turned out never to run (see class 13),
  so check that code runs before spending time on it.

## 2. Pointers stored in 32-bit integers

**Symptom.** Segfault, usually far from the cause, or a pointer that works on Linux by heap-layout luck and crashes
on Windows or Android. Things that "almost work" (a quit segfault, a menu crash on one platform).

**Find.**
- Rebuild one file at a time with the 64-bit warnings on. Take its command from `ninja -t commands`, replace the
  `-Wno-int-conversion -Wno-incompatible-pointer-types ...` with
  `-Wint-conversion -Wno-error=int-conversion -Wpointer-to-int-cast -Wint-to-pointer-cast -Wshorten-64-to-32
  -Wincompatible-pointer-types`, output to `/dev/null`. Keep warnings that mention `s32`, `u32` or `int`.
- `tools/ptr_warnings.sh`, `tools/intconv.sh` (stadion and plethron). The plethron audit list
  (`plethron/audits/int-to-pointer.txt` in the shared files) was generated with `-fsyntax-only -Wno-everything
  -Wint-conversion` over `compile_commands.json`.
- Run an AddressSanitizer build (`cmake -DPORT_ASAN=ON`, needs `libclang-rt-18-dev`) on every route. A truncated
  pointer can survive a normal run.

**Fix.** stadion's conventions (stadion's porting notes, kept in plethron as `docs/archive/PORTING_NOTES.md`): pointer-holding ints become
pointers or `uintptr_t`; no `& 0x1FFFFFFF`/`K0_TO_PHYS` on host pointers; ROM offsets stay `u32`; `OSMesg` through
`OS_MESG_32`/`OS_MESG_PTR`; tag port edits `// PORT:`.

**Examples.**
- GB Player quit segfault (plethron#32): an `s32` alias `D_80075E30` of the audio `lastInfo` pointer cleared half of
  a 64-bit pointer.
- Pokédex backdrop noise (plethron#30): `PokeModelPreview.backgroundImage` was `s32`, a cut host pointer.
- `osRecvMesg` into a `u32` local smashes the stack: receive into an `OSMesg`, then `(u32)(uintptr_t)`.
- **Float bits as ints**: m2c passes `f32` arguments as hex literals (`0x3F800000`) or `*(s32*)&f`. On the host
  these convert numerically (1065353216.0f instead of 1.0f). 161 of them in Zero's battle code alone.

## 3. Prototype and alias mismatches

The decomp calls functions it has not named through `func_XXXXXXXX` aliases (plethron's
`src/port/generated/zero_aliases.ld` maps `func_X = RealName`). Each caller has its own local declaration, so the
compiler never sees that two declarations disagree.

**Symptom.** Wrong argument counts (`a0`/`a1` passthroughs), pointers passed into `s32` parameters, structs passed
by value with the wrong size. On Linux these often work. On Windows they crash (class 11).

**Find.** An alias-aware cross-file prototype check (`protos.py` in the shared `plethron/tools/`, and
`tools/abi_audit/abi_calls.py`, which is a CI job in plethron).

**Fix.** Call the real name and copy the definition's signature into every declaration (`fixdecl.py`).
`tools/sync_protos.py` keeps them in sync.

**Example.** 44 wrong argument counts in Zero's battle code (plethron#3/#6).
`Trade_DrawSolidFillRect` took `Color_RGB8` (3 bytes) and 119 callers passed a 4-byte colour: main-menu window crash
on Windows only (plethron#25).

## 4. Byte order

N64 data (ROM, fragment images, cartridge and N64 saves, RDRAM views the RSP/RDP read) is big-endian. The host is
little-endian. Swapping is easy; the bugs are in data that is read at a different width than it was swapped at.

**Symptom.** Pink or rainbow backgrounds and textures (bytes of colours reordered), garbled text, wrong numbers
that look like `0x00010000` multiples, silent audio, GB Player text in the wrong colours.

**Find.**
- `raw_offsets.py`'s `NARROW` list: byte views of lifted host-order words.
- Look for C that reads a symbol at a different width than its declared type (u16 reads inside a u32 table, byte
  reads inside a word). Battle pacing was this (below).
- Bitfields: big-endian bitfield order is the reverse of little-endian. `tools/endian_union_sweep.py` (stadion).

**Fix.** stadion's way: named `Port_Swap*` / `Port_ReadBE*` helpers from `game/include/port_endian.h`, right after
each ROM copy, and nowhere else. Plethron adds `Port_WordByte`/`Port_WordsToBytes` for byte views of swapped words,
and `Port_MoveAnimU16` style accessors (offset `^ 2`) for halfwords inside word-swapped data. Keep one copy of each
helper (`tools/check_port_decls.py`).

**Examples.**
- Main menu and いちらん backgrounds pink instead of grey/gold (plethron#31): byte views of host-order words.
- `Color_RGBA8_u32` is numeric `0xRRGGBBAA`; writing one through a byte-wise `Color_RGBA8*` reverses it on a
  little-endian host. That was a cyan fog on every model.
- Badge rings rainbow (vs-order-glitches): six RGBA16 textures declared `extern u16[]`, so the lifter swapped them.
  Textures must stay bytes (`u8[]`).
- Battle pacing (plethron#52): the move table `D_80073D84` sits inside `D_80073B30`'s u32-lifted extent, so its
  halfwords had to be read at `offset ^ 2`. Raw big-endian `endFrame` reads needed `ANIM_END`. After that every
  battle state lasts as long as on ares; the mean picture score went from 29.9 to 8.2.
- GB Player: `GbApuRegs` bitfields are big-endian, so the GB was silent until the field order was reversed per byte.
  `osGbpakReadId` must keep `OSGbpakId`'s 16-bit fields in N64 byte order (the game compares them byte by byte).
  Bank sums and the GB header checksum are byte data, never u16 reads. The C-Up menu text was pink until its palette
  colours were put back in N64 order.
- Strip wipe between battle phases (plethron#73): C loads lifted `u32[]` data as a texture; only arrays registered
  with libultraship's TMEM swap (`Port_RegisterWordArray`) are swapped back. Now every lifted u16/u32 array is
  registered. Bug class: any lifted u16/u32 texture or palette loaded from C.
- Saves: keep the N64 save in N64 byte order so it is the same file emulators use (`zero_sram.cpp` detects and
  normalises Project64 word-swapped, half-swapped and old-port orders).

## 5. The data lifter (data that is not in C)

Only relevant if the game's data stays in assembly, as Zero's does. Plethron defines host storage for every data
symbol the C code references (`tools/zero/gen_lift.py` writes `src/port/generated/zero_lift_data.c`) and fills it
from the ROM at run time (`src/port/zero_lift.c`): scalars swapped, 32-bit pointers widened and relocated, function
pointers mapped. **The extern declaration's type is the layout.** Stadion's data was already in C, so it did not
have this class. If diaulos lifts data, read this whole section first.

**Symptom.** Null pointers in tables that should be full, text garbled into swapped words, a table with only its
first row, crashes on a screen that reads the second half of something.

**Find.**
- After every regen: `git diff src/port/generated/zero_aliases.ld` (new or shifted `+ N` inner aliases = layout
  mistake) and diff the lift table's count column old vs new. Look at every symbol whose count drops, especially to 0.
- `PORT_LIFT_AUDIT=1 build/plethron <rom>` lifts every image and lists redirects and byte pointers into word data.
- `PORT_TRACE_LIFT=1` logs lazily lifted unnamed blocks.
- Lift-table rows with a large count and a non-byte type are usually a small extern that grew over a blob.

**Rules we learned (each one cost a bug):**
- Declare address-holding data as pointers (`u8*`), never `uintptr_t`: the generator sizes the N64 side from the
  declared type and treats `uintptr_t` as 8 bytes.
- An open array (`T x[]`) is lifted as ONE element when another symbol follows. Size it.
  Example: `D_8218B42C` (demo cue tables) lifted as one row of two: the Cup clear parade crashed (plethron#48).
- A small extern grows over the unnamed data after it (the gap rule). `D_8218B63C` (one `Vec3f`) swallowed the
  battle message strings; `D_8200923C` (one colour) swallowed 14 rule-page tables, so ルールを見る crashed on every
  platform (plethron#47). Fix: `DECLARED_ONLY` in the generator, a byte-typed split, or the runtime foreign-tail
  check in `zero_lift.c`.
- Text declared `M2C_UNK` lifts as word-swapped bytes. Declare it `char[]`.
- Structs in data that hold pointers must be declared with their struct type, not `u8[]`, or their pointers stay
  32-bit N64 addresses.
- `extern M2C_UNK sPartyScreen` got 0x388 bytes; the host struct is 1048 and ran over the next symbol (plethron#51).
  Anything holding host pointers (message queues, pools, heaps) must be typed.
- A matrix inside a symbol the lifter typed as `Gfx` (`D_80074410` = identity `Mtx` + a display list) gave garbage
  modelview: no flames on Charizard and Moltres (plethron#71). Use `gIdentityMtx`.
- The lifter relocated a `G_TEXTURE` scale word `0x80008000` as an address: garbled habitat pins (plethron#30).
  Relocate `Gfx` w1 only for opcodes that take an address.
- Every loader must call the lift hook. `Fragment_LoadAndRelocateFromRomRange` did not, so the Pokédex Kanto map read
  all-zero storage.
- A generator change that fixes one screen can break another. Clipping extents at the first string address
  (plethron#39) emptied tournament opponent tables and crashed both tournaments, and nobody noticed until two branch
  lines merged (plethron#59). After any generator change, rerun **every** route.
- Merge conflicts in the generated table: rerun the generator, never pick a side.

Regen needs the decomp at the commit in `game/UPSTREAM.md`, built (`make init && make rom`, apt
`binutils-mips-linux-gnu`), `pip install libclang`, then `tools/zero/regen_lift.sh <decomp dir>`.

## 6. Missing return values

**Symptom.** A function returns garbage, or the right value by luck because clang emitted a tail call at `-O2`.
Frame loops that never end, fades that stop early.

**Find.** `tools/check_return_values.py` (clang `-ast-dump=json`, ~1 min, a CI job in plethron): fails when a caller
uses a value a function never returns, or a `void` function is stored as a value-returning function pointer. Mark
intentional ones `// PORT: v0`. On the N64 the caller got the last call's `v0`: read it from the decomp's
disassembly (`mips-linux-gnu-objdump -d --disassemble=FN build/.../*.elf`).

**Example.** 16 fixed in plethron#38, among them `FreeBattle_RunFrame` and `PartyScreen_RunFrame` (frame loops) and
the particle fade helpers. Report: plethron `docs/return-value-audit.md`.

## 7. Buffers sized for the N64

**Symptom.** One byte of a neighbour zeroed; shows up on one compiler or ABI only. Julius's Windows build drew
Earthquake's PP as "10/0", Linux clang drew it fine.

**Find.** ASan on the route that shows the screen (`-Wfortify-source` does not help: the game has its own
`sprintf`). Look at every `sprintf` into a stack array.

**Fix.** Size for the terminating NUL; use host structs instead of N64-sized byte buffers (`s8[0x3C]` Bezier paths).

**Examples.** `s8 sp70[7]` for `"%3d/%3d"`, `sp58[2]` for `"%d"` (plethron#66). A 6-entry host copy read with
`frame & 7` in the intro (ASan, plethron#45).

## 8. Code that got lost on the way

Making code "64-bit clean" in bulk deletes things. Plethron's first sound loader cleanup deleted 31 standalone calls
(move sounds, music, announcer ROM reads, volumes).

**Symptom.** A sound, an effect or a step simply missing, no error anywhere.

**Find.** Compare against the decomp, not git history (history gets squashed):
`tools/decomp_audit/audit.py` (token deficits per function, follows renames) and `dropped_calls.py` in the shared
`plethron/tools/` (call counts per function vs the decomp). Every hit needs a reason or `// PORT: restored`. Rerun
after each decomp sync.

**Related trap: files copied from stadion keep Stadium 1 constants.** `audio_hle.cpp` read the resampler taps at
Stadium 1's ROM offset `0x7F670+0xB0`; Zero's are at `0x808F0`. Now found by signature. Grep copied files for ROM
offsets and addresses. Also, the port half of a stadion fix can exist while the game never calls it
(`Port_ControllerBits`, `Port_NoteFrameBuffer`, widescreen hooks): plethron `stadion_bugfix_ref/` lists stadion's
fixes and whether each was needed, already ported, or not applicable. Check it (or its diaulos equivalent) first
when a bug "looks like a Stadium 1 one".

## 9. Hardware registers and timing

**Symptom.** Clicks on note onsets and music drifting against the picture (30-70 frames within a scene).

**Cause.** `audio_manager.c` read `HW_REG(AI_LEN_REG)`. On the port that is a scratch page that always reads 0, so
every audio frame was 552 samples (the N64 alternates 552/368), the virtual AI FIFO filled up and dropped a buffer
every ~32.5 frames. Fix: `osAiGetLength()` (stadion had the same fix).

**Find.** grep for `HW_REG(`, `IO_READ(` and raw `0xA4......` addresses. For audio, check the dumped frame sizes
(`port.pcm.frames`) for gaps or missing 368-sample frames.

**Other timing traps.**
- Busy-wait loops on hardware (the GB Player waiting on the Transfer Pak) deadlock under lockstep: give them a yield
  (`Port_GbIoSpin`).
- Loads are instant on the port. Zero's Pikachu cart copy screen (~1,600 frames) was skipped until Transfer Pak timing
  was emulated (36,070 cycles per 32-byte block). The send-out model load still takes ~18 frames on the N64 and 0 on
  the port, which shifts later camera timing. Preservation means original screens stay, so ask before skipping one.
- A frame the port skips drawing (fast-forward) must still run the game's one-shot draws: the main menu bar was left
  mid-slide until `Port_MustDrawThisFrame`.

## 10. Renderer (libultraship / Fast3D) differences

These are libultraship changes, which follow the workflow rule in [WORKFLOW.md](WORKFLOW.md): show old | fixed | ares
(and stadion when affected) before pushing. Follow libultraship upstream style there, not the decomp style guide.

- **Stale translation cache.** Model blobs reuse pool memory and are freed by a pool reset, so the display-list
  translation cache (keyed by host address) served the previous model's lists: spikes and missing bellies on intro
  models, on every platform (plethron#25). Suspect any cache keyed by host address when a model looks partly like
  another one. Flush on overlapping allocations.
- **Shade interpolation.** The RDP interpolates shade linearly in screen space; Fast3D did it perspective-correct.
  The floor by the walls was up to +20 brighter. The fix needed both a shader change (multiply by w, divide in the
  fragment shader; `noperspective` gave black near-clipped triangles) and RSP-style near-plane/guard-band clipping
  before shading (libultraship#15).
- **Combiner NOISE.** The RDP's 9-bit noise has a mean near 0.06 as a cycle-2 multiplier; Fast3D's is uniform 0..1.
  Sleep Powder and Haze look lighter. The accurate fix was **rejected**: it looks worse. Listed as a known difference.
  A fix that matches ares is not automatically wanted.
- **Render targets.** A target re-rendered without readback sampled stale contents (VS screen COM icons black from
  round 2, libultraship#16). Frame interpolation replayed a frame's display list after the game had loaded the next
  model into a shared buffer, scrambling order-screen icons; replays now skip draws into render targets
  (libultraship#18). Reproduce interpolation bugs at real speed (`PORT_SPEED=1 PORT_INTERPOLATE=1
  PORT_INTERP_IMMEDIATE=late`); max-speed runs hide them.
- **CPU edits to textures the GPU already has.** After the game recolours something on the CPU, call
  `Port_GfxForgetRenderTargets` or the GPU copy is sampled.
- **Widescreen.** Two full-screen texrects before 3D lock the frame to 4:3 (the sky in every non-Free-Battle battle,
  the VS zoom). Mark them with `gDPPort3DScene`/`gDPPortWideRects`. Never use `G_TEXRECT_WIDE` in game code: the
  skipped-frame state pass stops at it and loses texture loads. Screens that draw once and keep the frame buffer need
  their side margins redrawn every frame.
- **Raw lists at fragment addresses.** Display lists at `0x8Exxxxxx` fragment addresses are raw N64 lists (8-byte,
  big-endian): translate with `Port_GfxPrepareDL` before walking them. `G_DL` into raw lifted blocks must go through
  the translator each use.

## 11. Windows, MinGW and macOS only bugs

- **Win64 passes structs that are not 1, 2, 4 or 8 bytes by reference.** SysV passes small structs in registers.
  A declaration with a differently sized struct works on Linux and crashes on Windows (class 3's example).
  `tools/abi_audit/abi_calls.py` lists WIN64-ONLY mismatches (CI job `windows-abi-calls`), `layouts.py` compares
  record layouts Linux vs MinGW.
- **MinGW uses MS bitfield layout.** Build the game with `-mno-ms-bitfields`.
- **vcpkg picks the first `x86_64-w64-mingw32-gcc` on PATH.** A GCC MinGW (Strawberry Perl, MSYS2) builds spdlog/fmt
  against libstdc++ and the game fails to link. Plethron ships overlay triplets that pin llvm-mingw's clang
  (plethron#90).
- **`uint64_t` printf.** `unsigned long` on Linux, `unsigned long long` on Windows/macOS: cast or use `PRIu64`.
- **macOS.** AppleClang defaults to common symbols, and `.set` into a common symbol stays undefined: the lifter's
  aliases needed an ld64 `-alias_list` and `-fno-common` (plethron#106). Low address space tricks (`pagezero_size`
  below 4 GB) do not work on arm64 macOS, so the port must be truly 64-bit clean.
- **Testing Windows from a Linux container**: llvm-mingw tarball + vcpkg with an asset script (see
  [ARES_SETUP.md](ARES_SETUP.md#windows-builds-from-a-linux-container)), run under Wine with the OpenGL backend.

## 12. Uninitialised values the N64 got "for free"

Decompiled code sometimes reads a stack slot it never wrote. On the N64 that slot holds whatever the previous call
left there, and the game's look depends on it.

**Example.** Hyper Beam and Ice Beam ribbons: `HyperBeamIceBeamTrail_UpdateSlot`'s `spA8` is uninitialised. On the
N64 it holds the `a0` home slot left by an earlier call: the owner `ActivePokemon`'s N64 address. Emulating that value
brought Hyper Beam from a score of 7.0 to 2.2. Exact Ice Beam would need stack emulation; that is a decision for the
project owner, not a quiet fix.

**Find.** `ARES_REF_PEEK` (see ARES_SETUP.md) to read the slot's value in ares at function entry; compiler warnings
`-Wuninitialized`/`-Wsometimes-uninitialized` on the file.

## 13. Things that are not bugs

Check these before fixing anything:

- **ares does it too.** Zero plays no cry when a Pokémon faints (confirmed with sound logs on both sides). Starmie's
  purple patterned gem during Minimize, Minimize's lasting shrink, the Articuno Fly camera: all on ares. Withdraw and
  Zapdos's Drill Peck are model-only by design.
- **The code never runs.** Zero's Nas sound driver (`audio_and_gb_emulator`) has no caller: gdb breakpoints are never
  hit and the port's audio is byte-identical before and after editing it. The real sound path is libnaudio +
  libnumus + `audio_hle.cpp`. Set a breakpoint before fixing a function.
- **An N64 lag frame.** The N64 slows down in busy effects and the port does not. Sky Attack's charge sparkles and
  Psywave looked different because a per-frame random draw happened before a reseed on the N64 and after it on the
  port. One frame of shift made 25,758 calls identical.
- **A test that compares unrelated frames.** Old ares recordings predating a fix, phase counters that drifted between
  two runs, sync points that catch the wrong event (Razor Wind's turn 2 replays the charge animation).
- **Blinking on a different phase** (GB map markers every 8 frames).

## 14. Test harness artefacts

- **Resolution.** Zero draws menus and text in 640x480. At "native" 320x240 every other texel is dropped: the small
  font's 6 and 9 looked like 8 and GB Player text looked garbled. Run the port at `PORT_RESOLUTION=2x` for diffs.
- **Fast-forward skips frames.** Anything a screen draws once and keeps in the frame buffer can show stale.
- **Random numbers.** Both games time random numbers with the CPU counter. Without seeding (`@rng`) two battles
  with the same input differ. The title's attract battle uses its own generator and is not seeded.
- **Presses during transitions are dropped**, by the game, on both sides. Menus need ~25 frames between presses;
  the order screen ignores 1P's picks until the COM has made its own (~250 frames).
- **Headless audio.** `PORT_NO_AUDIO=1` hid real bugs. Use `SDL_AUDIO_DRIVER=dummy` instead.
- **Compiler.** Ubuntu's gcc Release build aborts at frame 610 under `_FORTIFY_SOURCE`; build with clang like CI.
