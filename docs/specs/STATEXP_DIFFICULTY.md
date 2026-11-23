# Stat Exp Clamping Settings (Launcher)

## Context
Stat Exp (`0`–`65535` per stat) is the Gen 1/2 equivalent of EVs. Pokémon Stadium assigns varying hardcoded Stat Exp values to both Rental Pokémon and Opponent (CPU) Pokémon. (Adjustable DVs are intentionally excluded because the HP DV is derived from the parity bits of the other four DVs and is non-monotonic.)

## UI Controls
Add **4 discrete preset sliders/dropdowns** to the launcher, grouped into two independent sections:
1. **Rental Pokémon:** `Minimum Stat Exp` and `Maximum Stat Exp`
2. **Opponent Pokémon:** `Minimum Stat Exp` and `Maximum Stat Exp`

Each control has 7 options: `Vanilla`, `Casual`, `Easy`, `Medium`, `Hard`, `Kaizo`, `Custom`. All 4 controls default to `Vanilla`.

## Preset Values
`Vanilla` leaves that bound unmodified (no floor for Minimum; no ceiling for Maximum). Because Gen 1/Stadium Stat Exp scales with `floor(min(255, ceil(sqrt(statExp))) / 4)`, the other 5 presets use a quadratic curve `round(((k / 4) ^ 2) * 65535)` for `k = 0..4` so each step adds ~25% of the maximum Stat Exp stat bonus (`0`, `+16`, `+32`, `+48`, `+63` stat points at Lv. 100), with difficulty polarity oriented relative to the player (higher Rental stats = easier; higher Opponent stats = harder):

- **Opponent Minimum & Opponent Maximum Stat Exp:**
  - `Casual` = `0`
  - `Easy` = `4096`
  - `Medium` = `16384`
  - `Hard` = `36863`
  - `Kaizo` = `65535`

- **Rental Minimum & Rental Maximum Stat Exp (reversed):**
  - `Casual` = `65535`
  - `Easy` = `36863`
  - `Medium` = `16384`
  - `Hard` = `4096`
  - `Kaizo` = `0`

These values are selectable via drop down, with "custom" spawning a slider that.

These settings do not apply in free battle.

## Validation (Preventing Min > Max)
Within each group (Rental or Opponent), if neither `Minimum` nor `Maximum` is set to `Vanilla` and the user changes a control such that numeric `Minimum > Maximum`:
- If the user just changed `Minimum`, automatically bump `Maximum` to match the new `Minimum` value.
- If the user just changed `Maximum`, automatically lower `Minimum` to match the new `Maximum` value.

## Runtime / Patching Behavior
For each of a Pokémon's 5 Stat Exp fields (HP, Attack, Defense, Speed, Special):
1. Start with the Pokémon's original `vanillaStatExp`.
2. If `Minimum` is not `Vanilla`, set `statExp = max(statExp, minimumValue)`.
3. If `Maximum` is not `Vanilla`, set `statExp = min(statExp, maximumValue)`.
4. Ensure the Pokémon's actual in-battle stats reflect the clamped Stat Exp values.
