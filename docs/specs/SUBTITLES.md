# What it is
Subtitles for the announcer's voice lines: while a line plays, its text is drawn in the player's language in a
translucent box above the battle message box. Plethron (Stadium Zero) has shipped this since PR #16, with the
transcriptions corrected against the audio in PR #98. Stadion will reuse the same design for its restored content (for
example the Japanese announcer on the restored Nintendo Cups while the player plays in a Western language, see
`game_specific/1/RESTORED_CONTENT.md`).

This document describes Plethron's implementation as it stands on Plethron `main` (c8a77ee), split into the parts a
port has to provide and the parts that carry over unchanged. Source files named below are in
[team-hippodrome/plethron](https://github.com/team-hippodrome/plethron).

| Piece | Plethron | Game-specific? |
| --- | --- | --- |
| Line start hook | `func_8003815C` (`game/src/libnumus/player.c`) calls `Subtitles_OnSound` | yes: where the game starts a voice line |
| Line identity | effect id in the announcer bank (JIKKYOU.WBK, effect table `D_800B41A8`) | yes: what uniquely names a line |
| "Still playing?" query | `Audio_QueueSetMonoOutputCommand(handle)` (nonzero while the sound plays) | yes |
| Text | rows `SUBTITLE_ANNOUNCER_<id>` in the translation tables | format shared, rows per game |
| Timing, wrapping, box | `game/src/subtitles.c` | no |
| Draw hook | end of `StageLoader_SetupFrame` (`game/src/stage_loader.c`) | yes: the game's end-of-frame |
| Glyphs | `Port_Font_*` over `hippodrome/text_font.h` (Unifont, or Pop) | **pluggable**: Stadion uses its ROM font |
| Toggle | Options row "Subtitles", stored as CVar `gPort.Subtitles` | menu placement per game |

# Data format
Subtitle text lives in the port's normal translation tables, so every language gets it and human translations
override AI ones like any other string.

- One row per voice line, id `SUBTITLE_ANNOUNCER_<NN>`, where `NN` is the line id as two upper-case hex digits
  (`%02X`).
- Plethron tables: `translations/ai.csv` holds the text in every language; `translations/human.csv` has an empty row
  for each id for human localizers. Lookup order is localized (only with localized names), human, then AI; an empty cell
  falls through to the next table.
- `ja` is a transcription of the audio, not ROM text. `md5` is the md5 of that transcription in EUC-JP (so it fits the
  table's md5 column, which normally hashes ROM strings), and `addr` is empty.
- Cells hold one logical line with no manual line breaks: the text is wrapped at run time to the box width. A literal
  `\n` in the text still forces a break.
- A line with no row (or an empty cell in the current language) shows nothing. Plethron leaves the badge call-outs
  (ids 01–08, "<colour> Badge, get!") without rows on purpose.
- The original-language mode shows no subtitles: `Port_Text_LookupId` returns NULL there. Japanese with kanji is a
  translated language and does show them.

The rows are packed into the port's archive at build time, so edits need a rebuild before the port shows them.

## Checking transcriptions against the audio
Plethron's `tools/announcer_clips.py <ROM> <out dir> [--asr <faster-whisper model dir>]` decodes every announcer clip
to `fxNN.wav` (NN = the row id) and, with `--asr`, writes `asr.tsv`: id, what speech recognition heard, and the
current `ja` cell. Several of the original transcriptions were wrong (Self-Destruct, id 23, said "paralyzed" instead
of "exploded"), so any new set of subtitles should be checked this way. Nothing decoded from the ROM goes into a
repository.

Zero's layout, for reference: audio pak header at ROM `0xB98000`, `files[1]` is the announcer bank (Yay0 effect
table, Yay0 `N64 PtrTablesV2` bank, wave data). An effect is a script: `0x81 <n>` selects sample n, `0x30 <ticks>`
plays it, `0x60 <ticks>` waits (values >= 0x80 take two bytes); ADPCM mono at about 11025 Hz, one tick = 1/100 s.
Stadium 1 stores its announcer differently (see Stadion integration), so its tool would be new.

# Clip/cue matching
Matching is by line identity, not by audio content or timing tables.

1. Every sound the game starts goes through one function. In Zero that is `func_8003815C`, which assigns the new
   sound's handle; the port calls `Subtitles_OnSound(effect_table, effect_id, handle)` right after the handle is
   assigned. This catches direct plays, the announcer's queue, and effects started by other effects.
2. `Subtitles_OnSound` ignores everything except the announcer bank (`effects == D_800B41A8`), an invalid handle
   (0 or -1) and ids outside 0x00–0xFF. Battle and system banks have no speech, so only the announcer bank is
   subtitled.
3. A hit publishes `(id, handle)` and bumps a serial counter. It can run on the audio thread (queued lines), so these
   three are `volatile` and the game thread only reads them. There is no lock: the draw side compares the serial with
   the last one it saw and copies id and handle when it changed. A newer line simply replaces the one on screen.

A port only needs some place where (a) a voice line starts, (b) the line's identity is known, and (c) there is a handle
or channel that can later be asked "is this still playing?".

# Timing
All timing runs in `Subtitles_Draw`, once per drawn frame, on the game thread.

- A new serial makes its line current and resets the hold counter to `SUBTITLE_HOLD_FRAMES` (45 frames).
- While `Audio_QueueSetMonoOutputCommand(handle)` is nonzero (despite its name, it reports whether that handle still
  plays) the line stays up and the hold counter is not touched.
- Once the sound has ended, the hold counter counts down one per frame; at 0 the line is cleared. So a line is visible
  for its clip length plus 45 frames, which keeps very short clips readable.
- The toggle and language are checked after the timing update, so turning subtitles off mid-line does not desync the
  state.
- Debug aid: `PORT_SUBTITLE_TEST=<hex id>` pins that line on screen every frame, so the box can be checked with the
  sound driver off (`PORT_NO_AUDIO=1`), which is how Plethron tests it headless.

Stadium games pick some announcer variants with `osGetCount`/`osGetTime`, so in lockstep test routes the picked line
never varies; that affects tests only.

# Layout and wrapping
Coordinates are 320x240 and doubled when the stage is 640x480 (`StageContext_IsHighResolution`).

| Constant | Value | Meaning |
| --- | --- | --- |
| `SUBTITLE_BOX_X` | 8 | box left edge |
| `SUBTITLE_BOX_W` | 216 | box width |
| `SUBTITLE_BOX_BOTTOM` | 176 | y of the box's bottom, just above Zero's battle message box (y 179) |
| `SUBTITLE_PAD` | 6 | text inset on each side; top inset is half |
| `SUBTITLE_GLYPH_HEIGHT` | 12 | glyph height (the trainer name boxes use 12) |
| `SUBTITLE_MAX_LINES` | 6 | extra lines are dropped |

- Line height = glyph height + 1 (scaled). Box height = lines × line height + (pad + 2) × scale, which leaves room
  under the last line's descenders. The box grows upwards from its fixed bottom.
- Box: `Ui_DrawPrimColorRect` in black at alpha 0xA0. Text: white env colour.
- Wrapping (`Subtitles_Wrap`) measures each glyph's advance at the subtitle height through the font, breaks at
  spaces, at `\n`, and between CJK characters (Japanese has no spaces), and only mid-word when one word alone is wider
  than the box. Latin and Hangul (`cp < 0x2E80` or U+AC00–U+D7A3) break at spaces only. No break is allowed before
  closing punctuation (、。！？，．…」』）and ASCII `! ? . ,`), the long-vowel mark ー, or small kana (っ ゃ ゅ ょ ぁ ぃ ぅ ぇ ぉ
  and katakana equivalents). Leading spaces of a line and trailing spaces before a break are dropped.
- A character the font lacks is measured and drawn as `?`.

The constants are Zero's screen; Stadion should re-pick the box position against its own battle message box and test
at both stage resolutions.

# Rendering hooks
- `Subtitles_Draw` is called at the end of `StageLoader_SetupFrame`, after `TextRenderer_FlushQueuedStrings`, so it sits
  on top of everything the stage drew that frame, and before the profiler overlay.
- It draws with the game's own text path: `Font_BeginTranslucentTextRendering`, then `Port_Font_PrintUtf8` per line,
  then `Font_EndTexturedTextRendering`. Each glyph is an IA8 texture loaded with `gDPLoadTextureBlock` and drawn with
  `gSPTextureRectangle` (see `Port_Font_DrawCodepointScaled` in `game/src/font.c`).
- Screens set up their font state once and print with it every frame, so the draw saves and restores the font
  context fields it touches (active set `unk_50`, spacing `unk_51`, `line_height`, `line_break_flag`, texture
  `unk_4C`). Any port must do the same with its equivalent state.
- `Port_Font_SetHeight(h)` makes the following prints use glyph height `h` instead of the active font set's height;
  `Port_Font_SetHeight(0)` returns to normal. Subtitles use it because they lay their text out themselves.
- No ImGui: the box is part of the game frame, so it scales and letterboxes with it.

# Font handling (pluggable)
The subtitle code needs exactly three things from the font, and nothing else:

| Need | Plethron call |
| --- | --- |
| Advance width of a code point at a height (for wrapping) | `Port_Font_Glyph(cp, height, &tex_w, &draw_w, &advance)` |
| Draw a UTF-8 line at x, y at that height | `Port_Font_SetHeight(h)` + `Port_Font_PrintUtf8(x, y, utf8)` |
| UTF-8 decoding | `Port_Utf8Next(&p)` (= `hu_utf8_next`) |

In Plethron these sit on `hippodrome/text_font.h`: Unifont (`assets/font/unifont_subset.hex`) by default, Pop
(`pop_subset.ttf`) when the "Text font" setting picks it, each glyph built as IA8 in the games' style (white fill, one
pixel dark outline from 16 px up; `gPort.SmallTextOutline` picks the style below that) and cached per (code point,
height, style).

**Stadion's planned difference:** Stadium 1 will load its own font from the ROM and use that for subtitles instead of
Unifont. Nothing in the timing, matching or wrapping depends on which face is used; only the three calls above change
backend. For a ROM font backend:

- `Glyph` returns the ROM glyph's texture and advance at the requested height (scaled if the ROM font has one fixed
  height), or NULL for characters the ROM font lacks.
- Decide the fallback for missing characters. Plethron draws `?`; for languages whose characters the ROM font cannot
  cover (Korean, Arabic, Hindi, kanji in a kana-only ROM font) falling back to Unifont per glyph, as Plethron does for
  Pop, keeps those languages readable. That choice is Stadion's.
- Keep the texture format the draw path expects (IA8 in Plethron), or convert in the backend.
- The ROM font comes from the user's ROM at run time, so nothing ROM-derived is committed or bundled.

Because the backend is a port choice, the font hook stays in each port and is not built into this library.

# Toggle and settings
- Options row "Subtitles" (text id `PORT_OPTIONS_SUBTITLES`, values from the game's own On/Off strings), shown only in
  translated languages, in `cartridge_check_options.c`.
- Stored as the libultraship CVar `gPort.Subtitles` (default on) through `Port_Subtitles_Enabled` /
  `Port_Subtitles_SetEnabled`, not in the N64 save, so the save stays as the cartridge writes it.

# Porting checklist for Stadion
1. Find the call where announcer lines start and what identifies a line. Stadium 1 plays the announcer through its
   sample streams (`AudioStream_Start(channel, rom_offset, volume, delay)` in `game/src/audio_stream.c`, GSM-encoded)
   rather than music-player effects, so the natural key is the clip's ROM offset or the index the game picked it from,
   not an effect id. Which channel and index carry the announcer is not verified yet.
2. Use `AudioStream_IsPlaying(channel)` (or the equivalent) as the "still playing" query.
3. Restored content mixes ROMs (for example Japanese announcer lines on a Western ROM): key rows by the source ROM as
   well as the line, for example `SUBTITLE_ANNOUNCER_<region>_<key>`, since the same index can be a different line in
   another dub.
4. Transcribe from the audio (and check with speech recognition), never copy ROM text; translations follow
   `TRANSLATION.md`.
5. Re-place the box against Stadium 1's battle message box; keep the hold time, wrapping rules and save/restore of font
   state.
6. Plug in the ROM font backend for the three font calls.
7. Add the Options toggle stored as a CVar, and a `PORT_SUBTITLE_TEST` style debug aid for headless checks.
