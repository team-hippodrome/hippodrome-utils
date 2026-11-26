# Mewtwo AI rules: provenance

This folder holds the code and raw results behind [MEWTWO_AI_RULES.md](../../MEWTWO_AI_RULES.md). It's kept here so every number in that spec can be traced back and re-run. Nothing here ships in a port, and nothing here is ROM data. `src/gen_data.h` and `data/rentals.json` are generated from pret/pokered, the pokestadium decomp source and Altissimo's public rental table. The upstream inputs (`data/pokered/`, `data/*.html`) and the built `sim` binary are git-ignored.

| Result file | Contents |
|---|---|
| `results/final.json` | Each tier's knobs, best team, win rates, 95% intervals and gauntlet results, plus the stock-scorer baseline. |
| `results/curve.json` | Mewtwo's win rate against perfect play across the difficulty path ℓ = 0…0.5. |

The spec itself is rendered from `results/final.json` by `tools/write_spec.py`. The Findings, Caveats and Sources sections were then added by hand.

## mewtwo_sim

A small C11 simulator for the Stadium 1 Round 2 Vs. Mewtwo fight with the
`HARDER_VS_MEWTWO` changes: 12 moves, two actions per turn, can't be frozen,
and Heal Bell. It runs tens of thousands of battles per second per core, so the
rule tables can be tuned with large parallel sweeps instead of headless runs of
the port.

## Layout

| Path | What it is |
|---|---|
| `tools/gen_data.py` | Builds `src/gen_data.h`: moves, base stats and effects from pret/pokered, the Stadium type chart (`sTypeEffects`) from the pokestadium decomp, and the 150 Round 2 rentals from altissimo's Prime Cup table. |
| `src/engine.c` | Battle engine: Gen 1 rules plus the Stadium changes below. |
| `src/ai_mewtwo.c` | The Mewtwo rule table (R1–R9), the helper predicates, and a rough copy of the stock scorer used as a baseline. |
| `src/player.c` | Player policies: the expert heuristic, plus a rollout player that knows the rule table (used as the "near-perfect" player). |
| `src/main.c` | `sim eval`, `sim search` (team local search) and `sim screen`. |
| `tools/runner.py` | Splits games across worker processes (`SIM_WORKERS`, which defaults to the core count) and computes Wilson intervals. |
| `tools/teams.py` | The benchmark gauntlet of archetype rental teams. |
| `tools/tune.py`, `tools/curve.py`, `tools/final.py` | The difficulty path, the curve sweep, and the final validation and gauntlet runs. |

## Build and run (PowerShell)

```powershell
# Optional: regenerate src/gen_data.h (it is already committed)
$env:POKESTADIUM_DIR = "C:\path\to\pokestadium"   # decomp checkout
python tools/gen_data.py          # also needs data/pokered (pret clone) and data/prime-cup.html
make                               # or: gcc -O2 -std=c11 -o sim src/engine.c src/ai_mewtwo.c src/player.c src/main.c -lm
.\sim eval --table 1 --team 100,81,111,33,86,98 --games 1 --log
python tools/curve.py
python tools/final.py
```

## Mechanics modelled

The Stadium specifics come from the pokestadium decomp (`battle_engine_mechanics.c`, `battle_engine_move_mechanics.c`):

- **Stat stages:** the stage table is 25…400%.
- **Accuracy stages:** the table runs from 1/3 to 3.
- **1/256 miss:** re-rolled, so 255 accuracy always hits.
- **Crit rate:** \((\text{base Spe} + 76)/1024\). High-crit moves use ×4, and Focus Energy uses ×4 + 160, capped at 255. Crits use unmodified stats and ignore screens.
- **Sleep:** lasts 1–3 actions (`Random & 3`, non-zero, in Stadium mode). Waking costs the action.
- **Secondary status chance:** 10% (26/256) for side-effect-1 moves and for Blizzard, 30% (77/256) otherwise. A target can't get a secondary status of its own type. Poison side effects use 52 or 103 out of 256. Stat-drop side effects use 85/256.
- **Substitute** blocks status moves, drain moves, secondary effects and stat drops.
- **Recoil** doesn't happen after a KO or after breaking a Substitute.
- **Recover and Rest** only fail at full HP.
- **Burn and paralysis drops** are recomputed from the status rather than stacked.
- **OHKO moves** fail when the user is slower.
- **Trapping** ends on switch.
- **Freeze** has no natural thaw, but Fire moves thaw the target.
- **Self-KO clause:** if the player's last Pokémon faints on the same turn as Mewtwo, the player loses.

Round 2 Mewtwo is Lv100 with DV 15 in every stat and max stat exp, giving 415/318/278/358/406. Attacking moves have PP Ups.

## Assumptions (flags for the port)

1. **Turn order** is decided once per turn on the speeds at the start of the turn, so Mewtwo's two actions both go before or both go after the player's action. This matches the "never Mewtwo → Player → Mewtwo" rule.
2. **Can-act checks run per Mewtwo action.** Sleep, freeze, trap, flinch, recharge, confusion and full paralysis are each checked on every action. This means sleep counters tick twice per turn for Mewtwo, which is a spec ambiguity.
3. **Residual damage** (poison, burn, Toxic, Leech Seed) applies once per side per turn, after that side's actions.
4. **First action timing:** Mewtwo's first action is decided at the start of the turn, without seeing the player's choice. The second action is decided on the state left by the first.
5. **Heal Bell** cures major status, Toxic, confusion, Leech Seed and Disable.
6. **Earthquake** is Ground with 100 power. **Submission** is Fighting with 80 power, 80% accuracy and ¼ recoil, and it can be Countered.
7. **Rental PP** is base PP with no PP Ups.
8. **Moves that do nothing:** Transform, Conversion, Mimic, Whirlwind/Roar/Teleport and Splash do nothing. Ditto and Porygon are excluded from team search.
9. **Leech Seed and Confuse Ray** are blocked by Substitute. Stadium's exact behaviour here hasn't been checked against the decomp.
10. **Stall-out:** a battle that reaches 300 turns counts as a Mewtwo win.
