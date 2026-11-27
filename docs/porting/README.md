# Porting notes for libultraship ports

What we learned getting plethron (Stadium Zero) and stadion (Stadium 1) from "it compiles" to "it looks and sounds
like the N64", written for whoever starts diaulos (Stadium 2 / 金銀). Diaulos is a much bigger game, but the bug
classes will be the same ones: the decompiled C was written for a 32-bit big-endian MIPS, and every place it leans on
that compiles cleanly on a 64-bit little-endian host and does something else.

| Document | What it covers |
|---|---|
| [PITFALLS.md](PITFALLS.md) | The bug classes, each with the symptom, how to find it, how it was fixed and a real example |
| [ARES_SETUP.md](ARES_SETUP.md) | Building and driving ares headless as the reference, running the port headless, and diffing pictures, sound and battle timing between the two |
| [WORKFLOW.md](WORKFLOW.md) | Branching, merging and review rules that cost us time when we ignored them |

The short version:

1. **Never trust a clean build.** The game builds with `-Wno-int-conversion -Wno-incompatible-pointer-types`, so
   most of the bugs below are silent. Turn the warnings on per file, and run the audits listed in PITFALLS.md.
2. **Always run the game.** "It compiles" is not done. Build SDL3, run headless under Xvfb, take screenshots and
   compare them with ares. Every port thread is expected to do this.
3. **ares is the ground truth.** If ares does it too, it is not a port bug (Zero's missing faint cry, Starmie's
   purple gem in Minimize, the Articuno Fly camera). Check before fixing.
4. **The port is a preservation port.** Fixes restore N64 behaviour; enhancements are options that are off by
   default.
5. **Never commit ROMs or anything taken from them** (pictures, audio recordings, RAM dumps, extracted tables).

Where the referenced tools live: plethron (`team-hippodrome/plethron`) `tools/` unless another repository is named.
Some one-off scripts never made it into a repository; those are in the project's shared files under `plethron/`.
