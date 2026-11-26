/* Player policies: a hand-written expert heuristic (fast, used for mass runs and
 * as the rollout default policy) and a Monte Carlo rollout player that knows
 * Mewtwo's rule table (the "near-perfect" player). */
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "sim.h"
#include "gen_data.h"

#define MAX_TURNS 300

static int alive_others(const Battle* battle) {
    int count = 0;
    for (int i = 0; i < battle->player_count; i++)
        if (i != battle->active && battle->player[i].hp > 0) count++;
    return count;
}

/* Value of using `move` right now, on a rough 0..1000+ scale. */
static int move_value(const Battle* battle, int move) {
    const Mon* me = &battle->player[battle->active];
    const Mon* mewtwo = &battle->mewtwo;
    const Volatile* my_vol = &battle->volatile_state[SIDE_PLAYER];
    const Volatile* m_vol = &battle->volatile_state[SIDE_MEWTWO];
    const MoveData* data = &kMoves[move];
    int effect = data->effect;
    int acc = data->accuracy + 1;
    int sub = m_vol->substitute_hp > 0;
    int m_status = mewtwo->status;
    int m_hp = sub ? m_vol->substitute_hp : mewtwo->hp;
    int my_speed = effective_stat(me, my_vol, STAT_SPEED);
    int m_speed = effective_stat(mewtwo, m_vol, STAT_SPEED);
    switch (effect) {
        case 0x20: return (!m_status && !sub) ? acc * 450 / 256 : 0;                 /* sleep */
        case 0x43: return (!m_status && !sub) ? acc * 380 / 256 : 0;                 /* paralyze */
        case 0x42: return (!m_status && !sub) ? acc * (move == MV_TOXIC ? 220 : 120) / 256 : 0;
        case 0x31: return (!m_vol->confusion_turns && !sub) ? acc * 90 / 256 : 0;
        case 0x54: return (!m_vol->leech_seed && !sub) ? acc * 140 / 256 : 0;
        case 0x56: return m_vol->disabled_slot < 0 ? 25 : 0;
        case 0x26:                                                                    /* OHKO */
            if (my_speed < m_speed) return 0;
            return sub ? 150 : acc * 1100 / 256;
        case 0x07: {                                                                  /* explosion */
            if (alive_others(battle) == 0) return 0; /* self-KO clause */
            int damage = expected_damage(battle, SIDE_PLAYER, move);
            int value = damage >= m_hp ? (sub ? 200 : 1300) : damage * 900 / (m_hp ? m_hp : 1);
            return value - 250 * me->hp / me->max_hp;
        }
        case 0x41: return my_vol->reflect ? 0 : 40;
        case 0x40: return my_vol->light_screen ? 0 : 110;
        case 0x4F: return (!my_vol->substitute_hp && me->hp * 4 > me->max_hp) ? 120 : 0;
        case 0x38:
            if (move == MV_REST) return (me->hp * 3 < me->max_hp || me->status) && me->hp < me->max_hp ? 230 : 0;
            return me->hp * 2 < me->max_hp ? 220 : (me->hp * 4 < me->max_hp * 3 ? 60 : 0);
        case 0x19: {                                                                  /* haze */
            int boosts = m_vol->stage[STAT_SPECIAL] - 6 + (m_vol->reflect ? 2 : 0) + (m_status ? -3 : 0);
            return boosts > 0 ? 80 * boosts : 0;
        }
        case 0x35: case 0x0D: return my_vol->stage[STAT_SPECIAL] < 10 ? 70 : 0;       /* special up */
        case 0x32: case 0x0A: return my_vol->stage[STAT_ATTACK] < 10 ? 60 : 0;
        case 0x33: case 0x0B: return my_vol->stage[STAT_DEFENSE] < 9 ? 25 : 0;
        case 0x34: case 0x0C: return (my_speed < m_speed && my_vol->stage[STAT_SPEED] < 12) ? 50 : 0;
        case 0x37: case 0x0F: return my_vol->stage[STAT_EVASION] < 12 ? 90 : 0;
        case 0x16: case 0x3E: case 0x17: case 0x3F:
            return (sub || m_vol->mist || m_vol->stage[STAT_ACCURACY] <= 1) ? 0 : 70;
        case 0x12: case 0x13: case 0x14: case 0x15: case 0x3A: case 0x3B: case 0x3C: case 0x3D:
            return (sub || m_vol->mist) ? 0 : 30;
        case 0x1A: return 45;                                                          /* bide */
        case 0x53: return 70;                                                          /* metronome */
        case 0x09: return m_vol->last_move ? 60 : 0;
        case 0x55: case 0x1C: case 0x18: case 0x39: case 0x52: case 0x2E: case 0x2F: return 0;
    }
    if (move == MV_COUNTER) return 20;
    if (data->power == 0 && move != MV_SEISMIC_TOSS && move != MV_NIGHT_SHADE) return 0;
    if (effect == 0x08 && (m_status != STATUS_SLEEP || sub)) return 0;
    if (effect == 0x03 && sub) return 0;
    int damage = expected_damage(battle, SIDE_PLAYER, move);
    int value = damage >= m_hp ? 1000 : damage * 800 / (m_hp ? m_hp : 1);
    if (effect == 0x27) value = value * 45 / 100;
    if (effect == 0x2B) value = value * 75 / 100;
    if (effect == 0x50 && damage < m_hp) value = value * 55 / 100;
    if (effect == 0x2A && !sub) value += 120;                                          /* trapping */
    if ((effect == 0x06 || effect == 0x24) && !m_status && !sub) value += 30;
    if (effect == 0x1B) value = value * 80 / 100;
    return value;
}

int heuristic_choose(Battle* battle, int noise) {
    if (player_forced_action(battle)) return -1;
    const Mon* me = &battle->player[battle->active];
    int best = -1, best_value = -1, any = 0;
    for (int i = 0; i < me->move_count; i++) {
        if (!move_is_usable(battle, SIDE_PLAYER, i)) continue;
        any = 1;
        int value = move_value(battle, me->moves[i]);
        if (noise) value += rng_range(battle, noise + 1);
        if (value > best_value) { best_value = value; best = i; }
    }
    if (!any) return 0; /* struggle */
    return best;
}

static int mon_matchup(Battle* battle, int index) {
    int saved = battle->active;
    battle->active = index;
    Volatile saved_vol = battle->volatile_state[SIDE_PLAYER];
    memset(&battle->volatile_state[SIDE_PLAYER], 0, sizeof(Volatile));
    for (int i = 0; i < 6; i++) battle->volatile_state[SIDE_PLAYER].stage[i] = 6;
    battle->volatile_state[SIDE_PLAYER].disabled_slot = -1;
    int best = 0;
    const Mon* mon = &battle->player[index];
    for (int i = 0; i < mon->move_count; i++) {
        if (mon->pp[i] <= 0) continue;
        int value = move_value(battle, mon->moves[i]);
        if (value > best) best = value;
    }
    battle->volatile_state[SIDE_PLAYER] = saved_vol;
    battle->active = saved;
    return best + mon->hp * 100 / mon->max_hp;
}

int heuristic_replacement(Battle* battle) {
    int best = -1, best_value = -1;
    for (int i = 0; i < battle->player_count; i++) {
        if (battle->player[i].hp <= 0 || i == battle->active) continue;
        int value = mon_matchup(battle, i);
        if (value > best_value) { best_value = value; best = i; }
    }
    return best;
}

#define MAX_ROLLOUTS 4096

/* Fills out[k] with the outcome of rollout k (1000 = player win, plus a small
 * damage tie-break). Rollout k uses the same seed for every candidate. */
static void rollout_values(const Battle* battle, int action, int replacement, int count, uint64_t base_seed, int* out) {
    for (int k = 0; k < count; k++) {
        Battle copy = *battle;
        copy.player_policy = 0;
        copy.log = 0;
        copy.rng = (base_seed + (uint64_t)k * 0x9E3779B97F4A7C15ULL) | 1; /* common random numbers */
        if (replacement >= 0) {
            copy.active = replacement;
        } else {
            battle_play_turn(&copy, action);
        }
        if (copy.winner < 0) battle_run(&copy, MAX_TURNS);
        out[k] = copy.winner == SIDE_PLAYER ? 1000 : 10 - copy.mewtwo.hp * 10 / copy.mewtwo.max_hp;
    }
}

/* Policy improvement over the heuristic: deviate from the default only when
 * the paired rollout difference is > 2 standard errors. */
static int improve(Battle* battle, const int* candidates, int count, int default_index, int is_replacement) {
    static int values[16][MAX_ROLLOUTS];
    int rollouts = battle->rollout_count > MAX_ROLLOUTS ? MAX_ROLLOUTS : battle->rollout_count;
    uint64_t base_seed = ((uint64_t)rng_next(battle) << 32) | rng_next(battle);
    for (int c = 0; c < count; c++)
        rollout_values(battle, is_replacement ? 0 : candidates[c], is_replacement ? candidates[c] : -1, rollouts, base_seed, values[c]);
    int best = default_index;
    double best_lower = 0.0;
    for (int c = 0; c < count; c++) {
        if (c == default_index) continue;
        double sum = 0, sum_sq = 0;
        for (int k = 0; k < rollouts; k++) {
            double d = values[c][k] - values[default_index][k];
            sum += d;
            sum_sq += d * d;
        }
        double mean = sum / rollouts;
        double var = (sum_sq / rollouts - mean * mean) / (rollouts > 1 ? rollouts - 1 : 1);
        double lower = mean - 2.0 * sqrt(var > 0 ? var : 0);
        if (battle->log) printf("   [cand %d mean diff %.1f lower %.1f]\n", candidates[c], mean, lower);
        if (lower > best_lower) { best_lower = lower; best = c; }
    }
    return candidates[best];
}

int player_choose(Battle* battle) {
    if (battle->player_policy == 0) return heuristic_choose(battle, battle->heuristic_noise);
    if (player_forced_action(battle)) return -1;
    int candidates[16], count = 0;
    const Mon* me = &battle->player[battle->active];
    int default_action = heuristic_choose(battle, 0);
    for (int i = 0; i < me->move_count; i++) if (move_is_usable(battle, SIDE_PLAYER, i)) candidates[count++] = i;
    if (!count) candidates[count++] = 0;
    for (int i = 0; i < battle->player_count; i++)
        if (i != battle->active && battle->player[i].hp > 0) candidates[count++] = ACTION_SWITCH(i);
    if (count == 1) return candidates[0];
    int default_index = 0;
    for (int c = 0; c < count; c++) if (candidates[c] == default_action) default_index = c;
    return improve(battle, candidates, count, default_index, 0);
}

int player_choose_replacement(Battle* battle) {
    if (battle->player_policy == 0) return heuristic_replacement(battle);
    int candidates[6], count = 0, default_action = heuristic_replacement(battle), default_index = 0;
    for (int i = 0; i < battle->player_count; i++) {
        if (battle->player[i].hp <= 0 || i == battle->active) continue;
        if (i == default_action) default_index = count;
        candidates[count++] = i;
    }
    if (count == 1) return candidates[0];
    return improve(battle, candidates, count, default_index, 1);
}
