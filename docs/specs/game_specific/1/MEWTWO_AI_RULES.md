# Mewtwo AI rule tables (Round 2, HARDER_VS_MEWTWO)

Companion to [HARDER_VS_MEWTWO.md](HARDER_VS_MEWTWO.md). It defines the decision procedure Mewtwo uses for each of its two Round 2 actions, plus four difficulty tiers tuned in a simulator. Each tier is named for the share of battles Mewtwo wins against near-perfect play: **10%** (the requested target), 30%, 50% and 80%.

The simulator, tuning scripts and raw results are in [provenance/mewtwo_ai_rules](provenance/mewtwo_ai_rules/README.md).

The rules never test species identity. They read only types, move effects, current HP, status, stat stages, and true stats, which the stock Stadium AI also reads.

## How to read this

- This is a **first-match list**. For each action, walk R1 to R9 from the top and take the first rule whose condition holds **and** whose chance roll succeeds. A failed roll falls through to the next rule.
- **The list runs once per action.** In Round 2 Mewtwo gets two actions per turn. Action 2 is decided on the battle state left by action 1, as the spec requires. Action 1 is decided at the start of the turn, without seeing the player's choice.
- **Chances are out of 256,** so they map to one `Battle_Random()` byte compared with `<`. 0 means never and 256 means always.
- **The tier tables below** only change the numbers. The rule order is the same in every tier.

## Helper predicates

| Name | Definition | Source |
|---|---|---|
| `max_damage(user, move, roll)` | Gen 1 damage formula: \(\lfloor\lfloor(2L/5+2)\cdot P\cdot A/D\rfloor/50\rfloor\), capped at 997, plus 2. Attack and Defense are both divided by 4 when either is above 255. Then STAB ×1.5, the type chart applied one entry at a time, and `roll/255`. No crit. Multi-hit counts as ×3 and two-hit moves as ×2. Fixed-damage moves return their fixed value. | [`BattleAI_PredictDamage`](https://github.com/team-hippodrome/pokestadium/blob/1b884131d027acc325e5130e017d8eac9362b204/src/fragments/battle_engine/battle_engine_com_decision_logic.c#L313), [`sTypeEffects`](https://github.com/team-hippodrome/pokestadium/blob/1b884131d027acc325e5130e017d8eac9362b204/src/fragments/battle_engine/battle_engine_mechanics.c) |
| `can_ko(move)` | The foe has no Substitute and `max_damage(Mewtwo, move, damage_roll)` ≥ foe current HP. | KO check in [`BattleAI_BuildMoveCandidateScores`](https://github.com/team-hippodrome/pokestadium/blob/1b884131d027acc325e5130e017d8eac9362b204/src/fragments/battle_engine/battle_engine_com_decision_logic.c#L1497) (`score ≥ displayHp`) |
| `attack_score(move)` | `max_damage × accuracy / 255`. If the foe has a Substitute and the move would break it, the score is 2000 + accuracy instead. | [`BattleAI_PickBestMove` case 10](https://github.com/team-hippodrome/pokestadium/blob/1b884131d027acc325e5130e017d8eac9362b204/src/fragments/battle_engine/battle_engine_com_decision_logic.c#L1233) |
| `foe_best` | The highest `max_damage(foe, move, 255)` over the foe's moves that still have PP. | Same formula, run from the foe's side |
| `foe_best_physical` | `foe_best`, limited to physical-type moves (type < 0x14). | |
| `status_threat` | The foe has a move with effect Sleep (0x20), Paralyze (0x43), Poison/Toxic (0x42), Confuse (0x31), Leech Seed (0x54), Disable (0x56), OHKO (0x26) or Explode (0x07). | pokered effect IDs, which Stadium shares |
| `stat_drop_threat` | The foe has a stat-lowering effect (0x12–0x17 or 0x3A–0x3F). | |
| `foe_incapacitated` | The foe is asleep or frozen. | |
| `tw_legal` | The foe has no major status, no Substitute, and no Ground type (Thunder Wave is Electric). | Same skip as [`BattleAI_ApplyTypeResistance`](https://github.com/team-hippodrome/pokestadium/blob/1b884131d027acc325e5130e017d8eac9362b204/src/fragments/battle_engine/battle_engine_com_decision_logic.c#L2188) for effect 0x43 |

## Priority rules

| # | Rule | Condition | Move |
|---|---|---|---|
| T2 | Skip second action | This is action 2, and the `pass_second` roll succeeds. | (none, Mewtwo passes) |
| T1 | Blunder | The `p_blunder` roll succeeds. | A random legal move, uniformly chosen |
| T3 | Support-only second action | This is action 2, and the `p_second_attack` roll **fails**. R2 and R9 are then skipped, and if R1 and R3–R8 all miss, Mewtwo passes. | (affects R2 and R9) |
| R1 | Cure | Heal Bell has PP. Paralyzed uses `p_cure_paralysis`. Poisoned or burned uses `p_cure_poison`. Otherwise, confused, seeded or disabled uses `p_cure_other`. Sleep can't be cured, because a sleeping Mewtwo doesn't act. | Heal Bell |
| R2 | Knock out | Some attack satisfies `can_ko`. | The KO move with the highest accuracy. Ties go to list order: Psychic, Thunderbolt, Earthquake, Blizzard, Submission. |
| R3 | Heal | Recover has PP, and HP < `heal_below`/256 of max. | Recover |
| R4 | Shield | Substitute has PP, Mewtwo has no Substitute, `status_threat` holds, the foe isn't incapacitated, and HP > `substitute_min_hp`/256 of max. | Substitute |
| R5 | Paralyze | `tw_legal` holds. | Thunder Wave |
| R6 | Screen | Reflect isn't up, and `foe_best_physical` ≥ `reflect_threat`/256 × Mewtwo HP. | Reflect |
| R7 | Mist | Mist isn't up, and `stat_drop_threat` holds. | Mist |
| R8 | Boost | Special stage < `amnesia_max_stage`, and `foe_best` < `amnesia_safe`/256 × Mewtwo HP. | Amnesia |
| R9 | Attack | Always matches. | The highest `attack_score`. Ties go to list order. |

## Tie-breakers and randomness

- **Tie-breaks are deterministic and follow list order.** For KO moves (R2), the highest accuracy wins, then the order Psychic > Thunderbolt > Earthquake > Blizzard > Submission. R9 uses the same list order.
- **All randomness is the per-rule chance rolls plus T1 and T2.** A blunder picks uniformly from the moves that have PP and aren't Disabled.
- **Predictions use a fixed damage roll** (`damage_roll`, 236/255, the midpoint of the 217–255 range) and assume no crit. So `can_ko` sometimes misjudges a kill, which is intentional because it gives the player outs.
- **The tier knobs come from one monotone difficulty path** \(\ell\in[0,1]\). As \(\ell\) rises, blunders fall from 160/256 to 0, `pass_second` falls from 256 to 0 by \(\ell=0.5\), and `p_second_attack` rises from 0 to 256 after \(\ell=0.5\). The chances for R2, R3, R4 and paralysis curing rise linearly. Making every knob monotone keeps a higher tier strictly harder.

## Tier tables

| Knob | 10% tier | 30% tier | 50% tier | 80% tier |
|---|---|---|---|---|
| Difficulty level \(\ell\) | 0.062 | 0.168 | 0.270 | 0.416 |
| R1 Heal Bell when paralyzed | 76/256 (30%) | 96/256 (38%) | 116/256 (45%) | 144/256 (56%) |
| R1 Heal Bell when poisoned/burned | 256/256 (100%) | 256/256 (100%) | 256/256 (100%) | 256/256 (100%) |
| R1 Heal Bell when confused/seeded/disabled | 256/256 (100%) | 256/256 (100%) | 256/256 (100%) | 256/256 (100%) |
| R2 take a guaranteed KO | 136/256 (53%) | 150/256 (59%) | 163/256 (64%) | 181/256 (71%) |
| R3 Recover HP threshold | HP < 128/256 of max (50%) | HP < 128/256 of max (50%) | HP < 128/256 of max (50%) | HP < 128/256 of max (50%) |
| R3 Recover chance | 136/256 (53%) | 150/256 (59%) | 163/256 (64%) | 181/256 (71%) |
| R4 Substitute vs status/OHKO/Explosion threat | 76/256 (30%) | 96/256 (38%) | 116/256 (45%) | 144/256 (56%) |
| R4 minimum HP | HP > 96/256 of max (38%) | HP > 96/256 of max (38%) | HP > 96/256 of max (38%) | HP > 96/256 of max (38%) |
| R5 Thunder Wave | 256/256 (100%) | 256/256 (100%) | 256/256 (100%) | 256/256 (100%) |
| R6 Reflect | 256/256 (100%) | 256/256 (100%) | 256/256 (100%) | 256/256 (100%) |
| R6 physical threat | foe physical max ≥ 64/256 of Mewtwo HP | foe physical max ≥ 64/256 of Mewtwo HP | foe physical max ≥ 64/256 of Mewtwo HP | foe physical max ≥ 64/256 of Mewtwo HP |
| R7 Mist | 256/256 (100%) | 256/256 (100%) | 256/256 (100%) | 256/256 (100%) |
| R8 Amnesia | 256/256 (100%) | 256/256 (100%) | 256/256 (100%) | 256/256 (100%) |
| R8 stage cap | Special stage < +4 | Special stage < +4 | Special stage < +4 | Special stage < +4 |
| R8 safety | foe best max < 64/256 of Mewtwo HP | foe best max < 64/256 of Mewtwo HP | foe best max < 64/256 of Mewtwo HP | foe best max < 64/256 of Mewtwo HP |
| T1 blunder (random legal move) | 150/256 (59%) | 133/256 (52%) | 117/256 (46%) | 93/256 (36%) |
| T2 skip second action | 224/256 (88%) | 170/256 (66%) | 118/256 (46%) | 43/256 (17%) |
| T3 second action may attack | 0/256 (0%) | 0/256 (0%) | 0/256 (0%) | 0/256 (0%) |
| Damage roll used by predictions | 236/255 | 236/255 | 236/255 | 236/255 |

## Results vs near-perfect play

Near-perfect play is defined in two steps:

1. **Team.** Pick the best 6-rental team, found by local search over all 150 Round 2 rentals from 7 starting teams with 800 games per candidate.
2. **Play.** Play that team with the stronger of two players:
   - **Expert heuristic.** Hand-written, and knows every mechanic.
   - **Rollout player.** Knows Mewtwo's exact rule table. It runs 64 common-random-number rollouts per candidate action, including switches, and only leaves the heuristic's choice when the paired gain is more than 2 standard errors.

Intervals are 95% Wilson intervals.

| Tier | Mewtwo win rate | 95% CI | Best team found | Heuristic player win | Rollout player win |
|---|---|---|---|---|---|
| 10% | **9.1%** | 7.7%–10.6% | Magneton, Pinsir, Clefable, Chansey, Rhydon, Drowzee | 90.0% | 90.9% |
| 30% | **27.6%** | 26.5%–28.8% | Moltres, Magneton, Pinsir, Drowzee, Farfetch'd, Chansey | 72.4% | 71.5% |
| 50% | **53.4%** | 52.1%–54.7% | Zapdos, Pinsir, Rhydon, Farfetch'd, Magneton, Moltres | 46.6% | 45.8% |
| 80% | **77.9%** | 75.8%–80.0% | Zapdos, Moltres, Vaporeon, Pinsir, Farfetch'd, Persian | 20.0% | 22.1% |
| Stock-scorer baseline, two actions | 81.5% | 78.7%–84.0% | Clefable, Rattata, Clefairy, Pinsir, Magneton, Farfetch'd | | |

### Benchmark gauntlet

Mewtwo's win rate against each archetype team, taking the better of the two players for each cell. Heuristic cells use 4000 games and rollout cells use 300.

| Team | Members | 10% tier | 30% tier | 50% tier | 80% tier | Baseline (heuristic only) |
|---|---|---|---|---|---|---|
| A_vanilla_cheese | Electrode, Magneton, Rhydon, Nidoking, Dewgong, Kingler | 20.3% | 44.8% | 70.4% | 88.7% | 89.0% |
| B_boom_offense | Jynx, Onix, Gengar, Exeggcute, Raichu, Snorlax | 44.0% | 69.0% | 89.7% | 98.0% | 99.6% |
| C_standard_strong | Mew, Jolteon, Exeggutor, Chansey, Tauros, Zapdos | 45.7% | 70.7% | 85.7% | 96.0% | 99.9% |
| C2_standard_lapras | Mew, Jolteon, Exeggutor, Chansey, Tauros, Lapras | 52.3% | 76.3% | 90.0% | 97.3% | 100.0% |
| D_debuff_stall | Venusaur, Tentacruel, Dugtrio, Electrode, Seadra, Alakazam | 51.3% | 76.7% | 92.0% | 98.7% | 99.7% |
| E_wrap_lock | Dragonite, Victreebel, Tentacruel, Cloyster, Charizard, Raichu | 51.0% | 79.0% | 90.0% | 99.0% | 100.0% |
| F_sleep_para | Jynx, Gengar, Parasect, Exeggutor, Raichu, Electabuzz | 40.7% | 73.3% | 91.7% | 99.5% | 99.7% |


## Findings

- **The stock scorer is already about an 80% tier once it gets two actions.** With the decomp's own move scorer, two actions and no blunders, Mewtwo beats the best team found 81.5% of the time. Most of the difficulty comes from the double action, not from smarter move choice. That is why the lower tiers lean on T1 (blunders) and T2 (skipping action 2) rather than on weakening the rules.
- **Cheese team A is not "crushed 95%+" at any tier.** Mewtwo beats it 20% / 45% / 70% / 89% across the four tiers. The near-perfect best teams themselves use cheese pieces (Magneton Thunder Wave, Toxic users such as Pinsir and Farfetch'd, Seismic Toss users such as Chansey and Drowzee). Pushing team A to 95% would also push the perfect-play rate above 80%, which conflicts with the tier targets. Only the full-strength table (no blunders, no skips; not one of the four tiers) beats team A at about 100%.
- **The 30% tier lands slightly low** (27.6%, and its CI just misses 30%). The search accepted the closest level it found within its tolerance. If you want it centred, \(\ell\approx0.175\) should give about 30% according to the difficulty curve. That is interpolated, not run.
- **Every tier is monotone.** Each knob moves one way along \(\ell\), so a higher tier is never easier against any team.

## Caveats

- **"Near-perfect" is an approximation.** The rollout player beats the heuristic player by only about 1 point. That means either the heuristic is already close to optimal, or neither player finds deeper lines. A true expectimax search over switches could push all four rates lower.
- **The heuristic player never switches voluntarily.** Only the rollout player considers switches.
- **Edge cases are simplified.** Struggle, Metronome and Transform are handled in simplified form. Freeze Clause is assumed off. Mewtwo's AI reads true stats and the foe's full moveset, as the user decided.
- **Team B was adapted.** Among Round 2 rentals, only Onix and Exeggcute learn Selfdestruct, and rental Electrode has no Explosion (per the parsed rental data). So team B is "boom-flavoured" rather than five exploders.

## Sources

- Decomp: [team-hippodrome/pokestadium @ 1b88413](https://github.com/team-hippodrome/pokestadium/tree/1b884131d027acc325e5130e017d8eac9362b204). Damage prediction, scorer, type chart, sleep roll (`Random & 3`), and the Blizzard freeze chance.
- Effect IDs and base data: [pret/pokered](https://github.com/pret/pokered).
- Mewtwo's moveset and stats: [Stadium Tactician cheat sheet](https://stadiumtactician.com/cheat-sheet/mewtwo/2), [Altissimo's Mewtwo page](https://altissimo1.github.io/Supplementary-Series/Stadium/Stadium-1/mewtwo.html), [Serebii](https://www.serebii.net/stadium/mewtwo.shtml).
- Rentals and rules: [Altissimo's Prime Cup page](https://altissimo1.github.io/Supplementary-Series/Stadium/Stadium-1/prime-cup.html), [Bulbapedia](https://bulbapedia.bulbagarden.net/wiki/Pok%C3%A9mon_Stadium_(English)).
- Self-KO Clause and the boom archetype: [GardenFlouer Stadium Cup lab](https://gardenflouer.com/lab/stadium-cup).
- The debuff archetype: [StrategyWiki](https://strategywiki.org/wiki/Pok%C3%A9mon_Stadium/Mewtwo).
