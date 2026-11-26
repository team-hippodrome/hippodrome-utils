"""Render docs/specs/game_specific/1/MEWTWO_AI_RULES.md from results/final.json."""
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
FINAL = json.loads((ROOT / "results/final.json").read_text())
OUT = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "MEWTWO_AI_RULES.md"
TIERS = [k for k in ("10", "30", "50", "80") if k in FINAL]
DECOMP = "https://github.com/team-hippodrome/pokestadium/blob/1b884131d027acc325e5130e017d8eac9362b204/src/fragments/battle_engine"


def pct(x):
    return f"{x * 100:.1f}%"


def p256(v):
    return f"{v}/256 ({v / 256 * 100:.0f}%)"


ROWS = [
    ("p_cure_paralysis", "R1 Heal Bell when paralyzed", p256),
    ("p_cure_poison", "R1 Heal Bell when poisoned/burned", p256),
    ("p_cure_other", "R1 Heal Bell when confused/seeded/disabled", p256),
    ("p_knockout", "R2 take a guaranteed KO", p256),
    ("heal_below", "R3 Recover HP threshold", lambda v: f"HP < {v}/256 of max ({v / 256 * 100:.0f}%)"),
    ("p_heal", "R3 Recover chance", p256),
    ("p_substitute", "R4 Substitute vs status/OHKO/Explosion threat", p256),
    ("substitute_min_hp", "R4 minimum HP", lambda v: f"HP > {v}/256 of max ({v / 256 * 100:.0f}%)"),
    ("p_paralyze", "R5 Thunder Wave", p256),
    ("p_reflect", "R6 Reflect", p256),
    ("reflect_threat", "R6 physical threat", lambda v: f"foe physical max ≥ {v}/256 of Mewtwo HP"),
    ("p_mist", "R7 Mist", p256),
    ("p_amnesia", "R8 Amnesia", p256),
    ("amnesia_max_stage", "R8 stage cap", lambda v: f"Special stage < {v - 6:+d}"),
    ("amnesia_safe", "R8 safety", lambda v: f"foe best max < {v}/256 of Mewtwo HP"),
    ("p_blunder", "T1 blunder (random legal move)", p256),
    ("pass_second", "T2 skip second action", p256),
    ("p_second_attack", "T3 second action may attack", p256),
    ("damage_roll", "Damage roll used by predictions", lambda v: f"{v}/255"),
]


def main():
    lines = []
    w = lines.append
    w("# Mewtwo AI rule tables (Round 2, HARDER_VS_MEWTWO)")
    w("")
    w("Companion to [HARDER_VS_MEWTWO.md](HARDER_VS_MEWTWO.md). It defines the decision procedure Mewtwo uses for "
      "each of its two Round 2 actions, plus four difficulty tiers tuned in a simulator. Each tier is named for "
      "the share of battles Mewtwo wins against near-perfect play: **10%** (the requested target), 30%, 50% and 80%.")
    w("")
    w("The rules never test species identity. They read only types, move effects, current HP, status, stat stages, "
      "and true stats, which the stock Stadium AI also reads.")
    w("")
    w("## How to read this")
    w("")
    w("- This is a **first-match list**. For each action, walk R1 to R9 from the top and take the first rule whose "
      "condition holds **and** whose chance roll succeeds. A failed roll falls through to the next rule.")
    w("- **The list runs once per action.** In Round 2 Mewtwo gets two actions per turn. Action 2 is decided on the "
      "battle state left by action 1, as the spec requires. Action 1 is decided at the start of the turn, without "
      "seeing the player's choice.")
    w("- **Chances are out of 256,** so they map to one `Battle_Random()` byte compared with `<`. 0 means never and "
      "256 means always.")
    w("- **The tier tables below** only change the numbers. The rule order is the same in every tier.")
    w("")
    w("## Helper predicates")
    w("")
    w("| Name | Definition | Source |")
    w("|---|---|---|")
    w(f"| `max_damage(user, move, roll)` | Gen 1 damage formula: \\(\\lfloor\\lfloor(2L/5+2)\\cdot P\\cdot A/D\\rfloor/50\\rfloor\\), capped at 997, plus 2. Attack and Defense are both divided by 4 when either is above 255. Then STAB ×1.5, the type chart applied one entry at a time, and `roll/255`. No crit. Multi-hit counts as ×3 and two-hit moves as ×2. Fixed-damage moves return their fixed value. | [`BattleAI_PredictDamage`]({DECOMP}/battle_engine_com_decision_logic.c#L313), [`sTypeEffects`]({DECOMP}/battle_engine_mechanics.c) |")
    w(f"| `can_ko(move)` | The foe has no Substitute and `max_damage(Mewtwo, move, damage_roll)` ≥ foe current HP. | KO check in [`BattleAI_BuildMoveCandidateScores`]({DECOMP}/battle_engine_com_decision_logic.c#L1497) (`score ≥ displayHp`) |")
    w(f"| `attack_score(move)` | `max_damage × accuracy / 255`. If the foe has a Substitute and the move would break it, the score is 2000 + accuracy instead. | [`BattleAI_PickBestMove` case 10]({DECOMP}/battle_engine_com_decision_logic.c#L1233) |")
    w("| `foe_best` | The highest `max_damage(foe, move, 255)` over the foe's moves that still have PP. | Same formula, run from the foe's side |")
    w("| `foe_best_physical` | `foe_best`, limited to physical-type moves (type < 0x14). | |")
    w("| `status_threat` | The foe has a move with effect Sleep (0x20), Paralyze (0x43), Poison/Toxic (0x42), Confuse (0x31), Leech Seed (0x54), Disable (0x56), OHKO (0x26) or Explode (0x07). | pokered effect IDs, which Stadium shares |")
    w("| `stat_drop_threat` | The foe has a stat-lowering effect (0x12–0x17 or 0x3A–0x3F). | |")
    w("| `foe_incapacitated` | The foe is asleep or frozen. | |")
    w(f"| `tw_legal` | The foe has no major status, no Substitute, and no Ground type (Thunder Wave is Electric). | Same skip as [`BattleAI_ApplyTypeResistance`]({DECOMP}/battle_engine_com_decision_logic.c#L2188) for effect 0x43 |")
    w("")
    w("## Priority rules")
    w("")
    w("| # | Rule | Condition | Move |")
    w("|---|---|---|---|")
    w("| T2 | Skip second action | This is action 2, and the `pass_second` roll succeeds. | (none, Mewtwo passes) |")
    w("| T1 | Blunder | The `p_blunder` roll succeeds. | A random legal move, uniformly chosen |")
    w("| T3 | Support-only second action | This is action 2, and the `p_second_attack` roll **fails**. R2 and R9 are then skipped, and if R1 and R3–R8 all miss, Mewtwo passes. | (affects R2 and R9) |")
    w("| R1 | Cure | Heal Bell has PP. Paralyzed uses `p_cure_paralysis`. Poisoned or burned uses `p_cure_poison`. Otherwise, confused, seeded or disabled uses `p_cure_other`. Sleep can't be cured, because a sleeping Mewtwo doesn't act. | Heal Bell |")
    w("| R2 | Knock out | Some attack satisfies `can_ko`. | The KO move with the highest accuracy. Ties go to list order: Psychic, Thunderbolt, Earthquake, Blizzard, Submission. |")
    w("| R3 | Heal | Recover has PP, and HP < `heal_below`/256 of max. | Recover |")
    w("| R4 | Shield | Substitute has PP, Mewtwo has no Substitute, `status_threat` holds, the foe isn't incapacitated, and HP > `substitute_min_hp`/256 of max. | Substitute |")
    w("| R5 | Paralyze | `tw_legal` holds. | Thunder Wave |")
    w("| R6 | Screen | Reflect isn't up, and `foe_best_physical` ≥ `reflect_threat`/256 × Mewtwo HP. | Reflect |")
    w("| R7 | Mist | Mist isn't up, and `stat_drop_threat` holds. | Mist |")
    w("| R8 | Boost | Special stage < `amnesia_max_stage`, and `foe_best` < `amnesia_safe`/256 × Mewtwo HP. | Amnesia |")
    w("| R9 | Attack | Always matches. | The highest `attack_score`. Ties go to list order. |")
    w("")
    w("## Tie-breakers and randomness")
    w("")
    w("- **Tie-breaks are deterministic and follow list order.** For KO moves (R2), the highest accuracy wins, then "
      "the order Psychic > Thunderbolt > Earthquake > Blizzard > Submission. R9 uses the same list order.")
    w("- **All randomness is the per-rule chance rolls plus T1 and T2.** A blunder picks uniformly from the moves "
      "that have PP and aren't Disabled.")
    w("- **Predictions use a fixed damage roll** (`damage_roll`, 236/255, the midpoint of the 217–255 range) and "
      "assume no crit. So `can_ko` sometimes misjudges a kill, which is intentional because it gives the player outs.")
    w("- **The tier knobs come from one monotone difficulty path** \\(\\ell\\in[0,1]\\). As \\(\\ell\\) rises, "
      "blunders fall from 160/256 to 0, `pass_second` falls from 256 to 0 by \\(\\ell=0.5\\), and "
      "`p_second_attack` rises from 0 to 256 after \\(\\ell=0.5\\). The chances for R2, R3, R4 and paralysis "
      "curing rise linearly. Making every knob monotone keeps a higher tier strictly harder.")
    w("")
    w("## Tier tables")
    w("")
    w("| Knob | " + " | ".join(f"{t}% tier" for t in TIERS) + " |")
    w("|---|" + "---|" * len(TIERS))
    w("| Difficulty level \\(\\ell\\) | " + " | ".join(f"{FINAL[t]['level']:.3f}" for t in TIERS) + " |")
    for key, label, fmt in ROWS:
        w(f"| {label} | " + " | ".join(fmt(FINAL[t]["rules"][key]) for t in TIERS) + " |")
    w("")
    w("## Results vs near-perfect play")
    w("")
    w("Near-perfect play is defined in two steps:")
    w("")
    w("1. **Team.** Pick the best 6-rental team, found by local search over all 150 Round 2 rentals from 7 "
      "starting teams with 800 games per candidate.")
    w("2. **Play.** Play that team with the stronger of two players:")
    w("   - **Expert heuristic.** Hand-written, and knows every mechanic.")
    w("   - **Rollout player.** Knows Mewtwo's exact rule table. It runs 64 common-random-number rollouts per "
      "candidate action, including switches, and only leaves the heuristic's choice when the paired gain is more "
      "than 2 standard errors.")
    w("")
    w("Intervals are 95% Wilson intervals.")
    w("")
    w("| Tier | Mewtwo win rate | 95% CI | Best team found | Heuristic player win | Rollout player win |")
    w("|---|---|---|---|---|---|")
    for t in TIERS:
        f = FINAL[t]
        w(f"| {t}% | **{pct(f['mewtwo_vs_perfect'])}** | {pct(f['ci'][0])}–{pct(f['ci'][1])} | "
          f"{', '.join(f['team'])} | {pct(f['heuristic_player_win'])} | {pct(f['rollout_player_win'])} |")
    if "baseline" in FINAL:
        b = FINAL["baseline"]
        w(f"| Stock-scorer baseline, two actions | {pct(b['mewtwo_vs_perfect'])} | {pct(b['ci'][0])}–{pct(b['ci'][1])} | "
          f"{', '.join(b['team'])} | | |")
    w("")
    w("### Benchmark gauntlet")
    w("")
    w("Mewtwo's win rate against each archetype team, taking the better of the two players for each cell. "
      "Heuristic cells use 4000 games and rollout cells use 300.")
    w("")
    names = list(FINAL[TIERS[0]]["gauntlet"].keys())
    w("| Team | Members | " + " | ".join(f"{t}% tier" for t in TIERS) +
      (" | Baseline (heuristic only) |" if "baseline" in FINAL else " |"))
    w("|---|---|" + "---|" * (len(TIERS) + (1 if "baseline" in FINAL else 0)))
    sys.path.insert(0, str(ROOT / "tools"))
    from teams import GAUNTLET
    for n in names:
        row = f"| {n} | {', '.join(GAUNTLET[n])} | " + " | ".join(pct(FINAL[t]["gauntlet"][n]["mewtwo_vs_best_play"]) for t in TIERS)
        if "baseline" in FINAL:
            row += f" | {pct(FINAL['baseline']['gauntlet'][n]['heuristic_mewtwo'])}"
        w(row + " |")
    w("")
    OUT.write_text("\n".join(lines) + "\n", encoding="utf8")
    print("wrote", OUT)


if __name__ == "__main__":
    main()
