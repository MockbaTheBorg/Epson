# Epson LX / IBM 1403 printer emulators

This repository contains two related small command-line emulators that convert classic dot-matrix printer output (Epson/LX-style) into modern PDFs:

- `epson` — an Epson/LX-style emulator producing PDF output with options for tractor edges, guide bands, wide carriage, etc.
- `1403` — a related hammer/printer emulator (for IBM 1403-style output) with mostly compatible output but its own options and quirks.

Both programs aim to preserve the look and feel of vintage printed output: dots are rendered as round vector circles, form-feeds become PDF pages, and optional visual extras (tractor edges, guide bands) are available.

## Highlights

- Multi-page PDF output (form-feed characters create new pages).
- Dots drawn as filled circles for a faithful appearance.
- Optional perforated tractor edges with holes and micro-perforation dots (`-e` / `--edge`).
- Optional guide bands (soft green or blue) to show line spacing (`-g` / `--guides`, `-b` / `--blue`).
- Wide carriage support for legal/continuous paper (`-w` / `--wide`).
- A `-v` / `--vintage` mode (1403 only) that emulates a worn ribbon: deterministic per-character micro-misalignment plus repeatable per-column intensity variations (fainter columns).
- Safe stdout behavior: if stdout is a TTY the tool writes `out.pdf` by default and prints a warning. Use `-o` to explicitly choose where to write.

## Building

You can compile either emulator with a simple gcc command. Two example targets are provided below.

Build `epson` (Epson/LX emulator):

```bash
gcc -fdiagnostics-color=always -g -o epson epson.c
```

Build `1403` (1403 hammer-emulator):

```bash
gcc -fdiagnostics-color=always -g -o 1403 1403.c
```

Note: The codebase currently contains shared headers that implement small PDF helpers directly in headers. If you compile both `epson.c` and `1403.c` into a single executable, be careful to avoid duplicate symbol/linking issues — either compile each emulator separately or refactor `pdf.h` into `pdf.c` + `pdf.h` to produce a single shared object.

## Usage — shared options (both emulators)

Common options (short and long forms are supported where shown):

- `-a`, `--autocr`      Automatically carriage return after line feed.
- `-e`, `--edge`        Draw perforated tractor edges (0.5 in per side).
- `-g`, `--guides`      Draw guide bands (soft green by default).
- `-1`, `--single`      When used with guides: single-line bands (1 line high) instead of the default band height.
- `-b`, `--blue`        Draw guide bands in blue (overrides default green).
- `-o`, `--output F`    Write PDF to file `F`. If omitted and stdout is a TTY, the tool writes `out.pdf`.
- `-w`, `--wide`        Use wide/legal printable carriage (13.875 in printable).
- `-s`, `--stdin`       Read input from stdin (takes precedence over a filename argument).
- `-r`, `--wrap`        Wrap long lines to the next line instead of discarding characters.
- `-d`, `--debug`       Enable debug messages on stderr.
- `-v`, `--vintage`     Emulate a worn printer head (applies per-emulator effects; see 1403-specific notes below).
- `-h`, `--help`        Show help and exit.

Usage example:

```bash
# Convert a file and write to a named PDF
./epson -e -g -o output.pdf input.txt

# Read from stdin and write to a file
cat input.txt | ./epson -s -o out.pdf
```

## 1403-specific notes (hammer printer emulator)

The `1403` emulator provides additional options beyond the shared set:

- `-f`, `--font F`      Specify a custom font file to embed (default: `Printer.ttf`). The emulator will search for the font relative to the executable directory first, then fall back to the specified path.

### Vintage mode (both emulators)

Both emulators support an optional `--vintage` / `-v` mode:

**1403 vintage mode** simulates two visual artifacts typical of a worn-out ribbon/printer:

- Per-character misalignment: a deterministic small x/y offset is applied to a subset of characters (the same character is always misaligned the same way). This emulates a particular hammer impact or ink inconsistency for that glyph.
- Per-column intensity variation: each character column gets a repeatable intensity multiplier making some columns slightly fainter (simulating varied hammer force or ribbon wear across the width of the carriage).

**Epson vintage mode** simulates wear and mechanical degradation in the dot-matrix printer head:

- Needle misalignment: Over time, individual needles in the print head can become slightly misaligned (bent or worn), causing dots to be displaced vertically or horizontally from their expected positions. In vintage mode, a deterministic per-needle offset is applied, creating a characteristic "wobbly" appearance where dots from the same column are not perfectly vertical.
- Per-column intensity variation: Similar to the 1403, each column of dots gets a repeatable intensity modifier, simulating uneven wear of the needles or ink ribbon across the print head width.

Enable vintage mode with:

```bash
./1403 -v -e -g -o test_vintage.pdf input.txt
./epson -v -e -g -o test_vintage_epson.pdf input.txt
```

Notes about `--vintage` behavior

- The pattern is repeatable — the code currently uses a fixed RNG seed so multiple runs produce the same result (useful for reproducible output).
- Misalignment and intensity amounts are conservative by default.

## Debugging and development notes

- There is a small `make.sh` / `clean.sh` in the repository; use them to build or clean.

## Examples

- Generate an Epson-style PDF with edges and green banding:

```bash
./epson -e -g -o test_epson.pdf input.txt
```

- Generate a 1403-style PDF with vintage effects (worn ribbon emulation):

```bash
./1403 -v -e -g -o test_vintage.pdf input.txt
```

## License & credits

See the source headers for licensing information and attribution.
