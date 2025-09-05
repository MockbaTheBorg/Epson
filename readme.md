# Epson - LX‑80 style dot‑matrix emulator

This project emulates many of the printable features of an Epson LX‑80 style printer and converts printer output files into a modern PDF (previously produced SVG).

It is designed to read text output produced by vintage systems (for example RunCPM LST.TXT) or any stream that contains Epson/LX control sequences and dot/graphics blocks.

Key behavior and features

- Output: generates a proper PDF file (multi‑page if the input contains form‑feed (FF) characters).
- Dots: printed pixels are rendered as filled round dots (vector circles) instead of rectangles.
- Tractor edges: optional perforated tractor strips with holes and micro‑perforation dots can be drawn with `-e` / `--edge`.
- Green guide bands: optional alternating soft green horizontal bands (0.5" high) can be drawn with `-g` / `--green`.
- Wide carriage: enable wide/legal printable carriage (13.875" printable) with `-w` / `--wide`.
- CLI options: no environment variables required — use short or long command line options (see examples below).
- FF support: form‑feed characters advance to a new PDF page so a single output PDF may contain multiple pages.
- Safe stdout behavior: when stdout is a terminal the program defaults to writing `out.pdf` (to avoid dumping binary PDF to your console); use `-o` / `--output` to pick a filename or pipe to a file.

Build

To compile:

    gcc epson.c -o epson

Usage

Basic examples:

- Write to a named PDF file:

    ./epson -o output.pdf input.txt

- Add tractor edges and green guide bands:

    ./epson -e -g -o out_edges_green.pdf input.txt

- Use wide carriage and create a PDF (with edges and green):

    ./epson -w -e -g -o out_wide.pdf input.txt

Notes

- If you omit `-o` and stdout is a terminal, the program writes to `out.pdf` and prints a warning to stderr. If you want PDF bytes on stdout (for piping) you must redirect stdout to a file or pipe and provide `-o -` if supported.
- The `-h`/`--help` option prints short usage information; `-d`/`--debug` enables brief debug messages on stderr.
- The emulator aims for visual fidelity (dot placement, tractor hole sizing/spacing, green bands) but is not a full LX‑80 implementation. If you need adjustments (dot size, hole spacing, band color or phase) say which parameter you want changed.

License and credits

See repository headers for licensing and attribution.
