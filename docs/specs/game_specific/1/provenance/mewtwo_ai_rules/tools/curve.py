"""Stage 1: map difficulty level -> Mewtwo win rate vs near-perfect play.
Writes results/curve.json incrementally."""
import json
import sys
import time
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from runner import evaluate, wilson  # noqa: E402
from teams import RENTALS  # noqa: E402
from tune import table_at, team_search  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
LEVELS = [float(x) for x in sys.argv[1].split(",")] if len(sys.argv) > 1 else \
    [0.0, 0.05, 0.1, 0.15, 0.2, 0.25, 0.3, 0.35, 0.4, 0.45, 0.5, 0.6]
OUT = ROOT / "results/curve.json"
OUT.parent.mkdir(exist_ok=True)
data = json.loads(OUT.read_text()) if OUT.exists() else {}

for level in LEVELS:
    key = f"{level:.4f}"
    if key in data:
        continue
    t0 = time.time()
    rules = table_at(level)
    team = team_search(rules, games=500, log=lambda m: print(m, flush=True))
    h_wins, h_games = evaluate(rules, team, 4000, seed=99)
    r_wins, r_games = evaluate(rules, team, 600, policy=1, rollouts=64, seed=17)
    lo, hi = wilson(r_games - r_wins, r_games)
    data[key] = dict(level=level, rules=rules, team=[RENTALS[i]["name"] for i in team], team_indices=team,
                     heuristic_player_win=h_wins / h_games, rollout_player_win=r_wins / r_games,
                     mewtwo_vs_perfect=1 - max(h_wins / h_games, r_wins / r_games),
                     mewtwo_ci=[lo, hi], rollout_games=r_games, seconds=time.time() - t0)
    print(f"LEVEL {key}: {data[key]['team']} heuristic {h_wins / h_games:.3f} rollout {r_wins / r_games:.3f} "
          f"({time.time() - t0:.0f}s)", flush=True)
    OUT.write_text(json.dumps(data, indent=1))
