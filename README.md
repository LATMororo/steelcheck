# steelcheck

A command-line program, written in C23, that computes the design resistances of rolled W and HP
steel cross sections according to ABNT NBR 8800:2024. AISC 360 and Eurocode 3 are planned.
Sections are read from a CSV database, and results will be written as text, HTML or PDF
reports showing the main equations.

**Status:** section database and the `list` and `show` commands; design checks are next.

## Requirements

- A C23 compiler, used in `-std=c23` mode:
  - Clang 19 or newer, or GCC 15 or newer, for all features;
  - GCC 14 works, but builds the program without the built-in section database (it needs
    `#embed`); `steelcheck --version` tells whether a build has it.
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

## Usage

```sh
steelcheck list                       # every section of the built-in Gerdau catalog
steelcheck list W310                  # sections whose designation starts with W310
steelcheck show W310x38.7             # every property of one section
steelcheck show W 310 x 38,7          # spaces, lower case, decimal comma accepted
steelcheck -q list HP                 # -q hides warnings about the database

steelcheck --db my_sections.csv list  # another database (same CSV format)
export STEELCHECK_DB=my_sections.csv  # ... or as the default, instead of --db
```

The database is the `--db` file, else the `STEELCHECK_DB` file, else the built-in catalog
(`data/gerdau_w_hp.csv`, embedded in the program at compile time with C23 `#embed`).

Problems in the database are reported like compiler messages, e.g.
`<built-in>:113: warning: W610x153.0: mass_kg_m 154.2 differs from the designation's 153`.
Exit codes: 0 success, 1 runtime error (unusable database, section not found), 2 usage error.

## Section data

`data/gerdau_w_hp.csv` is transcribed from Gerdau, *Perfil Estrutural – Informações Técnicas*,
table "Tabela de bitolas – perfis estruturais W e HP", edition 08/25. Values are in the units
of the catalog (see the column names) with `.` as the decimal separator.

## Disclaimer

This is an educational project, not certified structural design software. Check every result
independently before using it in real design work.
