# Epson - Relatively good Epson LX-80 emulator for RunCPM files.

This is a simple LX-80 emulator which can read a text file generated by RunCPM LST.TXT (LST: device text output, generated at A: user 0).
It is by no means a full fledged LX-80 printer, but it is capable of text and graphics.
The emulator can read the printer output files and write SVG files with the printed dos information, one SVG file per page.
It can also be used for other emulators which generate a text file.

## Building / Usage

To build just run 'gcc epson.c -o epson'.
To run it:
Option 1: epson file.txt > output.svg - Will generate the output inside output.svg.
Option 2: epson file.txt print.svg - Will generate the output inside the print.svg file.
Option 3: epson file.txt page - Will generate page_xxx.svg, one file per page generated.

