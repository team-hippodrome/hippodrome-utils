# Workflow rules that cost us time

[Back to the porting notes](README.md)

These are the project owner's rules for the port repositories, with the incident behind each one.

## Every merge must reach main

The owner builds from `main`. In plethron, PRs #25-#40 were merged into each other's `claude/*` base branches, so
`main` stopped at #22/#24 and a Windows crash fix never reached it ("the crash is back"). An integration PR (#46) had
to merge four branch tips into `main`.

- Branch independent fixes from `main` (or from the current hub branch, if one is open). No long PR stacks.
- If a PR's base merges first, retarget the PR to `main`. A stacked PR merged after its base lands on a dead branch.
- Before saying a fix is available: `git merge-base --is-ancestor <sha> origin/main`.
- Say in every PR what it is based on and what depends on it.
- Update stacked branches by rebase or by merging the base, not by force-pushing someone else's branch.

## Merges: watch the generated files and the submodule pin

- `git add -A` during a merge stages whatever submodule commit is checked out and can silently revert the
  libultraship pin. Check `git diff --cached` for the gitlink.
- Generated tables (plethron's `zero_lift_data.c`): rerun the generator after the merge, never pick a side.
- Generated indexes (`stadion_bugfix_ref/README.md`): rerun the index script. Plethron's #46 merge silently reverted
  21 status lines.
- After merging two branch lines, rerun **every** route, not only the PR's own. Plethron#39's lifter change and the
  tournament work met for the first time in a merge, and nobody ran the tournament routes until they crashed.

## libultraship changes: show the picture first

libultraship is a separate repository (the team fork, a submodule). Changes there are allowed, but before pushing,
show the owner a side-by-side of **old port | fixed port | ares**, plus stadion when it is affected (all ports share
the fork). The owner decides whether the fix looks worse than the bug. The RDP noise fix matched ares and was still
rejected because it looked worse.

- Follow libultraship upstream style there (`.clang-format`, `.clang-tidy`), not the decomp style guide. No `PORT:`
  tags and no `getenv` debug knobs in libultraship code.
- The port pins the fork's latest `main`; bump it with `git submodule update --remote libultraship` and commit the pin.
- Pushing to the fork needs that repository to be added to the session.

## Fixes go in the port, not the decomp

Port fixes stay in the port repository. Never edit the decomp repository for port work; a tool syncs annotations
between the two. If a struct or header is also wrong on the N64, raise it with the decomp side instead.

## Preservation first

The port keeps the original behaviour and screens (loading screens included). Enhancements are options, off by
default. When a difference from ares turns out to be original behaviour, or an emulated N64 quirk (stack garbage, load
times) would change what players see, ask before emulating or removing it.

## Commands for the owner

The owner builds on Windows (PowerShell, llvm-mingw, vcpkg, a path with a space in it). Every command block sent to
them must be safe to rerun from any state: `git fetch` then `git checkout -B <branch> origin/<branch>`,
`if (Test-Path ...)`. Quote paths. Cloud agents never get access to the owner's computer, so don't ask for it; test in
the container (including the Windows cross-build) instead.
