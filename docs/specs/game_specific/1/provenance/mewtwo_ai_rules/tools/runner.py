"""Parallel runner: splits games across worker processes (one sim process each)."""
import math
import os
import subprocess
import pathlib
from concurrent.futures import ThreadPoolExecutor

ROOT = pathlib.Path(__file__).resolve().parent.parent
SIM = str(ROOT / "sim")
WORKERS = int(os.environ.get("SIM_WORKERS", os.cpu_count() or 2))

RULE_FIELDS = ["baseline", "damage_roll", "p_cure_paralysis", "p_cure_sleep_move", "p_cure_poison",
               "p_cure_other", "p_knockout", "heal_below", "p_heal", "p_substitute", "substitute_min_hp",
               "p_paralyze", "p_reflect", "reflect_threat", "p_mist", "p_amnesia", "amnesia_max_stage",
               "amnesia_safe", "p_blunder", "p_second_blunder", "pass_second", "p_second_attack"]


def rules_arg(rules):
    return ",".join(str(int(round(rules[f]))) for f in RULE_FIELDS)


def _run(args):
    out = subprocess.run(args, capture_output=True, text=True, check=True).stdout.split()
    return int(out[0]), int(out[1])


def evaluate(rules, team, games, policy=0, rollouts=64, noise=0, seed=1, workers=WORKERS):
    """Returns (player_wins, games). Each worker gets a disjoint seed block."""
    per = math.ceil(games / workers)
    jobs = []
    for w in range(workers):
        n = min(per, games - w * per)
        if n <= 0:
            break
        jobs.append([SIM, "eval", "--rules", rules_arg(rules), "--team", ",".join(map(str, team)),
                     "--games", str(n), "--policy", str(policy), "--rollouts", str(rollouts),
                     "--noise", str(noise), "--seed", str(seed * 1000 + w)])
    with ThreadPoolExecutor(len(jobs)) as pool:
        results = list(pool.map(_run, jobs))
    return sum(r[0] for r in results), sum(r[1] for r in results)


def wilson(wins, games, z=1.96):
    if games == 0:
        return (0.0, 1.0)
    p = wins / games
    denom = 1 + z * z / games
    centre = (p + z * z / (2 * games)) / denom
    half = z * math.sqrt(p * (1 - p) / games + z * z / (4 * games * games)) / denom
    return centre - half, centre + half
