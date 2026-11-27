# ares as the reference, and running the port headless

[Back to the porting notes](README.md)

Everything here was done in Ubuntu 24.04 cloud containers with no GPU and no display. It works the same on a desktop
Linux machine. The tools named here are plethron's (`tools/ares_ref`, `tools/screen_diff`); stadion has its own
`tools/ares_ref` and `tools/tour.py`. Diaulos should start from plethron's copies, which are the newest, and change the
game-specific parts (hook addresses, sync events, routes).

Contents:

1. [Build ares_ref](#1-build-ares_ref)
2. [Drive ares with a script](#2-drive-ares-with-a-script)
3. [Build and run the port headless](#3-build-and-run-the-port-headless)
4. [Screen diff: the same input on both, pictures compared](#4-screen-diff-the-same-input-on-both-pictures-compared)
5. [Battles: seeding, sync points, phase logs](#5-battles-seeding-sync-points-phase-logs)
6. [Sound: recordings and call logs](#6-sound-recordings-and-call-logs)
7. [Looking inside: peeks, gdb, other emulators](#7-looking-inside-peeks-gdb-other-emulators)
8. [Windows builds from a Linux container](#8-windows-builds-from-a-linux-container)
9. [Container survival tips](#9-container-survival-tips)

## 1. Build ares_ref

`ares_ref` is a small headless driver around ares's N64 core (cycle-accurate, paraLLEl-RDP) and its Game Boy core
(for the Transfer Pak). It plays scripted input and saves pictures, save states, RAM, audio and logs.

```bash
sudo apt install mesa-vulkan-drivers libvulkan-dev \
    libgtk-3-dev libsdl2-dev libao-dev libopenal-dev libudev-dev libxv-dev   # ares's CMake wants all of these
tools/ares_ref/build.sh build/ares      # ~10 minutes; safe to rerun
```

- `build.sh` clones ares at a pinned commit, applies `ares-host-hooks.patch` (CPU host hooks: lets the driver run
  code at given PCs, which seeding and logging use), adds the `ares_ref` target and builds the `n64` and `gb` cores.
  Pin the commit: a moving ares changes reference pictures under you.
- paraLLEl-RDP needs Vulkan. With no GPU, Mesa's lavapipe (`mesa-vulkan-drivers`) runs it at about 50 frames per
  second, 20-30 when other jobs share the CPU. On macOS it loads MoltenVK from an installed ares.app.
- The N64 PIF firmware comes from the ares source tree (`mia/Firmware/Nintendo 64`); `ref.sh` sets it.

## 2. Drive ares with a script

```bash
export ROM=/path/to/game.z64 ARES_REF_BUILD=build/ares/build ARES_REF_SRC=build/ares/ares
tools/ares_ref/ref.sh my.script [state.bin]
```

Script lines count emulated VI frames from power-on (or from the loaded state):

```
hold 1200 1205 START        # hold buttons for frames 1200..1205 (A B Z START L R UP DOWN LEFT RIGHT CU CD CL CR, joined with +)
tap 1500 A                  # 2-frame press
every 30 shots/             # a picture every 30 frames: finding a moment (ref.sh also makes a contact sheet)
shot 1920 menu.ppm          # one picture (ref.sh converts it to .png)
window 1900 2040 win/       # every frame in a range
save 1920 menu.state        # a save state: start the next exploration from here with ref.sh <script> menu.state
dump 1920 rdram.bin         # 8 MB RDRAM, big-endian
sync 2000 cmd               # wait here until the game reaches an event (section 5)
end 2100
```

Environment for the run:

| variable | |
|---|---|
| `ARES_REF_SRAM`, `ARES_REF_SRAM_OUT` | N64 save in / out (N64 byte order, the same bytes as the port's save). For a formatted blank save, give 32 KiB of `0xFF` in and keep the output |
| `ARES_REF_GBROM`, `ARES_REF_GBSAV`, `ARES_REF_GBSAV_OUT`, `ARES_REF_GBBOARD` | a Game Boy cart in the Transfer Pak |
| `ARES_REF_WAV` | the N64 audio output as raw 48 kHz s16 stereo |
| `ARES_REF_XCHECK_RNG`, `ARES_REF_XCHECK` | seed the battle's random numbers like the port (section 5) |
| `ARES_REF_PHASELOG`, `ARES_REF_SOUNDLOG`, `ARES_REF_SYNC_LOG` | battle state changes, sound calls, sync points |
| `ARES_REF_PEEK`, `ARES_REF_PEEK_AT`, `ARES_REF_PEEK_CHANGED` | read memory at function entries (section 7) |
| `ARES_REF_WEAK_COM` | the COM's Pokémon at 1 HP whenever 1P picks a command, to reach late tournament rounds fast |

Things that bit us:

- nall rewrites path arguments that contain spaces: `ref.sh` passes the ROM through a symlink.
- ares shows 640x480 pictures (its VI filter and dither on top of a 320x240 frame buffer). A pixel-exact match is not
  possible; the diff blurs slightly (radius 1), which leaves a floor around 5-7.
- Writing a route: save a state at the menu you care about, explore with `every`, then replay from power-on and read
  the labelled thumbnails to place checkpoints. Menus eat presses that come less than ~25 frames apart or land in
  a transition.
- Hooks placed in battle code stayed active when the menus reused that memory, and broke the menus (white screen).
  The driver now drops hooks when another fragment is mapped over them. Diaulos will have the same overlay reuse.

## 3. Build and run the port headless

The container recipe (Ubuntu 24.04 has no SDL3 package):

```bash
sudo apt install clang lld cmake ninja-build python3 libgl-dev libgl1-mesa-dev libgles-dev libopengl-dev \
    libxtst-dev libzip-dev zipcmp zipmerge ziptool libbz2-dev zlib1g-dev nlohmann-json3-dev libtinyxml2-dev \
    libspdlog-dev xvfb
# SDL3 from source. Install the GL dev packages FIRST, or SDL3 builds without desktop GL and ImGui's GLSL 120 fails.
git clone --depth 1 -b release-3.2.24 https://github.com/libsdl-org/SDL.git /tmp/SDL
cmake -S /tmp/SDL -B /tmp/SDL/build -G Ninja -DCMAKE_BUILD_TYPE=Release && sudo ninja -C /tmp/SDL/build install
git submodule update --init --recursive
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLUS_ENABLE_VULKAN=OFF \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build
```

- `-DLUS_ENABLE_VULKAN=OFF`: Ubuntu's glslang CMake config is broken.
- libultraship downloads `stb_image.h` from raw.githubusercontent.com, which the cloud proxy blocked. Clone
  `nothings/stb`, check out `0bc88af4` and copy `stb_image.h` to `build/_deps/stb/` before configuring.
- Build with clang. Ubuntu's gcc Release build aborted under `_FORTIFY_SOURCE`.
- Assets in `build/` are copied only when the executable relinks: copy edited asset files by hand.

Running it:

```bash
Xvfb :99 -screen 0 1280x1024x24 &
export DISPLAY=:99 SDL_AUDIO_DRIVER=dummy XDG_RUNTIME_DIR=/tmp
PORT_SCREENSHOT_DIR=out PORT_SCREENSHOT_ALL=1 PORT_QUIT_AT=5200 build/plethron game.z64
```

Plethron's useful switches (`getenv` in `src/port`; diaulos will inherit most from stadion's port layer):

| switch | |
|---|---|
| `PORT_SPEED=max`, `PORT_SPEED=1` | as fast as possible (default for tests; ~130 fps under Xvfb) or real time |
| `PORT_QUIT_AT`, `PORT_STOP_AT`, `PORT_SHOT_AT`, `PORT_SCREENSHOT_DIR`, `PORT_SCREENSHOT_ALL` | stop and take pictures at frames |
| `PORT_RESOLUTION` (`native`, `2x`, `window`), `PORT_ASPECT=16:9` | picture size and widescreen |
| `PORT_REPLAY`, `PORT_RECORD`, `PORT_AUTOSTART`, `PORT_AUTOA`, `PORT_SKIP_OPENING` | scripted input |
| `PORT_SRAM_FILE`, `PORT_GB_ROM`, `PORT_GB_SAVE`, `PORT_GB_INPUT` | saves and Transfer Pak; `PORT_GB_INPUT="gbframe:A+Down ..."` presses Game Boy buttons by GB frame |
| `PORT_SOUND=1`, `PORT_AUDIO_DUMP=<file>`, `PORT_SOUNDLOG=<file>` | sound on at max speed, recording, call log |
| `PORT_XCHECK_RNG`, `PORT_SYNC`, `PORT_PHASELOG`, `PORT_GURANDOM_LOG` | the port side of section 5 |
| `PORT_FREEZE_CHECK`, `PORT_EXIT_ON_FREEZE` | stop a run that hangs instead of waiting for a timeout |
| `PORT_TRACE_LIFT`, `PORT_LIFT_AUDIT`, `PORT_TEXLOG`, `PORT_DRAWLOG`, `PORT_TRACE_FRAGMENTS`, `PORT_TRACE_GB` | traces |
| `PORT_INTERPOLATE`, `PORT_INTERP_IMMEDIATE`, `PORT_RENDER_EVERY=1` | frame interpolation tests |

ASan build: `cmake -B build_asan -DPORT_ASAN=ON` (apt `libclang-rt-18-dev`), run with
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and grep the log for `ERROR: AddressSanitizer`. Run every route on it
after a large change.

## 4. Screen diff: the same input on both, pictures compared

`tools/screen_diff` (plethron#26 onward; read its `README.md`) plays a route on the port and on ares and ranks the
checkpoints by how different the port's picture is.

```bash
pip install numpy pillow
python3 tools/screen_diff/run_all.py out/screens                     # every route
python3 tools/screen_diff/run.py routes/pokedex.route out/dex         # one route, both sides
python3 tools/screen_diff/run.py routes/pokedex.route out/dex --port-only   # after a fix: keep ares's pictures
python3 tools/screen_diff/compare.py out/screens/report out/screens/*/
```

The report lists checkpoints worst first with a score (mean RGB difference at 320x240 after a slight blur) and a
`port | ares | difference` picture per checkpoint.

How the timing problem was solved (this is the part to copy):

- The port skips the N64 boot (~165 frames ahead at the first picture), runs the intro a few percent faster and
  loads faster. So checkpoints before the first press shift by `@boot_offset`, and ares keeps every frame from 12
  before to 120 after each checkpoint (`@window`). `compare.py` scores against the closest one and reports the
  offset it picked: a large offset is a timing difference, not a rendering one.
- Presses land on the same frame on both sides and the menus wait for them, which lines the two up again.
- For battles use sync points (section 5), never fixed frame counts. ares falls behind in busy effects.
- Scoring with lighting differences: `compare.py` also reports a "lit" score with a smooth difference field removed,
  so a slightly brighter floor does not hide a wrong effect.

Route tokens: buttons, `hold:A:30`, `press:UP:60`, waits in frames, `shot:<label>`, `sync:<event>`; headers `@sram`,
`@gb`, `@window`, `@boot_offset`, `@rng`, `@weak_com`. Saves are crafted by scripts (`jp_gen1_save.py`,
`zero_sram_options.py`, `move_coverage/zero_teams.py`) so no save file is ever committed.

For every frame of a sequence (a wipe, a fade): `@window 0 0` plus one `1 shot:fNNNNN` per frame. A full battle is
~8,000 frames and ~8 GB, so cut routes short.

## 5. Battles: seeding, sync points, phase logs

Both Stadium games time their random numbers with the CPU counter. Same input, different battle.

- **`@rng <seed>`** makes the battle's counter reads (`Battle_Random`, the COM's `BattleAi_StirRandom`) return a
  seeded stream on both sides and restarts `guRandom` (particles) at each battle start and each move:
  `PORT_XCHECK_RNG` in the port (`src/port/xcheck.cpp`), `ARES_REF_XCHECK_RNG` in ares (host hooks). The values drawn
  go to `port_rng.txt` and `ares_rng.txt`; run.py says whether they agree. Reseed **before** any per-action random
  draw (the random intro camera drew before the move reseed and picked different shots), and reset per-frame
  counters with it (`gParticleFrameCounter` drifted 9 frames between sides and shifted every scrolling texture).
- **Sync points** (`sync:cmd`, `sync:move`, `sync:anim1`): each side waits until its game reaches the event, then
  keeps counting. Anchor move effects on `anim1` (the move animation start); `cmd` drifts ~34 frames because ares's
  menu phases run longer.
- **Phase logs** (`PORT_PHASELOG`, `ARES_REF_PHASELOG`, `tools/screen_diff/phase_diff.py`) list battle states whose
  length differs between the two. That is how the battle pacing bug (class 4, plethron#52) was found: the camera was
  right, the idle animation's phase drifted because a table was read at the wrong width.
- **guRandom call logs** (`PORT_GURANDOM_LOG`, ares peeks) compare particle random streams frame by frame.
- Weak COM (`@weak_com`) ends each round in one hit, to reach a Cup's later rounds and its ending.
- Battle planner (plethron `tools/move_coverage`): fifos that let a script answer the port's command prompts, used to
  play every species with every legal move (1,117 pairs) against ares.

## 6. Sound: recordings and call logs

- Record: `run.py <route> <dir> --port-only --port-env PORT_SOUND=1 --port-env PORT_AUDIO_DUMP=<dir>/port.pcm`
  (one `--port-env` per pair) and `ARES_REF_WAV=<dir>/ares.pcm run.py <route> <dir> --ares-only`.
- Compare per scene, aligned on the loudness envelope: level in dB, long-term spectrum correlation, chroma, onset
  counts and spectrograms (`acmp.py`, `flux.py` in the shared `plethron/audio-note-offsets/`). Spectral flatness near
  0.7 means noise, near 0.15 music: a quick check that audio is not garbage.
- Call logs settle "is a sound missing?" without listening: `PORT_SOUNDLOG` and `ARES_REF_SOUNDLOG` log every cry and
  sound command with its frame. `diff <(cut -d' ' -f3- port_sound.txt) <(cut -d' ' -f3-5 ares_sound.txt)`. With
  `@rng` the two lists hold the same calls in the same order.

## 7. Looking inside: peeks, gdb, other emulators

- **ares peeks**: `ARES_REF_PEEK=<out> ARES_REF_PEEK_AT="<func addr>@<sp offset|abs addr|a0x28>,..."` logs words at a
  function's entry (stack slots, globals, fields of the `a0` argument). `ARES_REF_PEEK_CHANGED=1` finds the first
  function entry that sees a word change (hook every function from the decomp's symbol list). This is how
  uninitialised stack values were read (class 12).
- **"Who changed this?"** In ares hook a function and log `ra`; in the port use `__builtin_return_address`.
- **gdb on the port**: lifted globals show as `u8[]`, so cast them. The title's attract demo battle (frames
  ~2800-5000, no input) is a free battle repro: break in the setup function and change the species or move
  (`plethron/dig-softlock/dig.gdb` in the shared files). A `PORT_BIN` wrapper that runs `gdb -batch` works with run.py.
- **Reading N64 return values or code**: build the decomp (`make init && make rom`) and
  `mips-linux-gnu-objdump -d --disassemble=<function> build/.../*.elf`.
- **pj64-pokestadium-debug** (`team-hippodrome/pj64-pokestadium-debug`): Project64 with a socket API for agents
  (breakpoints, write watchpoints, memory writes, save states, Transfer Pak). Good for forcing game states (stub a
  function, force a dice roll) when naming things in the decomp. Its GLideN64 is not a pixel reference: use ares for
  pictures.
- **mupen64plus** (apt, `--gfx mupen64plus-video-rice --testshots`): a quick second opinion when ares is not built yet.

## 8. Windows builds from a Linux container

Possible, slow to set up, and worth it for Windows-only bugs (class 11):

- llvm-mingw release tarball from GitHub; vcpkg `x64-mingw-static` with the port's overlay triplets.
- vcpkg downloads hit the proxy: set `X_VCPKG_ASSET_SOURCES="clear;x-script,<script> {url} {sha512} {dst};x-block-origin"`.
  The script (`vcpkg_fetch_asset.sh` in the shared `plethron/tools/`) turns GitHub archive URLs into `git clone` +
  `git -c tar.tar.gz.command="gzip -cn" archive` (reproduces GitHub's sha512), commit `.patch` URLs into
  `git format-patch --full-index` (and rewrites the portfile's SHA512, so rerun `vcpkg install` until it passes), and
  fetches bzip2 from archive.ubuntu.com. `mkdir -p` the destination first.
- Wine 9 (`apt wine64`): D3D11 fails, so write `{"Window":{"Backend":{"Id":2,"Name":"OpenGL"}}}` into the config
  first. `wine_port_bin.sh` (shared `plethron/tools/`) wraps the exe as a `PORT_BIN` for run.py and converts paths to
  `Z:\`. Wine + Mesa pictures came out byte-identical to the Linux build. Some containers' Wine crashes on everything;
  then a cross-build that links is the most you can check.

## 9. Container survival tips

- **Disk**: one full screen-diff run is ~4.5 GB of ares frames; all routes plus two Cup runs fill ~40 GB. Delete
  `<out>/<route>/ares` after comparing. Reuse ares frames for a second port run with hard links
  (`cp -al before after`, then `--port-only`). Don't name a run directory the same as the `PORT_BIN` path.
- **Long runs**: start them detached (`setsid nohup ...`) and keep a tracked background waiter so the container stays
  awake. Make sweeps resumable (skip finished items), because containers restart and kill Xvfb.
- **Environment variables do not persist between shell calls**: keep them in an env file and source it each time.
- **Never commit** ROMs, saves made by the game, pictures, recordings or RAM dumps. Put pictures in the project's
  shared files and link them from the PR.
