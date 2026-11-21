# What it is
Normally, Pokemon only change hue in the stadium games when a player gives the pokemon a nickname and the hue is assign through an obaque mechanism involving the bytes of the character in the nickname. This features allows the player to directly change the hue of the Pokemon by assigning a side-variable to store the hue choice.

This adds an attribute of hue choice to all pokemon in the game. This attribute should default to the hue choice that would be given by the nickname (or no hue change, in case of pokemon that do not have nicknames) in its generator.

# Where the player picks it
The hue is chosen on the screen where the player picks the Pokémon they take into a cup or battle (Julius, 2026-11-21):
- Stadium 1 and Stadium 2: the Pokémon select screen before a cup or gym.
- Stadium Zero, which only lets you pick Pokémon from your party: the entry screen (エントリーポケモンのせんたく), on both its party select and its rental select. In plethron, L / R step the hue by 15° on any Pokémon card there (a rental's card, or the card Z opens on a party row or a rental), and Z goes back to the nickname's hue.

The choice is not offered on the party summary.
