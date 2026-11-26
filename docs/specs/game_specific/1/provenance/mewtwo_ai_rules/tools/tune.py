"""Tune Mewtwo rule tables to target win rates against near-perfect play.

Near-perfect play = (1) the best 6-rental team found by local search with the
heuristic expert, then (2) the rollout (policy-improvement) player on that team
and on every gauntlet team. Mewtwo's win rate vs perfect play is
1 - max over teams of the player's win rate.

Usage: python tools/tune.py [--quick]
"""
import json
import random
import sys
import time
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from runner import evaluate, wilson, RULE_FIELDS  # noqa: E402
from teams import GAUNTLET, RENTALS, team_indices  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
EXCLUDED = {"Ditto", "Porygon"}  # Transform / Conversion not modelled
POOL = [i for i, r in enumerate(RENTALS) if r["name"] not in EXCLUDED]

MAX_TABLE = dict(baseline=0, damage_roll=236, p_cure_paralysis=256, p_cure_sleep_move=0, p_cure_poison=256,
                 p_cure_other=256, p_knockout=256, heal_below=128, p_heal=256, p_substitute=256,
                 substitute_min_hp=96, p_paralyze=256, p_reflect=256, reflect_threat=64, p_mist=256,
                 p_amnesia=256, amnesia_max_stage=10, amnesia_safe=64, p_blunder=0, p_second_blunder=0,
                 pass_second=0, p_second_attack=256)


def clamp(x, lo=0.0, hi=1.0):
    return max(lo, min(hi, x))


def table_at(level):
    """Difficulty path, level 0 (soft) .. 1 (hardest). Every knob is monotone in level."""
    t = dict(MAX_TABLE)
    t["p_blunder"] = 160 * (1 - level)
    t["pass_second"] = 256 * clamp(1 - 2 * level)
    t["p_second_attack"] = 256 * clamp(2 * level - 1)
    t["p_knockout"] = 128 + 128 * level
    t["p_heal"] = 128 + 128 * level
    t["p_cure_paralysis"] = 64 + 192 * level
    t["p_substitute"] = 64 + 192 * level
    return {k: int(round(v)) for k, v in t.items()}


def team_search(rules, games=300, restarts=None, seed=3, log=None):
    """Coordinate ascent (C 'search' mode) from each gauntlet team, two starts in parallel."""
    import subprocess
    from concurrent.futures import ThreadPoolExecutor
    from runner import SIM, WORKERS, rules_arg
    starts = restarts or [team_indices(n) for n in GAUNTLET.values()]

    def run(start):
        out = subprocess.run([SIM, "search", "--rules", rules_arg(rules), "--team", ",".join(map(str, start)),
                              "--games", str(games), "--noise", "0", "--seed", str(seed)],
                             capture_output=True, text=True, check=True).stdout.split()
        return [int(x) for x in out[0].split(",")], int(out[1])

    with ThreadPoolExecutor(WORKERS) as pool:
        found = list(pool.map(run, starts))
    best_team, best_wins = max(found, key=lambda f: f[1])
    if log:
        for start, (team, wins) in zip(starts, found):
            log(f"    from {RENTALS[start[0]]['name']}..: {[RENTALS[i]['name'] for i in team]} {wins}/{games}")
    # confirm on fresh seeds to reduce selection bias
    scored = []
    for team, _ in found:
        scored.append((evaluate(rules, team, 2000, seed=seed + 50)[0], team))
    return max(scored)[1]


def mewtwo_rate_vs_perfect(rules, team, rollout_games, rollouts, seed=17):
    wins, games = evaluate(rules, team, rollout_games, policy=1, rollouts=rollouts, seed=seed)
    return 1 - wins / games, wins, games


def main():
    quick = "--quick" in sys.argv
    log_lines = []

    def log(msg):
        print(msg, flush=True)
        log_lines.append(msg)

    targets = [0.10, 0.30, 0.50, 0.80]
    search_games = 150 if quick else 300
    rollout_games = 300 if quick else 1000
    rollouts = 32 if quick else 64
    results = {}
    cache = {}

    def measure(level):
        level = round(level, 4)
        if level in cache:
            return cache[level]
        rules = table_at(level)
        t0 = time.time()
        team = team_search(rules, games=search_games, log=log)
        heuristic = evaluate(rules, team, 4000, seed=99)[0] / 4000
        rate, wins, games = mewtwo_rate_vs_perfect(rules, team, rollout_games, rollouts)
        log(f"  level {level:.4f}: best team {[RENTALS[i]['name'] for i in team]} "
            f"heuristic player win {heuristic:.3f}, rollout player win {wins}/{games} "
            f"-> Mewtwo {rate:.3f} ({time.time() - t0:.0f}s)")
        cache[level] = (rate, team, wins, games, rules)
        return cache[level]

    for target in targets:
        lo, hi = 0.0, 1.0
        best = None
        for _ in range(6 if not quick else 4):
            mid = (lo + hi) / 2
            rate, team, wins, games, rules = measure(mid)
            if best is None or abs(rate - target) < abs(best[1][0] - target):
                best = (mid, (rate, team, wins, games, rules))
            if rate > target:
                hi = mid
            else:
                lo = mid
        level, (rate, team, wins, games, rules) = best
        lo_ci, hi_ci = wilson(games - wins, games)
        log(f"TARGET {target:.2f}: level {level:.4f} Mewtwo {rate:.3f} [{lo_ci:.3f}, {hi_ci:.3f}]")
        results[f"{int(target * 100)}"] = dict(level=level, rules=rules, team=[RENTALS[i]["name"] for i in team],
                                               team_indices=team, mewtwo_rate=rate, ci=[lo_ci, hi_ci],
                                               rollout_games=games)
    out = ROOT / ("results/tuned_quick.json" if quick else "results/tuned.json")
    out.parent.mkdir(exist_ok=True)
    out.write_text(json.dumps(results, indent=1))
    (out.with_suffix(".log")).write_text("\n".join(log_lines))


if __name__ == "__main__":
    main()
