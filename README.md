# steelcheck

A command-line program, written in C23, that computes the design resistances of rolled W and HP
steel cross sections according to ABNT NBR 8800:2024. AISC 360 and Eurocode 3 are planned.
Sections are read from a CSV database, and results will be written as text, HTML or PDF
reports showing the main equations.

**Status:** step 0, project skeleton (`--help`, `--version`, test framework).

## Requirements

- A C23 compiler, used in `-std=c23` mode:
  - Clang 19 or newer, or GCC 15 or newer, for all features;
  - GCC 14 works, except for the built-in section database (planned; it needs `#embed`).
- GNU Make.
- Linux, or Windows with [MSYS2](https://www.msys2.org/) (build from the **UCRT64** shell).

## Building and testing

```sh
make                      # debug build: build/debug/steelcheck and build/debug/test_runner
make test                 # build and run the tests
make BUILD=release        # optimized build in build/release/
make BUILD=san test       # AddressSanitizer + UBSan (Linux only)
make CC=clang             # choose the compiler
make WERROR=1             # treat warnings as errors
make clean                # remove build/
```

Then run `build/debug/steelcheck --help`.

## Section data

`data/gerdau_w_hp.csv` is transcribed from Gerdau, *Perfil Estrutural – Informações Técnicas*,
table "Tabela de bitolas – perfis estruturais W e HP", edition 08/25. Values are in the units
of the catalog (see the column names) with `.` as the decimal separator.

## Disclaimer

This is an educational project, not certified structural design software. Check every result
independently before using it in real design work.
