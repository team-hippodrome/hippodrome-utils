/* Battle engine: Gen 1 rules with the Pokemon Stadium (US) changes listed in
 * README.md, plus the HARDER_VS_MEWTWO two-moves-per-turn rule for Mewtwo. */
#include <stdio.h>
#include <string.h>

#include "sim.h"
#include "gen_data.h"

/* Stadium stat stage table (battle_engine_mechanics.c), percent. */
static const int kStagePercent[13] = {25, 28, 33, 40, 50, 66, 100, 150, 200, 250, 300, 350, 400};
/* Stadium accuracy stage table D_8438ACD4, numerator over 300 (1/3 .. 3). */
static const int kAccuracyStage[13] = {100, 108, 129, 150, 198, 225, 300, 399, 498, 600, 699, 798, 900};

static const int kMewtwoMoves[MAX_MOVES] = {
    MV_PSYCHIC_M, MV_AMNESIA, MV_RECOVER, MV_BLIZZARD, MV_MIST, MV_THUNDERBOLT,
    MV_SUBMISSION, MV_SUBSTITUTE, MV_REFLECT, MV_THUNDER_WAVE, MV_EARTHQUAKE, MV_HEAL_BELL,
};

enum {
    EFFECT_NONE = 0x00, EFFECT_POISON_SIDE1 = 0x02, EFFECT_DRAIN = 0x03, EFFECT_BURN_SIDE1 = 0x04,
    EFFECT_FREEZE_SIDE1 = 0x05, EFFECT_PARALYZE_SIDE1 = 0x06, EFFECT_EXPLODE = 0x07,
    EFFECT_DREAM_EATER = 0x08, EFFECT_MIRROR_MOVE = 0x09, EFFECT_ATTACK_UP1 = 0x0A,
    EFFECT_EVASION_UP1 = 0x0F, EFFECT_PAY_DAY = 0x10, EFFECT_SWIFT = 0x11, EFFECT_ATTACK_DOWN1 = 0x12,
    EFFECT_EVASION_DOWN1 = 0x17, EFFECT_CONVERSION = 0x18, EFFECT_HAZE = 0x19, EFFECT_BIDE = 0x1A,
    EFFECT_THRASH = 0x1B, EFFECT_SWITCH_TELEPORT = 0x1C, EFFECT_MULTI_HIT = 0x1D,
    EFFECT_FLINCH_SIDE1 = 0x1F, EFFECT_SLEEP = 0x20, EFFECT_POISON_SIDE2 = 0x21,
    EFFECT_BURN_SIDE2 = 0x22, EFFECT_FREEZE_SIDE2 = 0x23, EFFECT_PARALYZE_SIDE2 = 0x24,
    EFFECT_FLINCH_SIDE2 = 0x25, EFFECT_OHKO = 0x26, EFFECT_CHARGE = 0x27, EFFECT_SUPER_FANG = 0x28,
    EFFECT_SPECIAL_DAMAGE = 0x29, EFFECT_TRAPPING = 0x2A, EFFECT_FLY = 0x2B, EFFECT_ATTACK_TWICE = 0x2C,
    EFFECT_JUMP_KICK = 0x2D, EFFECT_MIST = 0x2E, EFFECT_FOCUS_ENERGY = 0x2F, EFFECT_RECOIL = 0x30,
    EFFECT_CONFUSION = 0x31, EFFECT_ATTACK_UP2 = 0x32, EFFECT_EVASION_UP2 = 0x37, EFFECT_HEAL = 0x38,
    EFFECT_TRANSFORM = 0x39, EFFECT_ATTACK_DOWN2 = 0x3A, EFFECT_EVASION_DOWN2 = 0x3F,
    EFFECT_LIGHT_SCREEN = 0x40, EFFECT_REFLECT = 0x41, EFFECT_POISON = 0x42, EFFECT_PARALYZE = 0x43,
    EFFECT_ATTACK_DOWN_SIDE = 0x44, EFFECT_SPECIAL_DOWN_SIDE = 0x47, EFFECT_CONFUSION_SIDE = 0x4C,
    EFFECT_TWINEEDLE = 0x4D, EFFECT_SUBSTITUTE = 0x4F, EFFECT_HYPER_BEAM = 0x50, EFFECT_RAGE = 0x51,
    EFFECT_MIMIC = 0x52, EFFECT_METRONOME = 0x53, EFFECT_LEECH_SEED = 0x54, EFFECT_SPLASH = 0x55,
    EFFECT_DISABLE = 0x56, EFFECT_HEAL_BELL = 0x60,
};

#define TYPE_NORMAL 0
#define TYPE_FIGHTING 1
#define TYPE_GROUND 4
#define TYPE_GHOST 8
#define TYPE_FIRE 0x14
#define TYPE_GRASS 0x16
#define TYPE_ELECTRIC 0x17
#define TYPE_ICE 0x19
#define TYPE_POISON 3

/* ---------- random numbers (xorshift64*) ---------- */
uint32_t rng_next(Battle* battle) {
    uint64_t x = battle->rng;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    battle->rng = x;
    return (uint32_t)((x * 0x2545F4914F6CDD1DULL) >> 32);
}
int rng_byte(Battle* battle) { return (int)(rng_next(battle) >> 24); }
int rng_range(Battle* battle, int count) { return (int)(((uint64_t)rng_next(battle) * (uint64_t)count) >> 32); }

/* ---------- building mons ---------- */
static int calc_stat(int base, int dv, int statexp, int level, int is_hp) {
    int exp_term = 0;
    /* ceil(sqrt(statexp)) / 4, as in Gen 1 CalcStat */
    int root = 0;
    while (root * root < statexp) root++;
    if (root > 255) root = 255;
    exp_term = root / 4;
    int value = (((base + dv) * 2 + exp_term) * level) / 100;
    return is_hp ? value + level + 10 : value + 5;
}

static void build_mon(Mon* mon, int species, const int* moves, int move_count, const int dv[4],
                      const int statexp[5], int pp_up) {
    memset(mon, 0, sizeof(*mon));
    const SpeciesData* sp = &kSpecies[species];
    int hp_dv = ((dv[0] & 1) << 3) | ((dv[1] & 1) << 2) | ((dv[2] & 1) << 1) | (dv[3] & 1);
    mon->species = species;
    mon->types[0] = sp->t1;
    mon->types[1] = sp->t2;
    mon->max_hp = mon->hp = calc_stat(sp->hp, hp_dv, statexp[0], 100, 1);
    mon->stat[STAT_ATTACK] = calc_stat(sp->atk, dv[0], statexp[1], 100, 0);
    mon->stat[STAT_DEFENSE] = calc_stat(sp->df, dv[1], statexp[2], 100, 0);
    mon->stat[STAT_SPEED] = calc_stat(sp->spe, dv[2], statexp[3], 100, 0);
    mon->stat[STAT_SPECIAL] = calc_stat(sp->spc, dv[3], statexp[4], 100, 0);
    mon->base_speed = sp->spe;
    mon->move_count = move_count;
    for (int i = 0; i < move_count; i++) {
        mon->moves[i] = moves[i];
        int pp = kMoves[moves[i]].pp;
        if (pp_up && kMoves[moves[i]].power > 0) pp = pp * 8 / 5;
        mon->pp[i] = pp;
    }
    mon->rental_index = -1;
}

void build_rental(Mon* mon, int rental_index) {
    const RentalData* r = &kRentals[rental_index];
    int moves[4], count = 0, dv[4], statexp[5];
    for (int i = 0; i < 4; i++)
        if (r->moves[i]) moves[count++] = r->moves[i];
    for (int i = 0; i < 4; i++) dv[i] = r->dv[i];
    for (int i = 0; i < 5; i++) statexp[i] = r->statexp[i];
    build_mon(mon, r->species, moves, count, dv, statexp, 0);
    mon->rental_index = rental_index;
}

/* Stats the AI assumes for a foe: species base stats with an estimated DV and
 * stat exp profile (no access to the real DVs/stat exp). Current HP stays the
 * displayed value. */
void estimate_mon(const Mon* real, Mon* out, int dv, int statexp) {
    const SpeciesData* sp = &kSpecies[real->species];
    *out = *real;
    out->stat[STAT_ATTACK] = calc_stat(sp->atk, dv, statexp, 100, 0);
    out->stat[STAT_DEFENSE] = calc_stat(sp->df, dv, statexp, 100, 0);
    out->stat[STAT_SPEED] = calc_stat(sp->spe, dv, statexp, 100, 0);
    out->stat[STAT_SPECIAL] = calc_stat(sp->spc, dv, statexp, 100, 0);
    out->max_hp = calc_stat(sp->hp, dv, statexp, 100, 1);
    if (out->hp > out->max_hp) out->max_hp = out->hp;
}

void build_mewtwo(Mon* mon) {
    int dv[4] = {15, 15, 15, 15};
    int statexp[5] = {65535, 65535, 65535, 65535, 65535};
    build_mon(mon, 150, kMewtwoMoves, MAX_MOVES, dv, statexp, 1);
}

static void reset_volatile(Volatile* vol) {
    memset(vol, 0, sizeof(*vol));
    for (int i = 0; i < 6; i++) vol->stage[i] = 6;
    vol->charging_slot = -1;
    vol->disabled_slot = -1;
    vol->bide_slot = -1;
}

void battle_init(Battle* battle, const int* team, int team_size, const MewtwoRules* rules, uint64_t seed) {
    memset(battle, 0, sizeof(*battle));
    for (int i = 0; i < team_size; i++) build_rental(&battle->player[i], team[i]);
    battle->player_count = team_size;
    build_mewtwo(&battle->mewtwo);
    reset_volatile(&battle->volatile_state[0]);
    reset_volatile(&battle->volatile_state[1]);
    battle->rng = seed * 0x9E3779B97F4A7C15ULL + 0x1234567ULL;
    if (!battle->rng) battle->rng = 1;
    battle->rules = rules;
    battle->winner = -1;
    battle->rollout_count = 16;
    battle->heuristic_noise = 20;
}

static Mon* mon_of(Battle* battle, int side) {
    return side == SIDE_MEWTWO ? &battle->mewtwo : &battle->player[battle->active];
}
static const Mon* mon_of_const(const Battle* battle, int side) {
    return side == SIDE_MEWTWO ? &battle->mewtwo : &battle->player[battle->active];
}

int player_alive_count(const Battle* battle) {
    int count = 0;
    for (int i = 0; i < battle->player_count; i++) count += battle->player[i].hp > 0;
    return count;
}

/* ---------- stats and damage ---------- */
int effective_stat(const Mon* mon, const Volatile* vol, int stat) {
    int value = mon->stat[stat] * kStagePercent[vol->stage[stat]] / 100;
    if (stat == STAT_ATTACK && mon->status == STATUS_BURN) value /= 2;
    if (stat == STAT_SPEED && mon->status == STATUS_PARALYSIS) value /= 4;
    if (value < 1) value = 1;
    if (value > 999) value = 999;
    return value;
}

int type_multiplier_x10(int move_type, const Mon* target) {
    int mult = 10;
    for (int i = 0; i < TYPE_CHART_LEN; i++) {
        if (kTypeChart[i][0] != move_type) continue;
        if (kTypeChart[i][1] == target->types[0]) mult = mult * kTypeChart[i][2] / 10;
        if (target->types[1] != target->types[0] && kTypeChart[i][1] == target->types[1])
            mult = mult * kTypeChart[i][2] / 10;
    }
    return mult;
}

static int is_special_type(int type) { return type >= 0x14; }

/* Core Gen 1 damage formula. roll 217..255, or 0 for the "max before random" value. */
static int damage_formula(const Battle* battle, int attacker_side, int move, int crit, int roll, int* type_x10) {
    const Mon* attacker = mon_of_const(battle, attacker_side);
    const Mon* defender = mon_of_const(battle, !attacker_side);
    const Volatile* att_vol = &battle->volatile_state[attacker_side];
    const Volatile* def_vol = &battle->volatile_state[!attacker_side];
    const MoveData* data = &kMoves[move];
    int special = is_special_type(data->type);
    int attack, defense;
    if (crit) {
        attack = attacker->stat[special ? STAT_SPECIAL : STAT_ATTACK];
        defense = defender->stat[special ? STAT_SPECIAL : STAT_DEFENSE];
    } else {
        attack = effective_stat(attacker, att_vol, special ? STAT_SPECIAL : STAT_ATTACK);
        defense = effective_stat(defender, def_vol, special ? STAT_SPECIAL : STAT_DEFENSE);
        if (!special && def_vol->reflect) defense *= 2;
        if (special && def_vol->light_screen) defense *= 2;
    }
    if (data->effect == EFFECT_EXPLODE) defense = defense / 2 > 0 ? defense / 2 : 1;
    if (attack > 255 || defense > 255) {
        attack /= 4;
        defense /= 4;
        if (attack < 1) attack = 1;
        if (defense < 1) defense = 1;
    }
    int level = crit ? 200 : 100;
    int damage = ((level * 2 / 5 + 2) * data->power * attack / defense) / 50;
    if (damage > 997) damage = 997;
    damage += 2;
    if (data->type == attacker->types[0] || data->type == attacker->types[1]) damage += damage / 2;
    int mult = type_multiplier_x10(data->type, defender);
    /* apply sequentially like the cartridge */
    int d = damage;
    for (int i = 0; i < TYPE_CHART_LEN; i++) {
        if (kTypeChart[i][0] != data->type) continue;
        if (kTypeChart[i][1] == defender->types[0]) d = d * kTypeChart[i][2] / 10;
        if (defender->types[1] != defender->types[0] && kTypeChart[i][1] == defender->types[1])
            d = d * kTypeChart[i][2] / 10;
    }
    if (type_x10) *type_x10 = mult;
    if (mult == 0) return 0;
    if (roll && d > 1) d = d * roll / 255;
    if (d < 1) d = 1;
    return d;
}

static int fixed_damage(const Battle* battle, int attacker_side, int move, int* is_fixed) {
    const Mon* defender = mon_of_const(battle, !attacker_side);
    *is_fixed = 1;
    switch (move) {
        case MV_SEISMIC_TOSS: case MV_NIGHT_SHADE: return 100;
        case MV_SONICBOOM: return 20;
        case MV_DRAGON_RAGE: return 40;
        case MV_PSYWAVE: return 75; /* mean of 1..149 */
        case MV_SUPER_FANG: return defender->hp / 2 > 0 ? defender->hp / 2 : 1;
    }
    *is_fixed = 0;
    return 0;
}

static int crit_threshold(const Mon* attacker, const Volatile* vol, int move) {
    int t = (attacker->base_speed + 76) / 4;
    int high = (move == MV_KARATE_CHOP || move == MV_RAZOR_LEAF || move == MV_CRABHAMMER || move == MV_SLASH);
    t *= 2;
    if (vol->focus_energy) t = t * 4 + 160 > 255 ? 255 : t * 4 + 160;
    else if (high) t = t * 4 > 255 ? 255 : t * 4;
    else t >>= 1;
    return t > 255 ? 255 : t;
}

/* AI-style prediction (BattleAI_PredictDamage): fixed roll, optional crit. */
int predict_damage(const Battle* battle, int attacker_side, int move, int roll, int assume_crit) {
    const MoveData* data = &kMoves[move];
    int is_fixed;
    int fixed = fixed_damage(battle, attacker_side, move, &is_fixed);
    if (is_fixed) return fixed;
    if (data->power == 0) return 0;
    if (data->effect == EFFECT_OHKO) return mon_of_const(battle, !attacker_side)->hp;
    int damage = damage_formula(battle, attacker_side, move, assume_crit, roll, NULL);
    if (data->effect == EFFECT_MULTI_HIT) damage *= 3;
    if (data->effect == EFFECT_ATTACK_TWICE || data->effect == EFFECT_TWINEEDLE) damage *= 2;
    return damage;
}

/* Mean damage including crit chance and accuracy (used by the player heuristic). */
int expected_damage(const Battle* battle, int attacker_side, int move) {
    const Mon* attacker = mon_of_const(battle, attacker_side);
    const Volatile* vol = &battle->volatile_state[attacker_side];
    const MoveData* data = &kMoves[move];
    int is_fixed;
    int fixed = fixed_damage(battle, attacker_side, move, &is_fixed);
    int hit = data->effect == EFFECT_SWIFT ? 256 : data->accuracy + 1;
    const Volatile* def_vol = &battle->volatile_state[!attacker_side];
    hit = hit * kAccuracyStage[vol->stage[STAT_ACCURACY]] / 300 * kAccuracyStage[12 - def_vol->stage[STAT_EVASION]] / 300;
    if (data->effect == EFFECT_SWIFT) hit = 256;
    if (hit > 256) hit = 256;
    if (is_fixed) return fixed * hit / 256;
    if (data->power == 0) return 0;
    int crit = crit_threshold(attacker, vol, move);
    int normal = damage_formula(battle, attacker_side, move, 0, 236, NULL);
    int critical = damage_formula(battle, attacker_side, move, 1, 236, NULL);
    int mean = (normal * (256 - crit) + critical * crit) / 256;
    if (data->effect == EFFECT_MULTI_HIT) mean = mean * 3;
    if (data->effect == EFFECT_ATTACK_TWICE || data->effect == EFFECT_TWINEEDLE) mean *= 2;
    return mean * hit / 256;
}

/* ---------- logging ---------- */
#define LOG(...) do { if (battle->log) printf(__VA_ARGS__); } while (0)
static const char* side_name(Battle* battle, int side) {
    return side == SIDE_MEWTWO ? "Mewtwo" : kSpecies[battle->player[battle->active].species].name;
}

/* ---------- status helpers ---------- */
static void cure_mewtwo_side(Battle* battle, int side) {
    Mon* mon = mon_of(battle, side);
    Volatile* vol = &battle->volatile_state[side];
    mon->status = STATUS_NONE;
    mon->sleep_counter = 0;
    vol->toxic_counter = 0;
    vol->confusion_turns = 0;
    vol->leech_seed = 0;
    vol->disabled_slot = -1;
    vol->disable_turns = 0;
}

static int has_type(const Mon* mon, int type) { return mon->types[0] == type || mon->types[1] == type; }

static void inflict_status(Battle* battle, int target_side, int status) {
    Mon* target = mon_of(battle, target_side);
    if (status == STATUS_FREEZE && target_side == SIDE_MEWTWO) return; /* spec: Mewtwo can't be frozen */
    target->status = status;
    if (status == STATUS_SLEEP) target->sleep_counter = 1 + rng_range(battle, 3); /* Stadium: Random & 3, nonzero */
    if (status == STATUS_FREEZE) battle->volatile_state[target_side].recharge = 0;
    LOG("  %s status -> %d\n", side_name(battle, target_side), status);
}

/* ---------- damage application ---------- */
static void deal_damage(Battle* battle, int attacker_side, int damage, int move, int* broke_sub, int* dealt) {
    Mon* target = mon_of(battle, !attacker_side);
    Volatile* tvol = &battle->volatile_state[!attacker_side];
    *broke_sub = 0;
    if (tvol->substitute_hp > 0) {
        if (damage >= tvol->substitute_hp) {
            *dealt = tvol->substitute_hp;
            tvol->substitute_hp = 0;
            *broke_sub = 1;
            LOG("  substitute broke\n");
        } else {
            tvol->substitute_hp -= damage;
            *dealt = damage;
        }
        return;
    }
    if (damage > target->hp) damage = target->hp;
    target->hp -= damage;
    *dealt = damage;
    tvol->last_damage_taken = damage;
    tvol->last_hit_counterable = move && (kMoves[move].type == TYPE_NORMAL || kMoves[move].type == TYPE_FIGHTING) && kMoves[move].power > 0;
    if (tvol->bide_turns > 0) tvol->bide_damage += damage;
    LOG("  %s takes %d (hp %d/%d)\n", side_name(battle, !attacker_side), damage, target->hp, target->max_hp);
}

static void self_damage(Battle* battle, int side, int damage) {
    Mon* mon = mon_of(battle, side);
    if (damage < 1) damage = 1;
    if (damage > mon->hp) damage = mon->hp;
    mon->hp -= damage;
}

static void heal(Mon* mon, int amount) {
    mon->hp += amount;
    if (mon->hp > mon->max_hp) mon->hp = mon->max_hp;
}

static int accuracy_hits(Battle* battle, int attacker_side, int move) {
    const MoveData* data = &kMoves[move];
    Volatile* tvol = &battle->volatile_state[!attacker_side];
    Volatile* avol = &battle->volatile_state[attacker_side];
    if (tvol->invulnerable) return 0;
    if (data->effect == EFFECT_SWIFT) return 1;
    int acc = data->accuracy;
    acc = acc * kAccuracyStage[avol->stage[STAT_ACCURACY]] / 300;
    acc = acc * kAccuracyStage[12 - tvol->stage[STAT_EVASION]] / 300;
    if (acc > 255) acc = 255;
    if (acc < 1) acc = 1;
    if (acc >= 255) return 1; /* Stadium re-rolls the 1/256 miss */
    return rng_byte(battle) < acc;
}

static int stat_effect_target(int effect, int* stat, int* amount, int* self) {
    if (effect >= 0x0A && effect <= 0x0F) { *stat = effect - 0x0A; *amount = 1; *self = 1; return 1; }
    if (effect >= 0x32 && effect <= 0x37) { *stat = effect - 0x32; *amount = 2; *self = 1; return 1; }
    if (effect >= 0x12 && effect <= 0x17) { *stat = effect - 0x12; *amount = -1; *self = 0; return 1; }
    if (effect >= 0x3A && effect <= 0x3F) { *stat = effect - 0x3A; *amount = -2; *self = 0; return 1; }
    if (effect >= 0x44 && effect <= 0x49) { *stat = effect - 0x44; *amount = -1; *self = 0; return 2; }
    return 0;
}

static void apply_stat_change(Battle* battle, int target_side, int stat, int amount) {
    Volatile* vol = &battle->volatile_state[target_side];
    if (amount < 0 && vol->mist) return;
    int stage = vol->stage[stat] + amount;
    if (stage < 0) stage = 0;
    if (stage > 12) stage = 12;
    vol->stage[stat] = stage;
}

static void do_move(Battle* battle, int side, int move, int depth);

static void reset_side_on_faint_or_switch(Battle* battle, int side) {
    reset_volatile(&battle->volatile_state[side]);
    /* trapping ends on switch (Stadium) */
    Volatile* other = &battle->volatile_state[!side];
    other->trap_turns = 0;
}

static void use_damaging(Battle* battle, int side, int move) {
    const MoveData* data = &kMoves[move];
    Mon* user = mon_of(battle, side);
    Mon* target = mon_of(battle, !side);
    Volatile* uvol = &battle->volatile_state[side];
    Volatile* tvol = &battle->volatile_state[!side];

    if (data->effect == EFFECT_DREAM_EATER && (target->status != STATUS_SLEEP || tvol->substitute_hp)) {
        LOG("  dream eater failed\n");
        return;
    }
    if (data->effect == EFFECT_DRAIN && tvol->substitute_hp) { LOG("  drain blocked by sub\n"); return; }
    if (data->effect == EFFECT_OHKO) {
        if (effective_stat(user, uvol, STAT_SPEED) < effective_stat(target, tvol, STAT_SPEED)) { LOG("  OHKO failed (slower)\n"); return; }
    }
    if (!accuracy_hits(battle, side, move)) {
        LOG("  %s missed\n", side_name(battle, side));
        if (data->effect == EFFECT_JUMP_KICK) self_damage(battle, side, 1);
        if (data->effect == EFFECT_EXPLODE) user->hp = 0;
        uvol->thrash_turns = 0;
        return;
    }
    int type_x10 = type_multiplier_x10(data->type, target);
    int is_fixed;
    int fixed = fixed_damage(battle, side, move, &is_fixed);
    if (move == MV_PSYWAVE) fixed = 1 + rng_range(battle, 149);
    if (!is_fixed && data->effect != EFFECT_OHKO && type_x10 == 0) {
        LOG("  no effect\n");
        if (data->effect == EFFECT_EXPLODE) user->hp = 0;
        return;
    }
    int hits = 1;
    if (data->effect == EFFECT_MULTI_HIT) {
        int r = rng_range(battle, 8);
        hits = r < 3 ? 2 : r < 6 ? 3 : r < 7 ? 4 : 5;
    }
    if (data->effect == EFFECT_ATTACK_TWICE || data->effect == EFFECT_TWINEEDLE) hits = 2;
    int crit = !is_fixed && data->effect != EFFECT_OHKO && rng_byte(battle) < crit_threshold(user, uvol, move);
    int damage;
    if (data->effect == EFFECT_OHKO) damage = 65535;
    else if (is_fixed) damage = fixed;
    else damage = damage_formula(battle, side, move, crit, 217 + rng_range(battle, 39), NULL);
    int total = 0, broke_sub = 0, had_sub = tvol->substitute_hp > 0;
    for (int h = 0; h < hits; h++) {
        int dealt;
        deal_damage(battle, side, damage, move, &broke_sub, &dealt);
        total += dealt;
        if (broke_sub || target->hp == 0) break;
    }
    if (data->effect == EFFECT_TRAPPING && !had_sub && target->hp > 0 && uvol->trap_turns == 0) {
        int r = rng_range(battle, 8);
        uvol->trap_turns = (r < 3 ? 2 : r < 6 ? 3 : r < 7 ? 4 : 5) - 1;
        uvol->trap_move = move;
        uvol->trap_damage = damage;
        tvol->recharge = 0;
    }
    if (data->effect == EFFECT_EXPLODE) user->hp = 0;
    int target_ko = target->hp == 0;
    if ((data->effect == EFFECT_RECOIL) && !target_ko && !broke_sub) self_damage(battle, side, total / 4);
    if (move == MV_STRUGGLE && !target_ko && !broke_sub) self_damage(battle, side, total / 2);
    if ((data->effect == EFFECT_DRAIN || data->effect == EFFECT_DREAM_EATER) && !had_sub) heal(user, total / 2 > 0 ? total / 2 : 1);
    if (data->effect == EFFECT_HYPER_BEAM) uvol->recharge = 1;
    if (data->effect == EFFECT_THRASH && uvol->thrash_turns == 0) {
        uvol->thrash_turns = 2 + rng_range(battle, 2); /* 3-4 total */
        uvol->thrash_move = move;
    }
    if (target_ko || had_sub) return;

    /* secondary effects */
    int e = data->effect;
    if (target->status == STATUS_FREEZE && data->type == TYPE_FIRE) target->status = STATUS_NONE;
    int same_type = has_type(target, data->type);
    switch (e) {
        case EFFECT_BURN_SIDE1: case EFFECT_BURN_SIDE2:
        case EFFECT_FREEZE_SIDE1: case EFFECT_FREEZE_SIDE2:
        case EFFECT_PARALYZE_SIDE1: case EFFECT_PARALYZE_SIDE2: {
            if (target->status || same_type) break;
            int chance = (e < 7 || move == MV_BLIZZARD) ? 26 : 77;
            if (rng_byte(battle) >= chance) break;
            int status = (e == EFFECT_BURN_SIDE1 || e == EFFECT_BURN_SIDE2) ? STATUS_BURN
                       : (e == EFFECT_FREEZE_SIDE1 || e == EFFECT_FREEZE_SIDE2) ? STATUS_FREEZE : STATUS_PARALYSIS;
            inflict_status(battle, !side, status);
            break;
        }
        case EFFECT_POISON_SIDE1: case EFFECT_POISON_SIDE2: case EFFECT_TWINEEDLE: {
            if (target->status || has_type(target, TYPE_POISON)) break;
            int chance = e == EFFECT_POISON_SIDE2 ? 103 : 52;
            if (rng_byte(battle) < chance) inflict_status(battle, !side, STATUS_POISON);
            break;
        }
        case EFFECT_FLINCH_SIDE1: case EFFECT_FLINCH_SIDE2:
            if (rng_byte(battle) < (e == EFFECT_FLINCH_SIDE1 ? 26 : 77)) tvol->flinch = 1;
            break;
        case EFFECT_CONFUSION_SIDE:
            if (!tvol->confusion_turns && rng_byte(battle) < 26) tvol->confusion_turns = 2 + rng_range(battle, 4);
            break;
        default: {
            int stat, amount, self;
            if (stat_effect_target(e, &stat, &amount, &self) == 2 && rng_byte(battle) < 85)
                apply_stat_change(battle, !side, stat, amount);
        }
    }
}

static void do_move(Battle* battle, int side, int move, int depth) {
    const MoveData* data = &kMoves[move];
    Mon* user = mon_of(battle, side);
    Mon* target = mon_of(battle, !side);
    Volatile* uvol = &battle->volatile_state[side];
    Volatile* tvol = &battle->volatile_state[!side];
    LOG(" %s uses %s\n", side_name(battle, side), data->name);
    uvol->last_move = move;
    int e = data->effect;
    int stat, amount, self;

    switch (e) {
        case EFFECT_METRONOME: {
            if (depth > 2) return;
            int pick;
            do { pick = 1 + rng_range(battle, 165); } while (pick == MV_METRONOME || pick == MV_STRUGGLE);
            do_move(battle, side, pick, depth + 1);
            return;
        }
        case EFFECT_MIRROR_MOVE: {
            int copy = tvol->last_move;
            if (!copy || copy == MV_MIRROR_MOVE || depth > 2 || copy == MV_HEAL_BELL) { LOG("  failed\n"); return; }
            do_move(battle, side, copy, depth + 1);
            return;
        }
        case EFFECT_HEAL:
            if (user->hp >= user->max_hp) { LOG("  failed\n"); return; }
            if (move == MV_REST) {
                user->hp = user->max_hp;
                user->status = STATUS_SLEEP;
                user->sleep_counter = 2;
                uvol->toxic_counter = 0;
            } else {
                heal(user, user->max_hp / 2);
            }
            return;
        case EFFECT_HEAL_BELL:
            cure_mewtwo_side(battle, side);
            return;
        case EFFECT_SUBSTITUTE:
            if (uvol->substitute_hp || user->hp <= user->max_hp / 4) { LOG("  failed\n"); return; }
            user->hp -= user->max_hp / 4;
            uvol->substitute_hp = user->max_hp / 4 + 1;
            return;
        case EFFECT_REFLECT: if (uvol->reflect) return; uvol->reflect = 1; return;
        case EFFECT_LIGHT_SCREEN: if (uvol->light_screen) return; uvol->light_screen = 1; return;
        case EFFECT_MIST: uvol->mist = 1; return;
        case EFFECT_FOCUS_ENERGY: uvol->focus_energy = 1; return;
        case EFFECT_HAZE:
            for (int s = 0; s < 2; s++)
                for (int i = 0; i < 6; i++) battle->volatile_state[s].stage[i] = 6;
            for (int s = 0; s < 2; s++) {
                Volatile* v = &battle->volatile_state[s];
                v->reflect = v->light_screen = v->mist = v->focus_energy = 0;
                v->confusion_turns = 0;
                v->leech_seed = 0;
                v->toxic_counter = 0;
                v->disabled_slot = -1;
            }
            target->status = STATUS_NONE;
            return;
        case EFFECT_SLEEP:
            if (target->status || tvol->substitute_hp || !accuracy_hits(battle, side, move)) { LOG("  failed\n"); return; }
            inflict_status(battle, !side, STATUS_SLEEP);
            tvol->recharge = 0;
            return;
        case EFFECT_POISON:
            if (target->status || tvol->substitute_hp || has_type(target, TYPE_POISON) || !accuracy_hits(battle, side, move)) { LOG("  failed\n"); return; }
            inflict_status(battle, !side, STATUS_POISON);
            if (move == MV_TOXIC) tvol->toxic_counter = 1;
            return;
        case EFFECT_PARALYZE:
            if (target->status || tvol->substitute_hp) { LOG("  failed\n"); return; }
            if (data->type == TYPE_ELECTRIC && has_type(target, TYPE_GROUND)) { LOG("  no effect\n"); return; }
            if (!accuracy_hits(battle, side, move)) { LOG("  missed\n"); return; }
            inflict_status(battle, !side, STATUS_PARALYSIS);
            return;
        case EFFECT_CONFUSION:
            if (tvol->confusion_turns || tvol->substitute_hp || !accuracy_hits(battle, side, move)) { LOG("  failed\n"); return; }
            tvol->confusion_turns = 2 + rng_range(battle, 4);
            return;
        case EFFECT_LEECH_SEED:
            if (tvol->leech_seed || tvol->substitute_hp || has_type(target, TYPE_GRASS) || !accuracy_hits(battle, side, move)) { LOG("  failed\n"); return; }
            tvol->leech_seed = 1;
            return;
        case EFFECT_DISABLE: {
            if (tvol->disabled_slot >= 0 || !accuracy_hits(battle, side, move)) { LOG("  failed\n"); return; }
            int options[MAX_MOVES], n = 0;
            for (int i = 0; i < target->move_count; i++) if (target->pp[i] > 0) options[n++] = i;
            if (!n) return;
            tvol->disabled_slot = options[rng_range(battle, n)];
            tvol->disable_turns = 1 + rng_range(battle, 8);
            return;
        }
        case EFFECT_BIDE:
            uvol->bide_turns = 2 + rng_range(battle, 2);
            uvol->bide_damage = 0;
            return;
        case EFFECT_SPLASH: case EFFECT_SWITCH_TELEPORT: case EFFECT_CONVERSION:
        case EFFECT_TRANSFORM: case EFFECT_MIMIC:
            LOG("  nothing happened\n");
            return;
        case EFFECT_CHARGE: case EFFECT_FLY:
            if (uvol->charging_move != move) {
                uvol->charging_move = move;
                if (e == EFFECT_FLY) uvol->invulnerable = 1;
                LOG("  charging\n");
                return;
            }
            uvol->charging_move = 0;
            uvol->invulnerable = 0;
            use_damaging(battle, side, move);
            return;
    }
    if (move == MV_COUNTER) {
        if (!uvol->last_hit_counterable || !uvol->last_damage_taken || kMoves[tvol->last_move].power == 0 ||
            !(kMoves[tvol->last_move].type == TYPE_NORMAL || kMoves[tvol->last_move].type == TYPE_FIGHTING) ||
            !accuracy_hits(battle, side, move)) { LOG("  counter failed\n"); return; }
        int broke, dealt;
        deal_damage(battle, side, uvol->last_damage_taken * 2, 0, &broke, &dealt);
        return;
    }
    int kind = stat_effect_target(e, &stat, &amount, &self);
    if (kind == 1 && data->power == 0) {
        if (self) {
            if (uvol->stage[stat] >= 12 || (stat < 4 && effective_stat(user, uvol, stat) >= 999)) { LOG("  failed\n"); return; }
            apply_stat_change(battle, side, stat, amount);
        } else {
            if (tvol->substitute_hp || tvol->mist || !accuracy_hits(battle, side, move)) { LOG("  failed\n"); return; }
            apply_stat_change(battle, !side, stat, amount);
        }
        return;
    }
    if (data->power > 0 || move == MV_SEISMIC_TOSS || move == MV_NIGHT_SHADE) use_damaging(battle, side, move);
}

/* ---------- turn flow ---------- */
int move_is_usable(const Battle* battle, int side, int slot) {
    const Mon* mon = mon_of_const(battle, side);
    const Volatile* vol = &battle->volatile_state[side];
    if (slot >= mon->move_count) return 0;
    if (mon->pp[slot] <= 0) return 0;
    if (vol->disabled_slot == slot) return 0;
    return 1;
}

/* Returns 1 if the side can act this action. Handles sleep/para/confusion etc. */
static int can_act(Battle* battle, int side) {
    Mon* mon = mon_of(battle, side);
    Volatile* vol = &battle->volatile_state[side];
    Volatile* foe = &battle->volatile_state[!side];
    if (vol->disable_turns > 0 && --vol->disable_turns == 0) vol->disabled_slot = -1;
    if (mon->status == STATUS_SLEEP) {
        if (--mon->sleep_counter <= 0) { mon->status = STATUS_NONE; LOG(" %s woke up\n", side_name(battle, side)); }
        else LOG(" %s is asleep\n", side_name(battle, side));
        return 0;
    }
    if (mon->status == STATUS_FREEZE) { LOG(" %s is frozen\n", side_name(battle, side)); return 0; }
    if (foe->trap_turns > 0) { LOG(" %s is trapped\n", side_name(battle, side)); return 0; }
    if (vol->flinch) { vol->flinch = 0; LOG(" %s flinched\n", side_name(battle, side)); return 0; }
    if (vol->recharge) { vol->recharge = 0; LOG(" %s must recharge\n", side_name(battle, side)); return 0; }
    if (vol->confusion_turns > 0) {
        if (--vol->confusion_turns == 0) {
            LOG(" %s snapped out of confusion\n", side_name(battle, side));
        } else if (rng_byte(battle) < 128) {
            int attack = effective_stat(mon, vol, STAT_ATTACK), defense = effective_stat(mon, vol, STAT_DEFENSE);
            if (vol->reflect) defense *= 2;
            if (attack > 255 || defense > 255) { attack /= 4; defense /= 4; if (!defense) defense = 1; }
            int damage = ((100 * 2 / 5 + 2) * 40 * attack / defense) / 50 + 2;
            if (vol->substitute_hp) { /* Gen 1 quirk ignored: damage hits self */ }
            self_damage(battle, side, damage);
            vol->charging_move = 0; vol->invulnerable = 0; vol->thrash_turns = 0; vol->bide_turns = 0;
            LOG(" %s hurt itself (%d)\n", side_name(battle, side), damage);
            return 0;
        }
    }
    if (mon->status == STATUS_PARALYSIS && rng_byte(battle) < 63) {
        vol->charging_move = 0; vol->invulnerable = 0; vol->thrash_turns = 0; vol->bide_turns = 0;
        LOG(" %s is fully paralyzed\n", side_name(battle, side));
        return 0;
    }
    return 1;
}

static void execute_action(Battle* battle, int side, int slot) {
    Mon* mon = mon_of(battle, side);
    Volatile* vol = &battle->volatile_state[side];
    Volatile* foe = &battle->volatile_state[!side];
    if (!can_act(battle, side)) {
        vol->bide_turns = vol->bide_turns > 0 ? 0 : 0;
        return;
    }
    /* continuing multi-turn actions */
    if (vol->trap_turns > 0) {
        vol->trap_turns--;
        int broke, dealt;
        LOG(" %s continues %s\n", side_name(battle, side), kMoves[vol->trap_move].name);
        deal_damage(battle, side, vol->trap_damage, vol->trap_move, &broke, &dealt);
        if (broke) vol->trap_turns = 0;
        return;
    }
    if (vol->bide_turns > 0) {
        if (--vol->bide_turns == 0) {
            int broke, dealt;
            LOG(" %s unleashes energy\n", side_name(battle, side));
            if (vol->bide_damage > 0) deal_damage(battle, side, vol->bide_damage * 2, 0, &broke, &dealt);
        }
        return;
    }
    if (vol->thrash_turns > 0) {
        vol->thrash_turns--;
        do_move(battle, side, vol->thrash_move, 0);
        if (vol->thrash_turns == 0 && !vol->confusion_turns) vol->confusion_turns = 2 + rng_range(battle, 4);
        return;
    }
    if (vol->charging_move) {
        do_move(battle, side, vol->charging_move, 0);
        return;
    }
    int move;
    if (slot < 0) {
        move = MV_STRUGGLE;
    } else {
        move = mon->moves[slot];
        mon->pp[slot]--;
        if (side == SIDE_MEWTWO) battle->mewtwo_revealed[slot] = 1;
    }
    (void)foe;
    do_move(battle, side, move, 0);
}

static void residual(Battle* battle, int side) {
    Mon* mon = mon_of(battle, side);
    Volatile* vol = &battle->volatile_state[side];
    if (mon->hp <= 0) return;
    int unit = mon->max_hp / 16 > 0 ? mon->max_hp / 16 : 1;
    if (mon->status == STATUS_POISON || mon->status == STATUS_BURN) {
        int damage = unit;
        if (mon->status == STATUS_POISON && vol->toxic_counter > 0) damage = unit * vol->toxic_counter++;
        self_damage(battle, side, damage);
    }
    if (vol->leech_seed && mon->hp > 0) {
        int damage = unit < mon->hp ? unit : mon->hp;
        self_damage(battle, side, damage);
        Mon* other = mon_of(battle, !side);
        if (other->hp > 0) heal(other, damage);
    }
}

int player_forced_action(const Battle* battle) {
    const Volatile* vol = &battle->volatile_state[SIDE_PLAYER];
    return vol->recharge || vol->charging_move || vol->bide_turns > 0 || vol->thrash_turns > 0 || vol->trap_turns > 0;
}

static int player_slot_or_struggle(const Battle* battle, int action) {
    if (action >= 0 && action < 4 && move_is_usable(battle, SIDE_PLAYER, action)) return action;
    for (int i = 0; i < 4; i++) if (move_is_usable(battle, SIDE_PLAYER, i)) return i;
    return -1;
}

static void switch_player(Battle* battle, int index) {
    Volatile* vol = &battle->volatile_state[SIDE_PLAYER];
    Mon* old = &battle->player[battle->active];
    (void)vol;
    reset_side_on_faint_or_switch(battle, SIDE_PLAYER);
    (void)old;
    battle->active = index;
    LOG(" Player sends out %s\n", kSpecies[battle->player[index].species].name);
}

static int any_fainted(Battle* battle) {
    return battle->mewtwo.hp <= 0 || battle->player[battle->active].hp <= 0;
}

static int mewtwo_slot_resolve(Battle* battle, int slot) {
    if (slot >= 0 && move_is_usable(battle, SIDE_MEWTWO, slot)) return slot;
    for (int i = 0; i < MAX_MOVES; i++) if (move_is_usable(battle, SIDE_MEWTWO, i)) return i;
    return -1;
}

static void mewtwo_block(Battle* battle, int first_slot) {
    for (int action = 0; action < 2; action++) {
        if (any_fainted(battle)) return; /* spec: faint cancels remaining Mewtwo moves */
        int slot = action == 0 ? first_slot : mewtwo_choose(battle, 1);
        if (slot == -2) return; /* AI elected to pass */
        slot = mewtwo_slot_resolve(battle, slot);
        execute_action(battle, SIDE_MEWTWO, slot);
    }
}

void battle_play_turn(Battle* battle, int player_action) {
    battle->turn++;
    LOG("Turn %d\n", battle->turn);
    int switching = !player_forced_action(battle) && player_action >= 4;
    int first_slot = mewtwo_choose(battle, 0);
    int player_slot = switching ? 0 : player_slot_or_struggle(battle, player_action);
    int player_move = (!switching && player_slot >= 0) ? battle->player[battle->active].moves[player_slot] : MV_STRUGGLE;
    if (player_forced_action(battle)) player_move = 0;
    int player_first;
    if (switching) {
        player_first = 1;
    } else if (player_move == MV_QUICK_ATTACK) {
        player_first = 1;
    } else if (player_move == MV_COUNTER) {
        player_first = 0;
    } else {
        int ps = effective_stat(&battle->player[battle->active], &battle->volatile_state[0], STAT_SPEED);
        int ms = effective_stat(&battle->mewtwo, &battle->volatile_state[1], STAT_SPEED);
        player_first = ps > ms ? 1 : ps < ms ? 0 : rng_range(battle, 2);
    }
    if (player_first) {
        if (switching) switch_player(battle, player_action - 4);
        else execute_action(battle, SIDE_PLAYER, player_slot);
        residual(battle, SIDE_PLAYER);
        mewtwo_block(battle, first_slot);
        residual(battle, SIDE_MEWTWO);
    } else {
        mewtwo_block(battle, first_slot);
        residual(battle, SIDE_MEWTWO);
        if (battle->player[battle->active].hp > 0 && battle->mewtwo.hp > 0) {
            execute_action(battle, SIDE_PLAYER, player_slot);
        }
        residual(battle, SIDE_PLAYER);
    }
    battle->volatile_state[0].flinch = 0;
    battle->volatile_state[1].flinch = 0;

    int alive = player_alive_count(battle);
    if (battle->mewtwo.hp <= 0) {
        /* self-KO clause: player's last mon fainting on the same turn loses */
        battle->winner = alive == 0 ? SIDE_MEWTWO : SIDE_PLAYER;
        return;
    }
    if (alive == 0) { battle->winner = SIDE_MEWTWO; return; }
    if (battle->player[battle->active].hp <= 0) {
        reset_side_on_faint_or_switch(battle, SIDE_PLAYER);
        int next = player_choose_replacement(battle);
        switch_player(battle, next);
    }
}

int battle_run(Battle* battle, int max_turns) {
    while (battle->winner < 0 && battle->turn < max_turns) {
        int action = player_choose(battle);
        battle_play_turn(battle, action);
    }
    if (battle->winner < 0) battle->winner = SIDE_MEWTWO; /* stall-out counts as a loss */
    return battle->winner;
}

const int* mewtwo_move_list(void) { return kMewtwoMoves; }
