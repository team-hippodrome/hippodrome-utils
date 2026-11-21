Speed stat estimates are a UI element borrowed from Pokemon Showdown.

They are located directly next to the HP bar in the cards for Trainer memory/Summary bars.

The speed stat estimate should have two entries: a min and a max estimate. They are initialize as the minimum possible speed stat that pokemon species can have at that level and the maximum possible speed stat that pokemon species can have at that level. For base rulesets, this can be found by simply getting the speed stat at 0 Stat Exp and 0 DVs vs 65535 stat exp and 15 dvs for the min and max respectively.

If that pokemon uses a move in the same priority bracket as the user (for gen 1, counter is the only move with negative priority and quick attack is the only move with positive priority), the speed stat estimates Minimum should update to the player current pokemon's speed state. Likewise, the maximum should update to the player current pokemon's speed stat if the player outspeed the COMs. these esimates should be stored in an auxilary saved filed keyed 

The speed stat estimate UI should update on speed increases/decrease and on a pokemon using Transform, but that should NOT overwrite the auxiliary save file of estimates. Estimates should normalize any speed boosts/decreases that are active in the battle.