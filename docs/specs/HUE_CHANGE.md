# What it is
Normally, Pokemon only change hue in the stadium games when a player gives the pokemon a nickname and the hue is assign through an obaque mechanism involving the bytes of the character in the nickname. This features allows the player to directly change the hue of the Pokemon by assigning a side-variable to store the hue choice.

This adds an attribute of hue choice to all pokemon in the game. This attribute should default to the hue choice that would be given by the nickname (or no hue change, in case of pokemon that do not have nicknames) in its generator.

# Where the player picks it
The hue is chosen on the screen where the player picks the Pokémon they take into a cup or battle (Julius, 2026-11-21):
- Stadium 1 and Stadium 2: the Pokémon select screen before a cup or gym.
- Stadium Zero, which only lets you pick Pokémon from your party: the entry screen (エントリーポケモンのせんたく), on both its party select and its rental select. In plethron, L / R step the hue by 15° on any Pokémon card there (a rental's card, or the card Z opens on a party row or a rental), and Z goes back to the nickname's hue.

The choice is not offered on the party summary.

# Restricted and unrestricted
The games only give each species hues from its own range: the species table has a minimum and a maximum in degrees
(Stadium Zero: D_80064C5C +0x10/+0x12, the same layout as Stadium 1's and Stadium 2's tables; for example Charizard
-20..15, Vaporeon -20..50), and a nickname picks a point in it. Hues outside it can look unnatural, so the setting has
three choices (Julius, 2026-11-22):
- Off: the game's own hues, no picker (the default, as every enhancement).
- Restricted: the picker only offers hues in the species' range: 0, the 15° steps inside it and its two ends
  (hu_hue_step_in_range). A choice stored while unrestricted that lies outside the range is drawn at the nearer end
  (hu_hue_clamp) but kept, so switching back shows it again.
- Unrestricted: the whole colour wheel in 15° steps (hu_hue_step).
A few species' ranges are a full turn (Stadium Zero: Magneton, Onix and ten others), so for them both modes are the same.
