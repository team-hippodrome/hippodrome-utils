# hippodrome-utils
Common utilities used for enhancement in team hippodrome port

## The library

A small C11 static library (`hippodrome_utils`, CMake alias `hippodrome::utils`) with the game-independent parts of
the enhancements in [docs/specs](docs/specs). Each port adds this repository as a submodule, calls
`add_subdirectory(hippodrome-utils)` and links `hippodrome::utils`; the port reads its own game's battle state, feeds
it in, and draws the result. Nothing here knows a game's memory layout.

| Header | What it does | Spec |
|---|---|---|
| `hippodrome/stats.h` | Gen 1/2 stat formula, stat stage ratios and paralysis as data (`hu_speed_rules`, `hu_gb_speed_rules`), effective Speed | SPEED_STAT_ESTIMATE |
| `hippodrome/speed_estimate.h` | A Speed range per Pokémon, narrowed from who moved first (`hu_speed_observe`) | SPEED_STAT_ESTIMATE |
| `hippodrome/trainer_memory.h` | The trainer memory store: per game/cup/division/round/trainer, the team, moves seen and Speed ranges; a small versioned file with a CRC (`hu_tm_load`/`hu_tm_save`) | TRAINER_MEMORY |
| `hippodrome/summary_card.h` | The summary bar cards: states, revealed moves, HP bands, how the model should look (`hu_card_model_look`) | PARTY_SUMMARY_BARS |
| `hippodrome/hue_choice.h` | The player's hue per Pokémon: fingerprints (`hu_hue_id_of`), the games' nickname hue (`hu_hue_nickname_shift`), the picker's 15° steps (`hu_hue_step`), a small versioned side file with a CRC (`hu_hue_load`/`hu_hue_save`) | HUE_CHANGE |
| `hippodrome/move_description.h` | Which move's description the held D-pad direction shows in the move menu, with its window's animation | MIDBATTLE_MOVE_DESCRIPTION |
| `hippodrome/order_queue.h` | The order queue's display state machine (Hidden, Showing, Move/Switch_Select, Move/Switch_Queued), its cancel rules and the COM-timed entering of the order (`hu_oq_command_frame`) | ORDER_QUEUEING |
| `hippodrome/window_anim.h` | The timing of a window that expands, then fills (and empties, then contracts): design principle 2 | AGENTS.md |

Formulas come from the decompilations (pokestadium0's `Battle_ComputePokemonStat`, the stage table at `D_8218ADF4`,
`Battle_ApplyParalysisSpeedCut`) and match pokered's. Rules that differ between games or mods (the stage table, the
paralysis cut, the stat cap) are data in `hu_speed_rules`, and move ids are 16-bit, so custom moves from mods fit.

Rules that only one game has stay in that game's port (for example Stadium Zero showing a Poké Ball Cup trainer's
team in the other cups).

### Building and testing

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The tests build only when this is the top-level project (`HIPPODROME_UTILS_TESTS`).
