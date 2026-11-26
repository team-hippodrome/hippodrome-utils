/* Gen 1 / Pokemon Stadium (US) battle simulator for the Round 2 Vs. Mewtwo fight.
 * See docs in ../README.md for the mechanics this file models and the
 * assumptions it makes. */
#pragma once
#include <stdint.h>

typedef struct {
    const char* name;
    unsigned char effect, power, type, accuracy, pp;
} MoveData;

typedef struct {
    const char* name;
    unsigned char hp, atk, df, spe, spc, t1, t2;
} SpeciesData;

typedef struct {
    const char* name;
    unsigned char species;
    unsigned char moves[4];
    unsigned char dv[4];        /* atk def spe spc */
    unsigned short statexp[5];  /* hp atk def spe spc */
} RentalData;

enum { STATUS_NONE, STATUS_SLEEP, STATUS_POISON, STATUS_BURN, STATUS_FREEZE, STATUS_PARALYSIS };
enum { STAT_ATTACK, STAT_DEFENSE, STAT_SPEED, STAT_SPECIAL, STAT_ACCURACY, STAT_EVASION };

#define MAX_MOVES 12
#define SIDE_PLAYER 0
#define SIDE_MEWTWO 1

typedef struct {
    int species;
    int types[2];
    int max_hp, hp;
    int stat[4]; /* unmodified attack defense speed special */
    int move_count;
    int moves[MAX_MOVES];
    int pp[MAX_MOVES];
    int status;
    int sleep_counter;
    int base_speed;
    int rental_index;
} Mon;

typedef struct {
    int stage[6]; /* 0..12, 6 is neutral */
    int substitute_hp;
    int reflect, light_screen, mist, focus_energy;
    int confusion_turns;
    int leech_seed;
    int toxic_counter;
    int disabled_slot, disable_turns;
    int charging_slot;   /* -1 when not charging a two-turn move */
    int charging_move;
    int invulnerable;
    int recharge;
    int bide_turns, bide_damage, bide_slot;
    int thrash_turns, thrash_move;
    int trap_turns, trap_move, trap_damage; /* this side is wrapping the foe */
    int flinch;
    int last_move;
    int last_damage_taken;       /* damage from the foe's last hit, for Counter */
    int last_hit_counterable;    /* foe's last damaging move was Normal/Fighting */
} Volatile;

/* Mewtwo rule-table parameters. Probabilities are out of 256. */
typedef struct {
    const char* name;
    int baseline;            /* 1 = approximate stock Stadium scorer, ignore rules */
    int damage_roll;         /* 217..255, roll used by PredictDamage */
    int p_cure_paralysis, p_cure_sleep_move, p_cure_poison, p_cure_other;
    int p_knockout;
    int heal_below;          /* Recover when hp*256 < max_hp*heal_below */
    int p_heal;
    int p_substitute;        /* raise a Substitute vs status/OHKO threats */
    int substitute_min_hp;   /* hp*256 must exceed max_hp*this */
    int p_paralyze;
    int p_reflect;
    int reflect_threat;      /* foe physical damage*256 >= hp*this */
    int p_mist;
    int p_amnesia;
    int amnesia_max_stage;   /* use Amnesia while special stage below this (6 = neutral) */
    int amnesia_safe;        /* foe best damage*256 < hp*this */
    int p_blunder;           /* random legal move instead of the rule result */
    int p_second_blunder;    /* extra blunder chance on the second action of a turn */
    int pass_second;         /* 0..256 chance the second action is skipped */
    int p_second_attack;     /* chance the second action may use R2/R9 attacks; else support-only or pass */
} MewtwoRules;

typedef struct {
    Mon player[6];
    int player_count;
    int active;
    Mon mewtwo;
    Volatile volatile_state[2];
    uint64_t rng;
    int turn;
    const MewtwoRules* rules;
    int mewtwo_revealed[MAX_MOVES];
    int player_policy;       /* 0 heuristic, 1 rollout */
    int rollout_count;
    int heuristic_noise;     /* 0..256 */
    int fainted_this_turn;
    int winner;              /* -1 none, 0 player, 1 mewtwo */
    int log;
} Battle;

/* engine.c */
uint32_t rng_next(Battle* battle);
int rng_byte(Battle* battle);
int rng_range(Battle* battle, int count); /* 0..count-1 */
void build_rental(Mon* mon, int rental_index);
void build_mewtwo(Mon* mon);
void estimate_mon(const Mon* real, Mon* out, int dv, int statexp);
#define ESTIMATED_DV 6          /* mean rental DV, weighted by the Round 2 rental table */
#define ESTIMATED_STATEXP 25600 /* stat exp of 136 of the 150 rentals */
void battle_init(Battle* battle, const int* team, int team_size, const MewtwoRules* rules, uint64_t seed);
int effective_stat(const Mon* mon, const Volatile* vol, int stat);
int type_multiplier_x10(int move_type, const Mon* target); /* 0, 2,5,10,20,40 encoded as x10 approx */
int predict_damage(const Battle* battle, int attacker_side, int move, int roll, int assume_crit);
int expected_damage(const Battle* battle, int attacker_side, int move);
int player_forced_action(const Battle* battle);
void battle_play_turn(Battle* battle, int player_action);
int battle_run(Battle* battle, int max_turns);
int player_alive_count(const Battle* battle);
int move_is_usable(const Battle* battle, int side, int slot);

/* player actions: 0..3 move slot, 4..9 switch to party index (action-4), -1 forced */
#define ACTION_SWITCH(k) (4 + (k))

/* ai_mewtwo.c */
int mewtwo_choose(Battle* battle, int action_index);
extern const MewtwoRules kRuleTables[];
extern const int kRuleTableCount;

/* player.c */
int player_choose(Battle* battle);
int player_choose_replacement(Battle* battle);
int heuristic_choose(Battle* battle, int noise);
int heuristic_replacement(Battle* battle);
