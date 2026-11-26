# What it is
The Mewtwo fight is quite easy in stadium 1 as despite how powerful Mewtwo is; a party of 6 is going to overwhelm any one pokemon. Thus, this enhancement makes Mewtwo harder.

# Modifications 
## Cannot be frozen
Mewtwo cannot be frozen. Burn, Sleep, Poison, and Paralysis can be applied to mewtwo.

## Expanded Move set
Mewtwo's move set is expanded to 12 moves:
1. Psychic
2. Amnesia
3. Recover
4. Blizzard
5. Mist
6. Thunderbolt
7. Submission
8. Substitute
9.  Reflect
10. Thunder Wave
11. Earthquake (Illegal)
12. Heal Bell (Not in Gen 1)

Heal Bell should be implemented as a move that cures status conditions. Mewtwo only started being able to use Earthquake in gen 3 so the illegality marking code needs a special exception for this fight.

Heal bell should also clear volatile status conditions like leech seed and confusion. 

### Summary Bar and Trainer Memory behavior for Mewtwo
To keep the surprise to the players that don't spoil themselves: Mewtwo's summary bar should only list 4 moves at first. When the fifth move is revealed: a new row should be added and the summary card expanded accordingly. Further revealed moves also add rows in the same fashion

## Round 2 Only: Two moves per turn
In Round 2, in addition to its expanded moveset; Mewtwo can move twice per turn.

Mewtwo's AI should process each of its two moves separately. That is to say: the second move is given the battle state after the first move as input.

The turn order can never be Mewtwo -> Player -> Mewtwo. If Mewtwo uses heal bell to heal paralysis on its first turn 1, either it already outspeed the player or was outspeeded by the player. 

If a Pokemon faints, all of Mewtwo's remaining moves are canceled.

## AI Logic
See [MEWTWO_AI_RULES.md](./MEWTWO_AI_RULES.md). The labels in the launcher for the settings should be:
- Easy: 10%
- Medium: 30%
- Hard: 50%
- Lunatic: 80%
- Kaizo: No blunders and no move skips.