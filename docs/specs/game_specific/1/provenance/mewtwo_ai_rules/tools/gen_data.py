"""Generate src/gen_data.h for the Mewtwo simulator.

Sources:
- pret/pokered (base stats, move table, effect constants)
- pokestadium decomp sTypeEffects (Stadium 1 type chart, read from
  src/fragments/battle_engine/battle_engine_mechanics.c)
- altissimo1 Prime Cup rental table (species, moves, DVs, stat exp);
  the page states these rentals are also the Vs. Mewtwo selection.
"""
import html
import json
import os
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent.parent
POKERED = ROOT / "data" / "pokered"
DECOMP = pathlib.Path(os.environ.get("POKESTADIUM_DIR", ROOT / "data" / "pokestadium"))

TYPE_IDS = {
    "NORMAL": 0, "FIGHTING": 1, "FLYING": 2, "POISON": 3, "GROUND": 4,
    "ROCK": 5, "BIRD": 6, "BUG": 7, "GHOST": 8, "FIRE": 0x14, "WATER": 0x15,
    "GRASS": 0x16, "ELECTRIC": 0x17, "PSYCHIC_TYPE": 0x18, "ICE": 0x19,
    "DRAGON": 0x1A,
}


def read_effects():
    text = (POKERED / "constants/move_effect_constants.asm").read_text()
    effects = {}
    value = 0
    for line in text.splitlines():
        line = line.split(";")[0].strip()
        if line.startswith("const_def"):
            value = 0
        elif line.startswith("const_skip"):
            value += 1
        elif line.startswith("const "):
            effects[line.split()[1]] = value
            value += 1
    return effects


def read_moves(effects):
    text = (POKERED / "data/moves/moves.asm").read_text()
    names = [n.strip() for n in re.findall(r'li "([^"]*)"', (POKERED / "data/moves/names.asm").read_text())]
    moves = []
    for m in re.finditer(r"^\s*move (\w+),\s*(\w+),\s*(\d+),\s*(\w+),\s*(\d+),\s*(\d+)", text, re.M):
        const, eff, power, mtype, acc, pp = m.groups()
        acc_byte = int(acc) * 255 // 100  # pokered "percent" macro
        moves.append(dict(const=const, effect=effects[eff], power=int(power),
                          type=TYPE_IDS[mtype], accuracy=acc_byte, pp=int(pp)))
    for move, name in zip(moves, names):
        move["name"] = name
    # Heal Bell (Gen 2 move, Normal, 5 PP) as move 166 with a custom effect id.
    moves.append(dict(const="HEAL_BELL", effect=0x60, power=0, type=0,
                      accuracy=255, pp=5, name="HEAL BELL"))
    return moves


def read_species():
    order = re.findall(r"dw (\w+)BaseStats|INCLUDE \"data/pokemon/base_stats/(\w+)\.asm\"",
                       (POKERED / "data/pokemon/base_stats.asm").read_text())
    order = list(order) + [("", "mew")]
    species = []
    for _, fname in order:
        if not fname:
            continue
        text = (POKERED / f"data/pokemon/base_stats/{fname}.asm").read_text()
        stats = re.search(r"db\s+(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)", text).groups()
        types = re.search(r"db (\w+), (\w+) ; type", text).groups()
        species.append(dict(name=fname, hp=int(stats[0]), atk=int(stats[1]), df=int(stats[2]),
                            spe=int(stats[3]), spc=int(stats[4]),
                            t1=TYPE_IDS[types[0]], t2=TYPE_IDS[types[1]]))
    return species


def read_type_chart():
    s = (DECOMP / "src/fragments/battle_engine/battle_engine_mechanics.c").read_text()
    i = s.index("u8 sTypeEffects[] = {")
    j = s.index("};", i)
    nums = [int(x, 0) for x in re.findall(r"0x[0-9A-Fa-f]+|\d+", s[i + 20:j])]
    out = []
    k = 0
    while nums[k] != 0xFF:
        out.append(tuple(nums[k:k + 3]))
        k += 3
    return out


def norm(name):
    return re.sub(r"[^A-Z0-9]", "", name.upper())


MOVE_ALIASES = {"PSYCHIC": "PSYCHICM", "HIGHJUMPKICK": "HIJUMPKICK", "SELFDESTRUCT": "SELFDESTRUCT",
                "VICEGRIP": "VICEGRIP", "DOUBLEEDGE": "DOUBLEEDGE"}


def read_rentals(moves, species):
    s = (ROOT / "data/prime-cup.html").read_text(encoding="utf8")
    s = s[s.index("Rental Pok", 1000000):]
    by_const = {norm(m["const"]): i + 1 for i, m in enumerate(moves)}
    by_name = {norm(m["name"]): i + 1 for i, m in enumerate(moves)}
    sp_by = {norm(sp["name"]): i + 1 for i, sp in enumerate(species)}
    sp_alias = {"NIDORANF": "NIDORANF", "NIDORANM": "NIDORANM", "MRMIME": "MRMIME", "FARFETCHD": "FARFETCHD"}
    rentals = []
    for row in re.findall(r"<tr[^>]*>(.*?)</tr>", s, re.S):
        cells = [html.unescape(re.sub("<[^>]+>", "", c)).strip()
                 for c in re.findall(r"<t[dh][^>]*>(.*?)</t[dh]>", row, re.S)]
        if len(cells) < 10 or not cells[1] or cells[2] != "100":
            continue
        name = cells[1]
        nm = norm(name.replace("♀", "F").replace("♂", "M"))
        sid = sp_by.get(sp_alias.get(nm, nm))
        if sid is None:
            raise SystemExit(f"unknown species {name}")
        mv = []
        idx = 3
        while not cells[idx].isdigit():
            if cells[idx] and cells[idx] != "N/A":
                key = norm(cells[idx])
                key = MOVE_ALIASES.get(key, key)
                mid = by_const.get(key) or by_name.get(key)
                if mid is None:
                    raise SystemExit(f"unknown move {cells[idx]} on {name}")
                mv.append(mid)
            idx += 1
        nums = [int(x) for x in cells[idx:idx + 10]]
        rentals.append(dict(name=name, species=sid, moves=mv,
                            dv=[nums[2], nums[4], nums[6], nums[8]],  # atk def spe spc
                            statexp=[nums[1], nums[3], nums[5], nums[7], nums[9]]))  # hp atk def spe spc
    return rentals


def main():
    effects = read_effects()
    moves = read_moves(effects)
    species = read_species()
    chart = read_type_chart()
    rentals = read_rentals(moves, species)
    assert len(species) == 151, len(species)
    out = ["/* Generated by tools/gen_data.py. Do not edit. */", "#pragma once", "#include \"sim.h\"", ""]
    out.append(f"#define NUM_MOVES {len(moves)}")
    out.append(f"#define NUM_RENTALS {len(rentals)}")
    out.append("static const MoveData kMoves[NUM_MOVES + 1] = {\n    {\"-\", 0, 0, 0, 0, 0},")
    for m in moves:
        out.append(f"    {{\"{m['name']}\", {m['effect']}, {m['power']}, {m['type']}, {m['accuracy']}, {m['pp']}}},")
    out.append("};")
    out.append("static const SpeciesData kSpecies[152] = {\n    {\"-\", 0, 0, 0, 0, 0, 0, 0},")
    for sp in species:
        out.append(f"    {{\"{sp['name']}\", {sp['hp']}, {sp['atk']}, {sp['df']}, {sp['spe']}, {sp['spc']}, {sp['t1']}, {sp['t2']}}},")
    out.append("};")
    out.append(f"static const unsigned char kTypeChart[{len(chart)}][3] = {{")
    out.append("    " + ", ".join(f"{{{a}, {d}, {m}}}" for a, d, m in chart))
    out.append("};")
    out.append(f"#define TYPE_CHART_LEN {len(chart)}")
    out.append("static const RentalData kRentals[NUM_RENTALS] = {")
    for r in rentals:
        mv = (r["moves"] + [0, 0, 0, 0])[:4]
        out.append(f"    {{\"{r['name']}\", {r['species']}, {{{', '.join(map(str, mv))}}}, "
                   f"{{{', '.join(map(str, r['dv']))}}}, {{{', '.join(map(str, r['statexp']))}}}}},")
    out.append("};")
    move_ids = "\n".join(f"#define MV_{m['const']} {i + 1}" for i, m in enumerate(moves))
    out.append(move_ids)
    (ROOT / "src/gen_data.h").write_text("\n".join(out) + "\n")
    (ROOT / "data/rentals.json").write_text(json.dumps(rentals, indent=1, ensure_ascii=False))
    print(len(moves), "moves", len(species), "species", len(rentals), "rentals", len(chart), "type pairs")


if __name__ == "__main__":
    main()
