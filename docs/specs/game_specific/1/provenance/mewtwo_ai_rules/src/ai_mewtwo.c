/* Mewtwo rule-table AI. A numbered first-match list evaluated once per
 * Mewtwo action (twice per turn in Round 2). See MEWTWO_AI_RULES.md. */
#include "sim.h"
#include "gen_data.h"

enum {
    SLOT_PSYCHIC, SLOT_AMNESIA, SLOT_RECOVER, SLOT_BLIZZARD, SLOT_MIST, SLOT_THUNDERBOLT,
    SLOT_SUBMISSION, SLOT_SUBSTITUTE, SLOT_REFLECT, SLOT_THUNDER_WAVE, SLOT_EARTHQUAKE, SLOT_HEAL_BELL,
};
static const int kAttackSlots[5] = {SLOT_PSYCHIC, SLOT_THUNDERBOLT, SLOT_EARTHQUAKE, SLOT_BLIZZARD, SLOT_SUBMISSION};

static int usable(const Battle* battle, int slot) { return move_is_usable(battle, SIDE_MEWTWO, slot); }

static int roll_pass(Battle* battle, int probability) {
    if (probability >= 256) return 1;
    if (probability <= 0) return 0;
    return rng_byte(battle) < probability;
}

/* ---- helper predicates (documented in the spec) ---- */
typedef struct {
    int foe_best_damage;        /* foe max-roll damage vs Mewtwo, best move */
    int foe_best_physical;      /* same, physical-type moves only */
    int foe_has_status_threat;  /* sleep/para/poison/confuse/seed/disable/OHKO */
    int foe_has_stat_drop;
    int foe_disabled_status;    /* foe is asleep or frozen */
} Threats;

static void read_threats(const Battle* battle, Threats* t) {
    const Mon* foe = &battle->player[battle->active];
    t->foe_best_damage = t->foe_best_physical = 0;
    t->foe_has_status_threat = t->foe_has_stat_drop = 0;
    for (int i = 0; i < foe->move_count; i++) {
        if (foe->pp[i] <= 0) continue;
        int move = foe->moves[i];
        const MoveData* data = &kMoves[move];
        int effect = data->effect;
        if (effect == 0x26) {
            t->foe_has_status_threat = 1;
            continue;
        }
        if (effect == 0x20 || effect == 0x43 || effect == 0x42 || effect == 0x31 || effect == 0x54 || effect == 0x56)
            t->foe_has_status_threat = 1;
        if ((effect >= 0x12 && effect <= 0x17) || (effect >= 0x3A && effect <= 0x3F)) t->foe_has_stat_drop = 1;
        if (effect == 0x07) t->foe_has_status_threat = 1; /* explosion: sub absorbs it */
        int damage = predict_damage(battle, SIDE_PLAYER, move, 255, 0);
        if (damage > t->foe_best_damage) t->foe_best_damage = damage;
        if (data->type < 0x14 && damage > t->foe_best_physical) t->foe_best_physical = damage;
    }
    t->foe_disabled_status = foe->status == STATUS_SLEEP || foe->status == STATUS_FREEZE;
}

static int best_attack(const Battle* battle, int roll, int want_knockout) {
    const Mon* foe = &battle->player[battle->active];
    const Volatile* foe_vol = &battle->volatile_state[SIDE_PLAYER];
    int best_slot = -1, best_score = -1;
    for (int i = 0; i < 5; i++) {
        int slot = kAttackSlots[i];
        if (!usable(battle, slot)) continue;
        int move = battle->mewtwo.moves[slot];
        int damage = predict_damage(battle, SIDE_MEWTWO, move, roll, 0);
        if (want_knockout) {
            if (foe_vol->substitute_hp || damage < foe->hp) continue;
            int score = kMoves[move].accuracy * 1000 + (5 - i); /* most accurate, then list order */
            if (score > best_score) { best_score = score; best_slot = slot; }
        } else {
            /* BattleAI_PickBestMove case 10: damage * accuracy/255 */
            int score = damage * kMoves[move].accuracy / 255;
            if (foe_vol->substitute_hp && damage >= foe_vol->substitute_hp) score = 2000 + kMoves[move].accuracy;
            if (score > best_score) { best_score = score; best_slot = slot; }
        }
    }
    return best_slot;
}

static int random_usable(Battle* battle) {
    int options[MAX_MOVES], count = 0;
    for (int i = 0; i < MAX_MOVES; i++) if (usable(battle, i)) options[count++] = i;
    return count ? options[rng_range(battle, count)] : -1;
}

/* Approximation of the stock Stadium scorer: KO check, heal weighting, case-10 attack score. */
static int baseline_choose(Battle* battle) {
    const Mon* me = &battle->mewtwo;
    int knockout = best_attack(battle, 236, 1);
    if (knockout >= 0) return knockout;
    if (me->hp * 2 < me->max_hp && usable(battle, SLOT_RECOVER) && rng_byte(battle) < 191) return SLOT_RECOVER;
    if (rng_byte(battle) < 64) {
        const Mon* foe = &battle->player[battle->active];
        const Volatile* foe_vol = &battle->volatile_state[SIDE_PLAYER];
        int options[6], count = 0;
        if (!foe->status && !foe_vol->substitute_hp && foe->types[0] != 4 && foe->types[1] != 4) options[count++] = SLOT_THUNDER_WAVE;
        if (battle->volatile_state[SIDE_MEWTWO].stage[STAT_SPECIAL] < 12) options[count++] = SLOT_AMNESIA;
        if (!battle->volatile_state[SIDE_MEWTWO].reflect) options[count++] = SLOT_REFLECT;
        if (!battle->volatile_state[SIDE_MEWTWO].substitute_hp && me->hp * 4 > me->max_hp) options[count++] = SLOT_SUBSTITUTE;
        if (count) {
            int slot = options[rng_range(battle, count)];
            if (usable(battle, slot)) return slot;
        }
    }
    return best_attack(battle, 236, 0);
}

/* The rules never test species identity; they read types, move effects and
 * stats (true stats, as the stock Stadium AI does). */
int mewtwo_choose(Battle* battle, int action_index) {
    const MewtwoRules* rules = battle->rules;
    const Mon* me = &battle->mewtwo;
    const Mon* foe = &battle->player[battle->active];
    const Volatile* my_vol = &battle->volatile_state[SIDE_MEWTWO];
    const Volatile* foe_vol = &battle->volatile_state[SIDE_PLAYER];
    if (rules->baseline) return baseline_choose(battle);
    if (action_index == 1 && roll_pass(battle, rules->pass_second)) return -2;
    int blunder = rules->p_blunder + (action_index == 1 ? rules->p_second_blunder : 0);
    if (roll_pass(battle, blunder)) return random_usable(battle);

    int support_only = action_index == 1 && !roll_pass(battle, rules->p_second_attack);
    Threats threats;
    read_threats(battle, &threats);

    /* R1 CURE */
    if (usable(battle, SLOT_HEAL_BELL)) {
        int p = 0;
        if (me->status == STATUS_PARALYSIS) p = rules->p_cure_paralysis;
        else if (me->status == STATUS_POISON || me->status == STATUS_BURN) p = rules->p_cure_poison;
        else if (my_vol->confusion_turns || my_vol->leech_seed || my_vol->disabled_slot >= 0) p = rules->p_cure_other;
        if (p && roll_pass(battle, p)) return SLOT_HEAL_BELL;
    }
    /* R2 KNOCKOUT */
    int knockout = best_attack(battle, rules->damage_roll, 1);
    if (!support_only && knockout >= 0 && roll_pass(battle, rules->p_knockout)) return knockout;
    /* R3 HEAL */
    if (usable(battle, SLOT_RECOVER) && me->hp * 256 < me->max_hp * rules->heal_below && roll_pass(battle, rules->p_heal))
        return SLOT_RECOVER;
    /* R4 SUBSTITUTE vs status / OHKO / explosion threats */
    if (usable(battle, SLOT_SUBSTITUTE) && !my_vol->substitute_hp && threats.foe_has_status_threat &&
        !threats.foe_disabled_status && me->hp * 256 > me->max_hp * rules->substitute_min_hp &&
        roll_pass(battle, rules->p_substitute))
        return SLOT_SUBSTITUTE;
    /* R5 PARALYZE */
    if (usable(battle, SLOT_THUNDER_WAVE) && !foe->status && !foe_vol->substitute_hp && foe->types[0] != 4 &&
        foe->types[1] != 4 && roll_pass(battle, rules->p_paralyze))
        return SLOT_THUNDER_WAVE;
    /* R6 REFLECT */
    if (usable(battle, SLOT_REFLECT) && !my_vol->reflect && threats.foe_best_physical * 256 >= me->hp * rules->reflect_threat &&
        roll_pass(battle, rules->p_reflect))
        return SLOT_REFLECT;
    /* R7 MIST */
    if (usable(battle, SLOT_MIST) && !my_vol->mist && threats.foe_has_stat_drop && roll_pass(battle, rules->p_mist))
        return SLOT_MIST;
    /* R8 AMNESIA */
    if (usable(battle, SLOT_AMNESIA) && my_vol->stage[STAT_SPECIAL] < rules->amnesia_max_stage &&
        threats.foe_best_damage * 256 < me->hp * rules->amnesia_safe && roll_pass(battle, rules->p_amnesia))
        return SLOT_AMNESIA;
    /* R9 ATTACK */
    if (support_only) return -2;
    int attack = best_attack(battle, rules->damage_roll, 0);
    return attack >= 0 ? attack : random_usable(battle);
}

/* Tuned tables are filled in by tune.py; these are the starting points. */
const MewtwoRules kRuleTables[] = {
    /* name, baseline, roll, cure_par, cure_slp, cure_psn, cure_oth, ko, heal_below, p_heal, p_sub, sub_min,
       p_para, p_reflect, reflect_threat, p_mist, p_amnesia, amn_stage, amn_safe, blunder, blunder2, pass2, p_second_attack */
    {"baseline", 1, 236, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 256},
    {"max", 0, 236, 256, 0, 256, 256, 256, 128, 256, 256, 96, 256, 256, 64, 256, 256, 10, 64, 0, 0, 0, 256},
};
const int kRuleTableCount = sizeof(kRuleTables) / sizeof(kRuleTables[0]);
