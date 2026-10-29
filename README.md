# steelcheck

A command-line program, written in C23, that checks rolled W and HP steel members according to
ABNT NBR 8800. AISC 360 and Eurocode 3 are planned. Sections are read from a CSV database,
members from a TOML input file, and results are written as text and HTML reports that show
every equation (PDF/LaTeX reports are planned).

**Status:** section database; `list`, `show` and `check` commands; NBR 8800 checks of tension,
compression, bending and shear about both axes, and their interaction; text and HTML reports.

The formulas and clause numbers follow **ABNT NBR 8800:2024**: 5.2 tension; 5.3 compression,
with the effective area of 5.3.4; 5.4.2 and Annex D bending; 5.4.3 shear; 5.5.1.2 interaction.
One interpretation: the effective width of 5.3.4.2 is limited to the real width b (with the
rounded c1 and c2 of Table 5 the formula gives up to about 0.2 % more just past the limit).

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

steelcheck check examples/members.toml                      # check members: text report
steelcheck --html report.html check examples/members.toml   # ... and an HTML report
```

The database is the `--db` file, else the `STEELCHECK_DB` file, else the built-in catalog
(`data/gerdau_w_hp.csv`, embedded in the program at compile time with C23 `#embed`).

Problems in the database are reported like compiler messages, e.g.
`<built-in>:113: warning: W610x153.0: mass_kg_m 154.2 differs from the designation's 153`.
Problems in an input file are reported the same way, with line and column, e.g.
`frame.toml:6:10: error: invalid number for 'length': '3,5'`.

Exit codes: 0 success, 1 runtime error (unusable database or input file, section not found),
2 usage error, 3 the check ran and at least one member fails.

## Input files

A small subset of [TOML](https://toml.io): one `[[member]]` table per member. See
[`examples/members.toml`](examples/members.toml).

```toml
code     = "NBR 8800"          # optional (the default)
material = "A572 Gr.50"        # default material of the members

[[member]]
label   = "C1"
section = "W 310 x 52.0"       # required
length  = 3.5                  # required [m]; default of Lx, Ly, Lz and Lb
Ky      = 0.8
NSd     = -600                 # [kN], compression negative
MxSd    = 80                   # [kN m]
```

| Keys | Unit | Meaning |
|------|------|---------|
| `label`, `section`, `material` | | strings in quotes |
| `length`, `Lx`, `Ly`, `Lz` | m | length; unbraced lengths for buckling about x, y and torsion |
| `Lb` | m | length between lateral bracings of the compression flange (0: braced) |
| `Kx`, `Ky`, `Kz` | | effective length factors (default 1.0) |
| `Cb` | | moment gradient factor, positive (default 1.0; see below) |
| `MA`, `MB`, `MC` | kN m | moments at 1/4, 1/2 and 3/4 of `Lb`: Cb is computed from them |
| `Mmax` | kN m | largest moment in `Lb` (optional; default: the largest of the others and `MxSd`) |
| `M1`, `M2`, `MCL` | kN m | one flange continuously restrained: moments for Cb (5.4.2.4 a)) |
| `An` | cm² | net area at the connections (tension rupture) |
| `Ct` | | reduction coefficient of the net area (needed with `An`) |
| `NSd`, `VySd`, `VxSd` | kN | axial force (tension positive), shear in the web, in the flanges |
| `MxSd`, `MySd` | kN m | bending moments about the major and the minor axis |

Materials: ASTM A36, A572 Gr.50, A588, A992; NBR 7007 MR250, AR350, AR350 COR, AR415.

Without design forces, only the resistances are computed. With them, only the checks the
forces call for are done (tension, compression, bending about x and y, shear along y and x),
plus the interaction of the axial force and the moments when at least two of them are given.
Within a check, each limit state is compared with its design force, usually the same for all
(`MxSd` for bending about x; FLT with one flange restrained is the exception, see Cb below), and
the one with the largest utilization governs. Design moments must already include second-order
effects. Not implemented: slender webs in bending (beams of Annex E), for which the bending
check is reported as not implemented; no Gerdau rolled section has one.

### The moment gradient factor Cb

Cb raises the lateral-torsional buckling resistance when the moment varies along the unbraced
length `Lb`. In NBR 8800:2024 it enters through λr and M_cr (Annex D, D.2.8 a)). Give it in one
of three ways:

- **Nothing:** Cb = 1.0, the value for a uniform moment and the lowest the formula of 5.4.2.3 a)
  gives for doubly symmetric sections. It is also the value the standard prescribes for
  cantilevers with warping restrained at the support (5.4.2.3 b)) and for the cases of
  5.4.2.3 c) and 5.4.2.4 c).
- **`MA`, `MB`, `MC`** (and optionally `Mmax`): the program computes
  Cb = 12.5 M_max / (2.5 M_max + 3 M_A + 4 M_B + 3 M_C) · R_m, with absolute values and
  R_m = 1.0 (doubly symmetric sections), and shows it in the report.
- **`M1`, `M2`, `MCL`**, when one flange has continuous lateral restraint (for example, braced
  by a slab or a deck) and the other is free, the restrained one is tensioned at one end at
  least, and the loads point to the free flange (5.4.2.4 a)). The program computes
  Cb = 3.00 − (2/3) M2/M1 − (8/3) M_CL/(M1 + M2). **Signs: negative compresses the free flange,
  positive tensions it.** M1 is the largest end moment that compresses the free flange, so it is
  negative; M2 is the moment at the other end; M_CL the moment at the middle of `Lb`. When M2
  tensions the free flange, it counts as zero in the last term. `Lb` is the unbraced length of
  the free flange. The other cases of 5.4.2.4 are fixed values: give `Cb = 2.0` for b), or
  nothing (1.0) for c). If the formula gives Cb ≤ 0, its conditions do not hold, and bending is
  reported as not checked. With design forces, FLT is checked against |M1|, the largest moment
  that compresses the free flange (5.4.2.4), and FLM and FLA against `MxSd`.
- **`Cb`**: a value you computed, for example by 5.4.2.4 (one flange with continuous lateral
  restraint) or by an elastic stability analysis (5.4.2.5). It must be positive; a value below
  1.0 is accepted with a warning, since only an analysis can give one.

NBR 8800:2024 sets **no upper limit** on Cb (the 2008 edition limited it to 3.0): 5.4.2.3 a)
reaches 5.0 when MA, MB and MC are zero, and 5.4.2.4 a) can exceed 3.00. The resistance stays
bounded anyway, by M_pl and by 1.50 W f_y (5.4.2.2).

Give only one of `Cb`, `MA`-`MB`-`MC` and `M1`-`M2`-`MCL`.

**Cb = 1.0 is not always on the safe side.** These loads and members are destabilizing, and for
them 5.4.2.3 requires Cb from an elastic stability analysis or an accepted procedure (5.4.2.5);
the program cannot tell, so check them yourself:

- gravity loads applied above the mid-height of the section, such as on the top flange, without
  lateral or torsional restraint where they act;
- cantilevers other than those of 5.4.2.3 b).

Gravity loads applied below mid-height (stabilizing) may conservatively be treated as if they
acted at mid-height, so the formula above applies to them (5.4.2.3).

The HTML report typesets the equations with [KaTeX](https://katex.org), loaded from a CDN; offline,
it shows their LaTeX source.

## Section data

`data/gerdau_w_hp.csv` is transcribed from Gerdau, *Perfil Estrutural – Informações Técnicas*,
table "Tabela de bitolas – perfis estruturais W e HP", edition 08/25. Values are in the units
of the catalog (see the column names) with `.` as the decimal separator.

## Disclaimer

This is an educational project, not certified structural design software. Check every result
independently before using it in real design work.
