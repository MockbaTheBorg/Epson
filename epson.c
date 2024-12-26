// Epson printer emulation
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

// Define the input and output files as global variables
#define AUTO_LF 0

FILE *fi;
FILE *fo;

char pagename[256];
int page = 1;

// Printer specific functions
#include "charset.h"
#include "epson.h"

// The program reads one file from the command line and generates a svg file.
// If the output file is not specified, the output is written to stdout.

// Main program
int main(int argc, char *argv[]) {
    char *filename;
    char *outname;

    // Check arguments
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename> [page|<output>]\n", argv[0]);
        return 1;
    }

    // Open input file
    filename = argv[1];
    fi = fopen(filename, "rb");
    if (fi == NULL) {
        fprintf(stderr, "Error opening file %s\n", filename);
        return 1;
    }

    // Open output file or stdout
    if (argc > 2) {
        outname = argv[2];

        // if the outname is 'page' rename the file to page<number>.svg
        if (strcmp(outname, "page") == 0) {
            sprintf(pagename, "page_%03d.svg", page);
            page++;
        } else {
            strcpy(pagename, outname);
        }
        fo = fopen(pagename, "w");
        if (fo == NULL) {
            fprintf(stderr, "Error opening file %s\n", pagename);
            return 1;
        }
    } else {
        fo = stdout;
    }

    // Initialize the printer (needs to be executed before anything else)
    printer_init();

    // Read the input file character by character
    file_output(fo, svg_header, page_height, page_width);
    int c;
    while (1) {
        c = file_get_char(fi);
        if (c == EOF)
            break;
        if (printer_process_char(c))
            break;
    }
    print_stderr("\nEnd of file.\n");
    file_output(fo, svg_footer);

    // Close files
    fclose(fi);
    if (fo != stdout) {
        fclose(fo);
    }

    return 0;
}
