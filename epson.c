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

// The program reads one file from the command line and generates a pdf file.
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

        // if the outname is 'page' rename the file to page<number>.pdf
        if (strcmp(outname, "page") == 0) {
            sprintf(pagename, "page_%03d.pdf", page);
            page++;
        } else {
            strcpy(pagename, outname);
        }
        // open for binary write because we're producing a PDF
        fo = fopen(pagename, "wb");
        if (fo == NULL) {
            fprintf(stderr, "Error opening file %s\n", pagename);
            return 1;
        }
    } else {
        fo = stdout;
    }

    // Initialize the PDF buffer and the printer (needs to be executed before anything else)
    pdf_init();
    printer_init();

    // Read the input file character by character and produce PDF content in memory
    int c;
    while (1) {
        c = file_get_char(fi);
        if (c == EOF)
            break;
        if (printer_process_char(c))
            break;
    }
    print_stderr("\nEnd of file.\n");

    // Write the generated PDF to the requested output. PDFs require seekable output
    // for the xref table, so when the user requested stdout we write to a temporary
    // seekable file and then stream it to stdout.
    if (fo == stdout) {
        FILE *tmp = tmpfile();
        if (tmp == NULL) {
            fprintf(stderr, "Error: cannot create temporary file for PDF output\n");
            fclose(fi);
            return 1;
        }
        pdf_write(tmp);
        rewind(tmp);
        char buf[8192];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), tmp)) > 0) {
            if (fwrite(buf, 1, n, stdout) != n) {
                fprintf(stderr, "Error writing PDF to stdout\n");
                break;
            }
        }
        fclose(tmp);
    } else {
        pdf_write(fo);
    }

    // Close files
    fclose(fi);
    if (fo != stdout) {
        fclose(fo);
    }

    return 0;
}
