// Epson printer emulation
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <unistd.h>

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options] <inputfile>\n", prog);
    fprintf(stderr, "  -e, --edge       Add perforated tractor edges (0.5in each side)\n");
    fprintf(stderr, "  -g, --green      Add green guide strips\n");
    fprintf(stderr, "  -o, --output F   Write PDF to file F (default stdout)\n");
    fprintf(stderr, "  -w, --wide       Use wide/legal carriage sizes (13.875in printable)\n");
    fprintf(stderr, "  -d, --debug      Enable debug messages\n");
    fprintf(stderr, "  -h, --help       Show this help\n");
}

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
    char *outname = NULL;
    int opt_edge = 0;
    int opt_green = 0;
    int opt_wide = 0;

    // Parse command line options
    static struct option long_options[] = {
        {"edge", no_argument, 0, 'e'},
        {"green", no_argument, 0, 'g'},
        {"output", required_argument, 0, 'o'},
        {"wide", no_argument, 0, 'w'},
        {"debug", no_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    int opt;
    int opt_index = 0;
    // getopt loop: options come before the input filename
    while ((opt = getopt_long(argc, argv, "ego:whd", long_options, &opt_index)) != -1) {
        switch (opt) {
            case 'e':
                opt_edge = 1;
                break;
            case 'g':
                opt_green = 1;
                break;
            case 'o':
                outname = strdup(optarg);
                break;
            case 'w':
                opt_wide = 1;
                break;
            case 'd':
                // enable runtime debug messages
                debug_enabled = 1;
                print_stderr("Debug enabled.\n");
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    // Check if there is a filename after the options
    if (optind < argc) {
        filename = argv[optind];
    } else {
        fprintf(stderr, "Usage: %s [options] <inputfile>\n  -e|--edge     add perforated tractor edges\n  -g|--green    add green guide strips\n  -o|--output   output filename (default stdout)\n  -w|--wide     use wide/legal carriage sizes\n", argv[0]);
        return 1;
    }

    // Open input file
    fi = fopen(filename, "rb");
    if (fi == NULL) {
        fprintf(stderr, "Error opening file %s\n", filename);
        return 1;
    }

    // Open output file or stdout
    if (outname != NULL) {
        // open for binary write because we're producing a PDF
        fo = fopen(outname, "wb");
        if (fo == NULL) {
            fprintf(stderr, "Error opening file %s\n", outname);
            return 1;
        }
    } else {
        fo = stdout;
    }

    // If writing to stdout but stdout is a terminal, avoid dumping binary PDF to the console.
    // Write to a default file 'out.pdf' instead and inform the user.
    if (fo == stdout) {
        #ifdef _POSIX_VERSION
        if (isatty(fileno(stdout))) {
            print_stderr("Stdout is a TTY — writing PDF to 'out.pdf' instead. Use -o to specify a file.\n");
            fo = fopen("out.pdf", "wb");
            if (fo == NULL) {
                print_stderr("Warning: cannot open 'out.pdf', will write to stdout\n");
                fo = stdout;
            }
        }
        #endif
    }

    if (opt_edge) {
        draw_tractor_edges = 1;
        print_stderr("Tractor edges enabled.\n");
    }

    if (opt_green) {
        draw_green_strips = 1;
        print_stderr("Green guide strips enabled.\n");
    }

    if (opt_wide) {
        /* Use wide/legal printable carriage: 13.875 inches printable (standard continuous form / legal width)
           When tractor edges are enabled the full media width will include the two 0.5in tractor strips. */
        page_width = 13.875f;
        wide_carriage = 1;
        print_stderr("Wide carriage enabled (printable %.3fin).\n", page_width);
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
