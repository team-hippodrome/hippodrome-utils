# What it is
Trainer memory is a flag stored in an auxiliary save file that records what moves the player has seen opponent trainer's pokemon use. It is stored as a binary mask of all moves of all pokemon used by all trainers in all battles.

Upon the use of a move by a COM-controlled pokemon, that move's bit (identified by its trainer, species, cup, division, and Round 1/2 (For Stadium 1 and 2))

Trainer memory not displayed for Stadium 2's challenge cup since pokemon are randomized. Stadium Zero's level 50-55 tournament does have trainer memory since only the trainer order is randomized.

Trainer memory should be able to be seen in the screen directly before when the player would normally pick their GB/rental pokemon by pressing the C-button.

Trainer memory should reveal the following:
- The Trainer the player has reached that trainer before
- The trainer's Pokemon if the player has reached (or encountered, in randomized trainer order cups) that trainer before
    - Exception: The player is in the level 30 tournament of stadium zero AND fought the trainer in the Poke Cup division (all trainers have the same pokemon in all cups in Zero lv30, only differing in stats and moves between divisions)
- Moves the player has seen the pokemon use in battle before
- The speed stat estimate of the pokemon (See ./SPEED_STAT_ESTIMATE)