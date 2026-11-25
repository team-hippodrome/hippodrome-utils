# What it is
Trainer memory is a flag stored in an auxiliary save file that records what moves the player has seen opponent trainer's pokemon use. It is stored as a binary mask of all moves of all pokemon used by all trainers in all battles.

Upon the use of a move by a COM-controlled pokemon, that move's bit (identified by its trainer, species, cup, division, and Round 1/2 (For Stadium 1 and 2))

Trainer memory not displayed for Stadium 2's challenge cup since pokemon are randomized. Stadium Zero's level 50-55 tournament does have trainer memory since only the trainer order is randomized.

Trainer memory should be able to be seen in the screen directly before when the player would normally pick their GB/rental pokemon by pressing the C-button.

Trainer memory should reveal the following:
- The Trainer the player has reached that trainer before
- The trainer's Pokemon if the player has reached (or encountered, in randomized trainer order cups) that trainer before
    - Exception: The player is in the level 30 tournament of stadium zero AND fought the trainer in the Poke Cup division (all trainers have the same pokemon in all cups in Zero lv30, only differing in stats and moves between divisions). This rule is Stadium Zero only, so it lives in Plethron, not in this library.
- The trainer's portrait (the picture shown on the VS screen), if the player has met that trainer before
- Each revealed pokemon's icon
- Moves the player has seen the pokemon use in battle before, including moves that missed or failed (see the Moves section of ./PARTY_SUMMARY_BARS.md)
- The speed stat estimate of the pokemon (See ./SPEED_STAT_ESTIMATE). If a stored speed estimate contradicts the pokemon's true raw Speed (for example from a stale or corrupted save), it is cleared and reset to the initial range rather than kept.

# Window
The trainer memory window must not cover any part of the screen that the game draws once and never redraws (such as the header at the top of Stadium Zero's menu before picking pokemon), since that part would keep the window's image after it closes.

Opening trainer memory should not pause the game, even the first time. Anything slow to prepare is loaded ahead of time (Plethron does it during the N64 logo, whose frames are held anyway).
