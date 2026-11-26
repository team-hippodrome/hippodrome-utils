/* Command line driver.
 *   sim eval --rules r0,r1,... --team a,b,c --games N [--policy 0|1] [--rollouts K]
 *            [--noise N] [--seed S] [--log]
 *   sim screen --rules ... --games N     (each rental alone: wins, mean damage dealt)
 *   sim list                             (rental index, name)
 * Rules are the 22 integers of MewtwoRules after the name. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim.h"
#include "gen_data.h"

static int parse_ints(const char* text, int* out, int max) {
    int count = 0;
    while (*text && count < max) {
        out[count++] = (int)strtol(text, (char**)&text, 10);
        if (*text == ',') text++;
    }
    return count;
}

static void rules_from_ints(MewtwoRules* rules, const int* v) {
    int* fields = &rules->baseline;
    for (int i = 0; i < 22; i++) fields[i] = v[i];
    rules->name = "cli";
}

static int team_wins(const int* team, int team_size, const MewtwoRules* rules, int games, int noise,
                     unsigned long long seed) {
    int wins = 0;
    for (int g = 0; g < games; g++) {
        Battle battle;
        battle_init(&battle, team, team_size, rules, seed * 1000003ULL + (unsigned long long)g);
        battle.player_policy = 0;
        battle.heuristic_noise = noise;
        wins += battle_run(&battle, 300) == SIDE_PLAYER;
    }
    return wins;
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: sim eval|screen|list ...\n"); return 1; }
    MewtwoRules rules = kRuleTables[1];
    int team[6] = {0}, team_size = 0, games = 100, policy = 0, rollouts = 16, noise = 20, log = 0;
    unsigned long long seed = 1;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--rules") && i + 1 < argc) {
            int v[22];
            if (parse_ints(argv[++i], v, 22) != 22) { fprintf(stderr, "need 22 rule ints\n"); return 1; }
            rules_from_ints(&rules, v);
        } else if (!strcmp(argv[i], "--table") && i + 1 < argc) {
            rules = kRuleTables[atoi(argv[++i])];
        } else if (!strcmp(argv[i], "--team") && i + 1 < argc) {
            team_size = parse_ints(argv[++i], team, 6);
        } else if (!strcmp(argv[i], "--games") && i + 1 < argc) games = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--policy") && i + 1 < argc) policy = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--rollouts") && i + 1 < argc) rollouts = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--noise") && i + 1 < argc) noise = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--log")) log = 1;
    }
    if (!strcmp(argv[1], "list")) {
        for (int i = 0; i < NUM_RENTALS; i++) printf("%d\t%s\n", i, kRentals[i].name);
        return 0;
    }
    if (!strcmp(argv[1], "screen")) {
        for (int r = 0; r < NUM_RENTALS; r++) {
            long damage = 0;
            int wins = 0;
            for (int g = 0; g < games; g++) {
                Battle battle;
                int solo[1] = {r};
                battle_init(&battle, solo, 1, &rules, seed * 1000003ULL + (unsigned long long)g * 7919ULL + (unsigned long long)r);
                battle.player_policy = 0;
                battle.heuristic_noise = noise;
                int winner = battle_run(&battle, 300);
                wins += winner == SIDE_PLAYER;
                damage += battle.mewtwo.max_hp - battle.mewtwo.hp;
            }
            printf("%d\t%s\t%d\t%.1f\n", r, kRentals[r].name, wins, (double)damage / games);
        }
        return 0;
    }
    if (!strcmp(argv[1], "search")) {
        /* coordinate ascent from --team over the rental pool, heuristic player, common seeds */
        int current = team_wins(team, team_size, &rules, games, noise, seed);
        int improved = 1;
        while (improved) {
            improved = 0;
            for (int slot = 0; slot < team_size; slot++) {
                for (int cand = 0; cand < NUM_RENTALS; cand++) {
                    if (kRentals[cand].species == 132 || kRentals[cand].species == 137) continue; /* Ditto, Porygon */
                    int dup = 0;
                    for (int j = 0; j < team_size; j++) dup |= team[j] == cand;
                    if (dup) continue;
                    int saved = team[slot];
                    team[slot] = cand;
                    int w = team_wins(team, team_size, &rules, games, noise, seed);
                    if (w > current) { current = w; improved = 1; }
                    else team[slot] = saved;
                }
            }
            for (int slot = 1; slot < team_size; slot++) {
                int trial[6];
                trial[0] = team[slot];
                for (int j = 0, k = 1; j < team_size; j++) if (j != slot) trial[k++] = team[j];
                int w = team_wins(trial, team_size, &rules, games, noise, seed);
                if (w > current) { current = w; improved = 1; memcpy(team, trial, sizeof(trial)); }
            }
        }
        for (int j = 0; j < team_size; j++) printf("%d%s", team[j], j + 1 < team_size ? "," : " ");
        printf("%d %d\n", current, games);
        return 0;
    }
    if (!strcmp(argv[1], "eval")) {
        int wins = 0;
        long turns = 0;
        for (int g = 0; g < games; g++) {
            Battle battle;
            battle_init(&battle, team, team_size, &rules, seed * 1000003ULL + (unsigned long long)g);
            battle.player_policy = policy;
            battle.rollout_count = rollouts;
            battle.heuristic_noise = noise;
            battle.log = log;
            int winner = battle_run(&battle, 300);
            wins += winner == SIDE_PLAYER;
            turns += battle.turn;
            if (log) printf("RESULT %s\n", winner == SIDE_PLAYER ? "player" : "mewtwo");
        }
        printf("%d %d %.2f\n", wins, games, (double)turns / games);
        return 0;
    }
    return 1;
}
