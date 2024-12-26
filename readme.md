# Epson - Relatively good Epson LX-80 emulator for RunCPM files.

This is a simple LX-80 emulator which can read a text file generated by RunCPM LST.TXT (LST: device text output, generated at A: user 0).<br>
It is by no means a full fledged LX-80 printer, but it is capable of text and graphics.<br>
The emulator can read the printer output files and write SVG files with the printed dos information, one SVG file per page.<br>
It can also be used for other emulators which generate a text file.

## Building / Usage

To build just run 'gcc epson.c -o epson'.<br>
To run it:<br>
Mode 1: epson file.txt > output.svg - Will generate the output inside output.svg.<br>
Mode 2: epson file.txt print.svg - Will generate the output inside the print.svg file.<br>
Mode 3: epson file.txt page - Will generate page_xxx.svg, one file per page generated. The argument has to be the word 'page' in lowercase.<br>

Mode 1 and Mode 2 will ignore FF (form feed) characters, and will have all sorts of issues is FF is issued. Those modes are good only for single page printings.<br>
Mode 3 will create a page_001.svg file, then page_002.svg and so on, every time a FF character is found.<br>