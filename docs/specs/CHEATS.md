# Cheats

## Scope

Use Plethron's [cheats panel and behavior](https://github.com/team-hippodrome/plethron/pull/109) as the baseline
for [Stadion issue #203](https://github.com/team-hippodrome/stadion/issues/203): eight Stadium battle toggles per
human player and Game Boy GameShark codes per Transfer Pak. All cheats are off in a fresh config. With cheats off,
the port takes its normal paths, including the normal battle RNG calls.

- **Not part of parity:** Re-enabling Mew or Mewtwo in cups that ban them and allowing duplicate species are separate
  roster-rule requests. This spec does not change cup entry or team validation.
- **Game Boy scope:** Use codes for the exact cartridge version instead of built-in EXP, money, badge or item
  controls. Codes do not extend the set of cartridges the port already supports. In-game editing of cheats is
  also out of scope; the launcher sets them before play.

## Launcher and settings

- **Battle cheats:** One row per cheat and four checkboxes, labeled 1P through 4P. Players are identified by
  controller port, not battle side. Plethron stores each row as a four-bit `gCheats.<key>` mask, bit 0 for 1P
  through bit 3 for 4P.
- **Game Boy codes:** One text field per Transfer Pak. Plethron stores the text as `gCheats.Gb1.Codes` through
  `gCheats.Gb4.Codes`. Show rejected entries without preventing accepted codes in the same field from working.
  Warn that saving inside the Game Boy game can make cheat effects permanent.
- **Visibility:** Plethron's desktop launcher shows **Cheats** under **Save editor** in Debug mode, or whenever
  a battle toggle or valid Game Boy code is set. The active button is amber. On handhelds, **Files & help** has
  a **Cheats** entry marked **Cheats (on)** when set. Other launcher layouts can use their equivalent tool entry;
  cheats must remain visible and reachable with Debug off when any is set.
- **Persistence:** Store settings in the port config, not in either game save. **All off** clears all four players'
  toggles and all four code fields, and saves that reset.

## Stadium battles

Only real human trainers can enable cheats. A checked COM or unused controller port does not grant any effect
to that slot. Scripted battles (including the opening and title-screen demo) ignore every toggle, even if they
use human flags internally. For an active Pokémon in a tag battle, use the trainer who brought it, not the active
battle side or the trainer currently entering commands. The exception is **Opponent minimum stats**, which is
granted by any eligible trainer on the opposing side.

| Toggle | Plethron key | Effect |
| --- | --- | --- |
| Infinite HP | `InfiniteHp` | Skip damage to the active Pokémon at the normal HP subtraction hook. The hit still lands and Substitute takes damage normally. Top up a living active Pokémon to its current max HP during battle updates. Do not revive it or prevent the user of Self-Destruct or Explosion from fainting. |
| Infinite PP | `InfinitePp` | Skip the move's normal PP expenditure in both the active state and the battle roster copy. This does not refill already spent PP. |
| Status always good | `NoStatus` | Clear major status (sleep, poison, burn, freeze, paralysis), confusion and the bad-poison counter during battle updates. Undo burn's Attack cut and paralysis's Speed cut. Keep the battle roster's status in sync so the cure holds on switch-out. |
| Your moves never miss | `NeverMiss` | Skip the normal accuracy roll only. Keep the game's other hit and effect checks, such as Fly/Dig, Substitute, Mist and Dream Eater against an awake target. |
| Opponent never hits | `FoeNeverHits` | Force a miss at the hit-test hook when an opposing active Pokémon targets a protected Pokémon. This wins over the attacker's **Your moves never miss**, and does not block self-targeted moves. |
| Always your turn first | `MoveFirst` | If exactly one active Pokémon qualifies, put its move first even against normal move priority or higher Speed. If both qualify, keep normal order. Do not give extra actions or override the game's switch and no-move handling. |
| Perfect stats | `PerfectStats` | At battle runtime initialization, recalculate non-HP stats with DV 15 and Stat Exp 65535, using the Pokémon's species and level. |
| Opponent minimum stats | `FoeMinStats` | At battle runtime initialization, recalculate the opposing Pokémon's non-HP stats with DV 0 and Stat Exp 0 if any eligible trainer on the other side has this set. The target can be human or COM. If both stat cheats apply to the same Pokémon, keep its normal stats. |

The stat overrides affect Attack, Defense, Speed and Special in Gen 1, and both Special stats in Gen 2. Use the
port's normal stat formula and cap (`hu_gb_stat` in `include/hippodrome/stats.h` for standard Gen 1/2 non-HP stats).
Set the raw and effective runtime stats, then apply the game's normal status modifiers. Do not change HP, the
underlying DVs or Stat Exp, or the registered Pokémon. Hooks that modify damage or hit tests must recognize the
live battlers, not the COM's scratch copies.

Battle effects belong to the active battle state and its roster copy, not the selected or registered team used
for saving or continuing a tournament. Do not write cheat effects to the N64 save or to the attached cartridge
from Stadium battles. Keep cheat-forced turn order and stat overrides out of observations meant to describe
unassisted Speed (see [Speed Stat Estimate](SPEED_STAT_ESTIMATE.md) and [Trainer Memory](TRAINER_MEMORY.md)).

## Game Boy GameShark codes

- **Format:** `01VVLLHH` writes byte `VV` to address `HHLL` (low address byte first in the code). Accept
  case-insensitive hex; split on whitespace, commas or semicolons and ignore hyphens inside a token. Accept only
  type `01` and addresses `C000` through `DFFF`, inclusive. Reject anything else, including echo RAM and cartridge
  save RAM. Use at most the first 32 valid codes per Pak and report further valid codes as unused.
- **Timing and target:** Once per emulated Game Boy VBlank, apply the running cartridge's Pak list in input
  order (the last write to the same address wins). Do not run the codes during Stadium's ordinary Transfer Pak
  access or against another Pak. Use the Game Boy core's WRAM address mapping; in a core with banked WRAM, do not
  treat a Game Boy address as a flat host array index.
- **Save boundary:** The hook writes only to WRAM, never directly to cartridge save RAM. If the player saves
  inside the Game Boy game, it can save modified money, items, badges or other affected data as usual.
  **All off** stops future writes; it does not undo a prior in-game save.

## Porting and verification

The Plethron reference (`946abf0`) is `src/port/cheats.{h,cpp}` for settings and parsing, `src/port/launcher.cpp` for the panel,
`src/port/enhancements/cheats_battle.c` and its game-side callers for the battle hooks, and `src/port/gb_core.c`
for VBlank. Stadion can use its [Game Boy Tower core's frame-end and mapped WRAM write paths](https://github.com/team-hippodrome/stadion/blob/main/src/port/gb_core.c).
The launcher, battle hooks and cartridge state stay in the ports; this spec does not add a shared cheat API.

- **Off and reset:** Compare a seeded no-cheat battle with the baseline. Enable cheats, restart with Debug off
  and check that the launcher marks them active. Use **All off**, restart and verify that nothing remains set.
- **Battle:** Exercise all eight toggles. Check slot isolation in human vs COM and multiplayer (including a tag
  partner's switch-in where supported), and verify that scripted battles are unaffected. Cover forced miss vs
  never miss, two always-first players, conflicting stat cheats, and self-KO with Infinite HP. Verify the
  original roster on tournament resume and that a forced order does not narrow or persist an unassisted
  Speed estimate.
- **Game Boy:** Accept `01AA00C0` (`C000`), `01FF00D3` (`D300`) and `01FFFFDF` (`DFFF`). Reject `010000A0`
  (save RAM), `010000E0` (echo RAM) and `99123456` (unsupported type). Test more than 32 codes, and check that
  writes hit only the running Pak's emulated WRAM at VBlank, including its mapped bank where applicable.
