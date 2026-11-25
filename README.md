# mpqcli

A small command-line utility for inspecting and modifying MPQ (Mo'PaQ) archives used by World of Warcraft (and other Blizzard games).

## Why do I exist

MPQ archives are the container format for WoW game data (DBC, M2 models, textures, etc.). The only robust library for reading/writing them is StormLib, which is a C/C++ library with no standalone CLI. Existing tools are either GUI-only (MPQEditor) or read-only (mpyq). When you need to script extraction or modification of MPQ contents in a headless/CI/container environment, there's no ready-made tool — so this wraps StormLib in a simple CLI.

## Commands

- `list <archive>` — list files
- `has <archive> <path>` — check if a file exists
- `get <archive> <path> <out>` — extract a file
- `add <archive> <src> <dest>` — add a file
- `remove <archive> <path>` — remove a file
- `create <archive>` — create a new archive
- `verify <archive>` — verify archive integrity

## Build

```sh
g++ -O2 -std=c++17 -I<stormlib-include> src/mpqcli.cpp \
  -L<stormlib-lib> -lstorm -lz -lbz2 -o mpqcli
```

Requires StormLib (https://github.com/ladislav-zezula/StormLib).