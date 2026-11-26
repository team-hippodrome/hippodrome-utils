"""Stage 2: pick levels for each target from results/curve.json, refine with a
local bisection, then validate and run the benchmark gauntlet.
Writes results/final.json."""
import json
import sys
import time
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from runner import evaluate, wilson  # noqa: E402
from teams import GAUNTLET, RENTALS, team_indices  # noqa: E402
from tune import table_at, team_search  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
CURVE = json.loads((ROOT / "results/curve.json").read_text())
OUT = ROOT / "results/final.json"
TARGETS = [0.10, 0.30, 0.50, 0.80]
BASELINE = dict(baseline=1, damage_roll=236, p_cure_paralysis=0, p_cure_sleep_move=0, p_cure_poison=0,
                p_cure_other=0, p_knockout=0, heal_below=0, p_heal=0, p_substitute=0, substitute_min_hp=0,
                p_paralyze=0, p_reflect=0, reflect_threat=0, p_mist=0, p_amnesia=0, amnesia_max_stage=0,
                amnesia_safe=0, p_blunder=0, p_second_blunder=0, pass_second=0, p_second_attack=256)


def perfect_rate(rules, team, rollout_games=1500):
    h_wins, h_games = evaluate(rules, team, 6000, seed=123)
    r_wins, r_games = evaluate(rules, team, rollout_games, policy=1, rollouts=64, seed=321)
    best_wins, best_games = max((h_wins, h_games), (r_wins, r_games), key=lambda x: x[0] / x[1])
    return 1 - best_wins / best_games, wilson(best_games - best_wins, best_games), h_wins / h_games, r_wins / r_games


def interpolate(target):
    points = sorted((v["level"], v["mewtwo_vs_perfect"]) for v in CURVE.values())
    # enforce monotonicity (running max) before interpolating
    mono, running = [], 0.0
    for level, rate in points:
        running = max(running, rate)
        mono.append((level, running))
    for (l0, r0), (l1, r1) in zip(mono, mono[1:]):
        if r0 <= target <= r1:
            return l0 if r1 == r0 else l0 + (target - r0) * (l1 - l0) / (r1 - r0)
    return mono[-1][0] if target > mono[-1][1] else mono[0][0]


def main():
    data = json.loads(OUT.read_text()) if OUT.exists() else {}
    for target in TARGETS:
        key = f"{int(target * 100)}"
        if key in data:
            continue
        t0 = time.time()
        level = interpolate(target)
        lo, hi = max(0.0, level - 0.06), min(1.0, level + 0.06)
        best = None
        for _ in range(3):
            rules = table_at(level)
            team = team_search(rules, games=800, log=None)
            rate, ci, h, r = perfect_rate(rules, team, rollout_games=800)
            print(f"  target {target}: level {level:.4f} -> {rate:.3f} team {[RENTALS[i]['name'] for i in team]}", flush=True)
            if best is None or abs(rate - target) < abs(best["rate"] - target):
                best = dict(level=level, rules=rules, team=team, rate=rate)
            if abs(rate - target) <= 0.02:
                break
            if rate > target:
                hi = level
            else:
                lo = level
            level = (lo + hi) / 2
        rules, team = best["rules"], best["team"]
        rate, ci, h, r = perfect_rate(rules, team, rollout_games=1500)
        gauntlet = {}
        for name, names in GAUNTLET.items():
            idx = team_indices(names)
            hw, hg = evaluate(rules, idx, 4000, seed=555)
            rw, rg = evaluate(rules, idx, 300, policy=1, rollouts=64, seed=556)
            gauntlet[name] = dict(heuristic_mewtwo=1 - hw / hg, rollout_mewtwo=1 - rw / rg,
                                  mewtwo_vs_best_play=1 - max(hw / hg, rw / rg))
        data[key] = dict(target=target, level=best["level"], rules=rules,
                         team=[RENTALS[i]["name"] for i in team], team_indices=team,
                         mewtwo_vs_perfect=rate, ci=list(ci), heuristic_player_win=h, rollout_player_win=r,
                         gauntlet=gauntlet, seconds=time.time() - t0)
        print(f"TARGET {key}: {json.dumps(data[key])}", flush=True)
        OUT.write_text(json.dumps(data, indent=1))
    if "baseline" not in data:
        gauntlet = {}
        for name, names in GAUNTLET.items():
            idx = team_indices(names)
            hw, hg = evaluate(BASELINE, idx, 4000, seed=555)
            gauntlet[name] = dict(heuristic_mewtwo=1 - hw / hg)
        team = team_search(BASELINE, games=800)
        rate, ci, h, r = perfect_rate(BASELINE, team, rollout_games=800)
        data["baseline"] = dict(rules=BASELINE, team=[RENTALS[i]["name"] for i in team], mewtwo_vs_perfect=rate,
                                ci=list(ci), gauntlet=gauntlet)
        OUT.write_text(json.dumps(data, indent=1))
        print("BASELINE", json.dumps(data["baseline"]), flush=True)


if __name__ == "__main__":
    main()
