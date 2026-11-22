# What it is
The Pokemon Stadium games are unique for being one of the few Nintendo 64 games to get separate ROMs for each european country. This is because each country had to dub the announcer lines. The European dubs are not an oddity: the German dub of stadium 1 is famous for its bizarre lines while the Spanish and French dubs were done by national voice acting legends.

To this end: the Hippodrome ports want to embrace the international identity of the stadium games and make sure ALL languages are supported.

## Supported Languages
The following languages are considered *first-class*, all Team Hippodrome port should support them with extensive testing and oversight to ensure accurate translations.
- English
- Spanish

The following languages are considered *auxiliary*, they should be prioritized for checking once first-class languages have finished QA
- Japanese
- German
- Portuguese

The following languages are considered *legacy*. While AI translation and ROM support are available, this is simply because these languages have localized versions of the Stadium games (or in Korea's case, the gen2 GBC game). Hippodrome ports do not promise extensive QA beyond ensuring that nothing crashes when using these languages.
- Korean
- French
- Italian

### Official vs Non-official languages
The following languages are non-official, as they have no localization of the stadium games:
- Korean
- Portuguese

All other languages are considered official.


## ROM text rules
Team Hippodrome port repos should *never* contain verbatim snippets of non-functional text ROM text in their repo. The only exception is if the non-functional text is  to be rendered in plain text for ROM-matching. 

Text is considered non-functional when it does not concern giving player instructions on how to play the game.
Examples of non-functional text include:
- Pokedex entries
- Dialogue
- Flavor text

The repos should only contain functional text when it is *essential* for understanding functionality via comment. Repos should use at most 15 words of ROM text in comments.

Translations of ROM text are allowed.

## Translation Architecture For Stadium 1 and 2
Translations are stored in csv files in a `translation` folder at the root of the repo. For each version of the ROM, the repo stores a csv that stores the md5-hashes and address points of all text that appears in the that ROM. The other columns of that CSV contain the plaintext translation of the string into the various languages.

Example: `en.csv` would have the columns
- `id`, `md5`, `address`, `es`, `jp`, `pt`, `de`, `ko`, `fr`, `it`

Separately, there would be a `kanji.csv` for rendering the Japanese ROM text with kanji since the original ROM was kana-only.

Every official language will also have a `{code}_localized.csv` for computer opponent Pokemon nicknames. (See Pokemon nicknames below)

### Stadium Zero
Because Stadium zero was never translated into any language other than japanese, Plethron has a completely different translation architecture which is detailed [here](https://github.com/team-hippodrome/plethron/tree/main/translations).

## Rendering
Translated text is drawn with two shared fonts in `assets/font`, the same in every port, through `hippodrome/text_font.h`
(moved from Plethron). The library builds each glyph in the style of the games' own font (white fill, a one-pixel dark
outline from 16 pixels up) as an IA8 texture; the port loads the files, picks the face and caches the glyphs.

| File | What it is | Licence |
| --- | --- | --- |
| `unifont_subset.hex` | GNU Unifont 16-pixel bitmaps of every character in every port's tables: the default face, and the fallback for any character Pop lacks | GPL 2+ with the font embedding exception, or OFL 1.1 (`LICENSE-unifont.txt`) |
| `unifont_subset.otf` | The same characters as outlines, for text a port draws with Dear ImGui (its launcher) | as above |
| `pop_subset.ttf` | Pop, the prototype rounded font: Latin drawn by Team Hippodrome, kana and kanji from M PLUS Rounded 1c, Hangul from Jua | OFL 1.1 (`LICENSE-pop.txt`) |

The subsets cover the union of every port's tables, and only grow. After a port's tables gain characters, run
`tools/font/make_unifont_subset.py --tables <every port's tables>` (and `tools/font/make_pop_font.py` with the base
fonts it names, from google/fonts).

## Translation Guidelines
### Dialects
If a translation must use a term or spelling that differs between regions, the following national dialects are considered the fallback in case of ambiguity.
- English: American
- Spanish: Latin American, with a fallback to Mexican in case of split terminology within LATAM
  - *Exception:* When *adding* (not translating) text to the official Spanish ROM, additions should be written in Castilian, since the original Spanish localization was Castilian.
- Portuguese: Brazilian
- German: Bundesdeutsch
- French: Metropolitan

### In-universe terminology

In-universe terminology should follow the official localizations by TPCI. This can be queried (respectfully) via PokeAPI.

### Pokemon Nicknames
Pokemon nicknames are offered in purely transliterated form in the standard CSV files. Localized nicknames are offered in the localized CSV.

All trainers in Stadium 1 and 2 have nicknames that follow some sort of gimmick (for example a prefix, suffix, or transformation). Localized names should attempt to apply this gimmick to the native names of the pokemon species. 

Example:

|id|md5|address|jp|en|es|de|pt|ko|fr|it|
|---|---|---|---|---|---|---|---|---|---|---|
BURGLAR_ARCANINE|e163a4b50e6e2f74c8ba807434ee2cf3|82402390 82403310 82404290 82405210|どろンディ|Thiefcanine|Ladricanine|Diebkani|Ladracanine|도둑디|Volcanin|Ladrocanine