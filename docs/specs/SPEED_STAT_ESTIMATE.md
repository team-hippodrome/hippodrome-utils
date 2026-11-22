Speed stat estimates are a UI element borrowed from Pokemon Showdown. Only the idea is borrowed: the logic is derived from the decompilations (pokestadium0 and pokestadium, and pokered for the Gen 1 formulas), not from Showdown's code.

They are located under the moves, below the HP bar, in the cards for Trainer memory/Summary bars (see ./PARTY_SUMMARY_BARS.md).

The speed stat estimate should have two entries: a min and a max estimate. They are initialize as the minimum possible speed stat that pokemon species can have at that level and the maximum possible speed stat that pokemon species can have at that level. For base rulesets, this can be found by simply getting the speed stat at 0 Stat Exp and 0 DVs vs 65535 stat exp and 15 dvs for the min and max respectively.

If that pokemon uses a move in the same priority bracket as the user (for gen 1, counter is the only move with negative priority and quick attack is the only move with positive priority), the speed stat estimates Minimum should update to the player current pokemon's speed state. Likewise, the maximum should update to the player current pokemon's speed stat if the player outspeed the COMs. these estimates should be stored in an auxiliary saved filed keyed like trainer memory (trainer, species, cup, division, round).

Priority brackets come from the move data, not from a hardcoded Gen 1 list. Stadium 2's port will support custom moves (including moves backported by mods), so the estimate must not only work for Gen 1.

A turn where either side switches instead of using a move says nothing about speed, and does not update the estimate.

The speed stat estimate UI should update on speed increases/decrease and on a pokemon using Transform, but that should NOT overwrite the auxiliary save file of estimates. Estimates should normalize any speed boosts/decreases that are active in the battle.

The estimate should account for the paralysis speed stat halving.