"""Benchmark gauntlet of archetype rental teams (names -> rental indices)."""
import json, pathlib
RENTALS = json.load(open(pathlib.Path(__file__).resolve().parent.parent / "data/rentals.json", encoding="utf8"))
INDEX = {r["name"]: i for i, r in enumerate(RENTALS)}
GAUNTLET = {
    "A_vanilla_cheese": ["Electrode", "Magneton", "Rhydon", "Nidoking", "Dewgong", "Kingler"],
    "B_boom_offense": ["Jynx", "Onix", "Gengar", "Exeggcute", "Raichu", "Snorlax"],
    "C_standard_strong": ["Mew", "Jolteon", "Exeggutor", "Chansey", "Tauros", "Zapdos"],
    "C2_standard_lapras": ["Mew", "Jolteon", "Exeggutor", "Chansey", "Tauros", "Lapras"],
    "D_debuff_stall": ["Venusaur", "Tentacruel", "Dugtrio", "Electrode", "Seadra", "Alakazam"],
    "E_wrap_lock": ["Dragonite", "Victreebel", "Tentacruel", "Cloyster", "Charizard", "Raichu"],
    "F_sleep_para": ["Jynx", "Gengar", "Parasect", "Exeggutor", "Raichu", "Electabuzz"],
}
def team_indices(names):
    return [INDEX[n] for n in names]
