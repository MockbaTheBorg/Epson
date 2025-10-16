// Epson printer emulation
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <unistd.h>

#include "epson.h"

// Global variables
int draw_tractor_edges = 0;
int draw_guide_strips = 0;
int guide_single_line = 0;
int green_blue = 0;
int wide_carriage = 0;
int debug_enabled = 0;
int vintage_enabled = 0;
float vintage_current_intensity = 1.0f;
// Per-column intensity multipliers (0..1.5) to simulate hammer force variation
float *vintage_col_intensity = NULL;
int vintage_cols = 0;
// Per-character deterministic misalignment offsets (inches)
// We'll store small x,y offsets for ASCII 32..126
float vintage_char_xoff[127];
float vintage_char_yoff[127];
float page_width = PAGE_WIDTH;
float page_height = PAGE_HEIGHT;
int page_cpi = PAGE_CPI;
int page_lpi = PAGE_LPI;
float page_xmargin = PAGE_XMARGIN;
float page_ymargin = PAGE_YMARGIN;
char **pdf_contents = NULL;
size_t *pdf_lens = NULL;
size_t *pdf_caps = NULL;
int pdf_pages = 0;

// Printer charset (9x9 bitmaps for 256 characters)
int charset[256*9];
// Input file (defined below with other globals)
// Printer state variables (defined below)
// Printer state variables (definitions)
int auto_cr = 1;
int mode_bold = 0;
int mode_italic = 0;
int mode_doublestrike = 0;
int mode_wide = 0;
int mode_wide1line = 0;
int mode_subscript = 0;
int mode_superscript = 0;
int mode_compressed = 0;
int mode_elite = 0;
int mode_underline = 0;

float xpos = PAGE_XMARGIN;
float ypos = PAGE_YMARGIN;
float step60 = 0.0;         // in
float step72 = 0.0;         // in
float xstep = 0.5;          // in
float ystep = 1.0;          // in
float lstep = 0.0;          // in
float yoffset = 0;          // in   (subscript/superscript)
int line_count = 0;

static void print_usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [options] <inputfile>\n", prog);
    fprintf(stderr, "  -e, --edge       Add perforated tractor edges (0.5in each side)\n");
    fprintf(stderr, "  -g, --guides     Add guide strips (green by default)\n");
    fprintf(stderr, "  -1, --single     Draw guide bands every 1 line instead of the default\n");
    fprintf(stderr, "  -b, --blue       Draw guide bands in blue instead of green\n");
    fprintf(stderr, "  -o, --output F   Write PDF to file F (otherwise to stdout)\n");
    fprintf(stderr, "  -w, --wide       Use wide/legal carriage sizes (13.875in printable)\n");
    fprintf(stderr, "  -s, --stdin      Read input from standard input (takes precedence)\n");
    fprintf(stderr, "  -c, --charset    Dump the printer character set to stderr and exit\n");
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
int main(int argc, char *argv[])
{
    char *filename;
    char *outname = NULL;
    int opt_edge = 0;
    int opt_guides = 0;
    int opt_single = 0;
    int opt_blue = 0;
    int opt_wide = 0;
    int opt_stdin = 0;
    int opt_charset = 0;

    // Parse command line options
    static struct option long_options[] = {
        {"edge", no_argument, 0, 'e'},
        {"guides", no_argument, 0, 'g'},
        {"single", no_argument, 0, '1'},
        {"blue", no_argument, 0, 'b'},
        {"output", required_argument, 0, 'o'},
        {"wide", no_argument, 0, 'w'},
        {"stdin", no_argument, 0, 's'},
        {"charset", no_argument, 0, 'c'},
        {"debug", no_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};
    int opt;
    int opt_index = 0;
    // getopt loop: options come before the input filename
    while ((opt = getopt_long(argc, argv, "eg1bo:wscdh", long_options, &opt_index)) != -1)
    {
        switch (opt)
        {
        case 'e':
            opt_edge = 1;
            break;
        case 'g':
            opt_guides = 1;
            break;
        case '1':
            opt_single = 1;
            break;
        case 'b':
            opt_blue = 1;
            break;
        case 'o':
            outname = strdup(optarg);
            break;
        case 'w':
            opt_wide = 1;
            break;
        case 's':
            opt_stdin = 1;
            break;
        case 'c':
            opt_charset = 1;
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

    // Lists the charset if requested and exits
    if (opt_charset) {
        dump_charset();
        return 0;
    }

    // Decide input source: stdin (-s) takes precedence over any filename supplied
    if (opt_stdin)
    {
        fi = stdin;
    }
    else
    {
        // Check if there is a filename after the options
        if (optind < argc)
        {
            filename = argv[optind];
        }
        else
        {
            print_usage(argv[0]);
            return 1;
        }

        // Open input file
        fi = fopen(filename, "rb");
        if (fi == NULL)
        {
            fprintf(stderr, "Error opening file %s\n", filename);
            return 1;
        }
    }

    // Open output file or stdout
    if (outname != NULL)
    {
        // open for binary write because we're producing a PDF
        fo = fopen(outname, "wb");
        if (fo == NULL)
        {
            fprintf(stderr, "Error opening file %s\n", outname);
            return 1;
        }
    }
    else
    {
        fo = stdout;
    }

    // If writing to stdout but stdout is a terminal, avoid dumping binary PDF to the console.
    // Write to a default file 'out.pdf' instead and inform the user.
    if (fo == stdout)
    {
#ifdef _POSIX_VERSION
        if (isatty(fileno(stdout)))
        {
            print_stderr("Stdout is a TTY — writing PDF to 'out.pdf' instead. Use -o to specify a file.\n");
            fo = fopen("out.pdf", "wb");
            if (fo == NULL)
            {
                print_stderr("Warning: cannot open 'out.pdf', will write to stdout\n");
                fo = stdout;
            }
        }
#endif
    }

    if (opt_edge)
    {
        draw_tractor_edges = 1;
        print_stderr("Tractor edges enabled.\n");
    }

    if (opt_guides)
    {
        draw_guide_strips = 1;
        print_stderr("Green guide strips enabled.\n");
        if (opt_single) {
            guide_single_line = 1;
            print_stderr("Green guide strips: single-line mode enabled.\n");
        }
        if (opt_blue) {
            green_blue = 1;
            print_stderr("Guide strips set to blue (overrides green).\n");
        }
    }

    if (opt_wide)
    {
        // Use wide carriage: 13.875in printable area
        page_width = WIDE_WIDTH;
        wide_carriage = 1;
        print_stderr("Wide carriage enabled (printable %.3fin).\n", page_width);
    }

    // Initialize the PDF buffer and the printer (needs to be executed before anything else)
    pdf_init();
    printer_init();

    // Read the input file character by character and produce PDF content in memory
    int c;
    while (1)
    {
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
    if (fo == stdout)
    {
        FILE *tmp = tmpfile();
        if (tmp == NULL)
        {
            fprintf(stderr, "Error: cannot create temporary file for PDF output\n");
            fclose(fi);
            return 1;
        }
        pdf_write(tmp);
        rewind(tmp);
        char buf[8192];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), tmp)) > 0)
        {
            if (fwrite(buf, 1, n, stdout) != n)
            {
                fprintf(stderr, "Error writing PDF to stdout\n");
                break;
            }
        }
        fclose(tmp);
    }
    else
    {
        pdf_write(fo);
    }

    // Close files
    fclose(fi);
    if (fo != stdout)
    {
        fclose(fo);
    }

    return 0;
}
