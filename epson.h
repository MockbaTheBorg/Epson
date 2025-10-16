#ifndef EPSON_H
#define EPSON_H

#include "pdf.h"

// Global program settings
#define DEBUG 0

// Printer page settings (defaults)
#define PAGE_WIDTH 8.5      // in
#define WIDE_WIDTH 13.875   // in
#define PAGE_HEIGHT 11.0    // in
#define PAGE_CPI 10         // cpi
#define PAGE_LPI 6          // lpi
#define PAGE_LINES ((int)(PAGE_HEIGHT * PAGE_LPI))  // 66 lines per page
#define PAGE_XMARGIN 0.0    // in
#define PAGE_YMARGIN 0.025  // in

// Number of columns per tab stop (default 8)
#define TAB_STOPS 8

// Printer dot
#define DOT_RADIUS 0.5      // pt
#define DOT_OPACITY 0.5     // 0.0 to 1.0

// Printer flags
extern int auto_cr;

// Printer modes
extern int mode_bold;
extern int mode_italic;
extern int mode_doublestrike;
extern int mode_wide;
extern int mode_wide1line;
extern int mode_subscript;
extern int mode_superscript;
extern int mode_compressed;
extern int mode_elite;
extern int mode_underline;

// Global printer variables
extern float page_width;
extern float page_height;
extern int page_cpi;
extern int page_lpi;
extern float page_xmargin;
extern float page_ymargin;

// Line counter for automatic pagination
extern int line_count;

// Page cursor position
extern float xpos;
extern float ypos;
extern float step60;         // in
extern float step72;         // in
extern float xstep;          // in
extern float ystep;          // in
extern float lstep;          // in
extern float yoffset;        // in   (subscript/superscript)

// Forward declarations for tractor-edge option (defined later)
extern int draw_tractor_edges;
extern int draw_green_strips;
extern int wide_carriage;
extern int debug_enabled; // forward decl for debug flag
extern int vintage_enabled;
extern float vintage_dot_misalignment[9];
void pdf_draw_tractor_edges_page(void);
void printer_reset(void);

// Define an array with the names of control characters from 0 to 31
const char *control_names[] = {
    "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
    "BS", "HT", "LF", "VT", "FF", "CR", "SO", "SI",
    "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
    "CAN", "EM", "SUB", "ESC", "FS", "GS", "RS", "US"
};

// Printer charset (9x9 bitmaps for 256 characters)
extern int charset[256*9];

// Input file
extern FILE *fi;

// Printer state variables
extern float xpos, ypos, xstep, ystep, lstep, yoffset;
extern int mode_bold, mode_italic, mode_doublestrike, mode_wide, mode_wide1line, mode_underline, mode_subscript, mode_superscript, mode_elite, mode_compressed;
extern int line_count;

// Is the printer initialized?
int epson_initialized = 0;

// Print a message to stderr (debug/info only)
void print_stderr(const char *msg, ...) {
    if (!debug_enabled) return; // silent unless debug enabled
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
}

// Print a control character to stderr
void print_control(int c) {
    // use debug printing for control output
    if (!debug_enabled) return;
    print_stderr("<%s>", control_names[c]);
    if (c == 10) {
        print_stderr("\n");
    }
}

// Print a message to the output file
void file_output(FILE *fo, const char *msg, ...) {
    va_list args;
    va_start(args, msg);
    vfprintf(fo, msg, args);
    va_end(args);
}

// Dump the charset to stdout as bitmap (for debugging)
void dump_charset() {
    int index = 0;
    int c;
    for (int i = 0; i < 256; i++) {
        printf("Char %3d (0x%02X):\n", i, i);
        for (int j = 0; j < 9; j++) {
            c = charset[index++];
            for (int k = 8; k >= 0; k--) {
                if (c & (1 << k)) {
                    printf("O");
                } else {
                    printf(".");
                }
            }
            printf("\n");
        }
        printf("\n");
    }
}

// Rotate the charset bitmaps 90 degrees clockwise
void rotate_charset() {
    int nc[9];
    int index = 0;
    int c;
    for (int i = 0; i < 256; i++) {
        for (int j = 0; j < 9; j++) {
            nc[j] = 0;
        }
        for (int j = 0; j < 9; j++) {
            c = charset[index++];
            for (int k = 0; k < 9; k++) {
                if (c & (1 << k)) {
                    nc[8 - k] |= (1 << j);
                }
            }
        }
        index -= 9;
        for (int j = 0; j < 9; j++) {
            charset[index++] = nc[j];
        }
    }
}

// Initialize the printer
void printer_init() {
    epson_initialized = 1;
    rotate_charset();
    if (debug_enabled) print_stderr("Printer initialized.\n");
    printer_reset();
}

// Reset the printer
void printer_reset() {
    if (!epson_initialized) {
        fprintf(stderr, "Error: printer not initialized.\n");
        return;
    }
    // Restore default settings
    // Respect wide_carriage: if set, keep wide printable page_width
    if (wide_carriage) {
        page_width = WIDE_WIDTH;
    } else {
        page_width = PAGE_WIDTH;
    }
    page_height = PAGE_HEIGHT;
    page_cpi = PAGE_CPI;
    page_lpi = PAGE_LPI;
    page_xmargin = PAGE_XMARGIN;
    page_ymargin = PAGE_YMARGIN;
    //
    mode_bold = 0;
    mode_italic = 0;
    mode_doublestrike = 0;
    mode_wide = 0;
    mode_wide1line = 0;
    //
    step60 = 1.0 / 52.9;    // in (1 pc = 1/6 in)
    step72 = 1.0 / 72.0;    // in (1 pt = 1/72 in)
    xstep = 0.5;            // pc
    ystep = 1.0;          // in
    lstep = 1.0 / 6.0;    // in
    //
    line_count = 0;  // Initialize line count
    if (debug_enabled) print_stderr("Printer reset.\n");
}

// Get a character from the input file
int file_get_char(FILE *fi) {
    if (!epson_initialized) {
        fprintf(stderr, "Error: printer not initialized.\n");
        return EOF;
    }
    return fgetc(fi);
}

// Print one column of a character
void printer_print_column(int c) {
    float ys = ystep * step72;
    float adj = step72 * 0.5;
    // if tractor edges are present, printable area is offset from left by tractor strip width
    float x_offset_in = draw_tractor_edges ? TRACTOR_WIDTH_IN : 0.0f;
    // Printable area bounds (in inches)
    float printable_left = x_offset_in;
    float printable_right = x_offset_in + page_width;

    for (int i = 0; i < 9; i++) {
        if (c & (1 << i)) {
            float x_in = x_offset_in + xpos + adj;
            // Skip dots that would fall inside the tractor edges or outside the printable area
            if (draw_tractor_edges) {
                if (x_in < printable_left - 1e-6f || x_in > printable_right + 1e-6f) {
                    continue;
                }
            }
            // Draw a small filled circle for each dot in the PDF content stream
            pdf_draw_dot_inch(x_in, ypos + yoffset + adj + (i * ys), DOT_RADIUS, vintage_enabled ? vintage_dot_misalignment[i] : 0.0f);
        }
    }
}

// Print one character
void printer_print_char(int c) {
    if (mode_italic)
        c += 128;
    int index = c * 9;
    int lc = 0;
    float xs = xstep * step60; // xs is in inches per dot column (1/120 in at 10 cpi)
    float xhs = xs / 2;
    float xds = xs * 2;
    float ys = ystep * step72;
    float yhs = ys / 2;
    for (int i = 0; i < 9; i++) {
        c = charset[index];
        printer_print_column(c | mode_underline);

        if (mode_doublestrike)
            ypos += yhs;
        if (mode_bold)
            xpos += xs;
        if(mode_bold || mode_doublestrike) {
            printer_print_column(c | mode_underline);
        }
        if (mode_bold)
            xpos -= xs;
        if (mode_doublestrike)
            ypos -= yhs;
        if(mode_wide) {
            xpos += xds;
            printer_print_column(c | mode_underline);
            xpos -= xs;
        }
        index++;
        xpos += xs;
        lc = c;
    }
    for (int i = 0; i <= mode_wide; i++) {
        xpos += xs;
        xpos += xs;
        xpos += xs;
    }
}

// Process graphics
void process_graphics(float gstep) {
    int nl = file_get_char(fi);
    if (nl == EOF)
        return;
    int nh = file_get_char(fi);
    if (nh == EOF)
        return;
    int n = nl + 256 * nh;
    print_stderr("<%d>", n);
    float xs = gstep * step60;
    int c;
    while (n > 0) {
        c = file_get_char(fi);
        if (c == EOF)
            return;
        // reverse the bit order
        c = ((c & 0x01) << 7) | ((c & 0x02) << 5) | ((c & 0x04) << 3) | ((c & 0x08) << 1) | ((c & 0x10) >> 1) | ((c & 0x20) >> 3) | ((c & 0x40) >> 5) | ((c & 0x80) >> 7);
        printer_print_column(c);
        xpos += xs;
        n--;
    }
}

// Process LPI sequence
void process_lpi(float ppi) {
    int n = file_get_char(fi);
    if (n == EOF)
        return;
    lstep = (float)n / ppi;
    // print the lstep
    print_stderr("<%f>", lstep);
}


// Process subscript/superscript sequence
int process_sscript() {
    int c = file_get_char(fi);
    int result = 0;
    if (c < 31) {
        print_control(c);
    } else {
        print_stderr("%c", c);
    }
    switch (c) {
        case '0':   // Superscript on
        case 0:
            mode_subscript = 0;
            mode_superscript = 1;
            yoffset = 0;
            ystep = 0.5;
            break;
        case '1':   // Subscript on
        case 1:
            mode_subscript = 1;
            mode_superscript = 0;
            yoffset = 0.05;
            ystep = 0.5;
            break;
        default:
            yoffset = 0;
            ystep = 1.0;
            result = 1;
            break;
    }
    return result;
}

// Process underline sequence
void process_underline() {
    int c = file_get_char(fi);
    if (c < 31) {
        print_control(c);
    } else {
        print_stderr("%c", c);
    }
    switch (c) {
        case '0':   // Underline off
        case 0:
            mode_underline = 0;
            break;
        case '1':   // Underline on
        case 1:
            mode_underline = 256;
            break;
        default:
            break;
    }
}

// Process an escape sequence
int printer_process_escape() {
    int c = file_get_char(fi);
    int result = 0;
    print_stderr("<ESC>%c", c);
    switch (c) {
        case '@':   // Reset printer
            printer_reset();
            break;
        case 'E':   // Bold on
            mode_bold = 1;
            break;
        case 'F':   // Bold off
            mode_bold = 0;
            break;
        case '4':   // Italic on
            mode_italic = 1;
            break;
        case '5':   // Italic off
            mode_italic = 0;
            break;
        case 'G':   // Double strike on
            mode_doublestrike = 1;
            break;
        case 'H':   // Double strike off
            mode_doublestrike = 0;
            break;
        case 'S':   // Subscript/Superscript on
            result = process_sscript();
            break;
        case 'T':   // Subscript/Superscript off
            mode_subscript = 0;
            mode_superscript = 0;
            yoffset = 0;
            ystep = 1.0;
            break;
        case 'M':   // 12 cpi (Elite)
            mode_elite = 1;
            if (mode_compressed)
                xstep = 10.0 / 20.0 / 2.0;
            else
                xstep = 10.0 / 12.0 / 2.0;
            break;
        case 'P':   // 10 cpi (Pica)
            mode_elite = 0;
            if (mode_compressed)
                xstep = 10.0 / 17.16 / 2.0;
            else
                xstep = 0.5;
            break;
        case '-':   // Underline mode
            process_underline();
            break;
        case 'K':   // 60dpi graphics
            process_graphics(1.0);
            break;
        case 'L':   // 120dpi graphics
        case 'Y':   // 120dpi graphics (fast)
            process_graphics(0.5);
            break;
        case '0':   // Set LPI = 1/8 in
            lstep = 1.0 / 8.0;
            break;
        case '1':   // Set LPI = 7/72 in
            lstep = 7.0 / 72.0;
            break;
        case '2':   // Set LPI = 1/6 in
            lstep = 1.0 / 6.0;
            break;
        case 'A':   // Set LPI n/72 in
            process_lpi(72);
            break;
        case '3':   // Set LPI = n/216 in
            process_lpi(216);
            break;
        default:
            break;
    }
    return result;
}

// Process form feed
void process_ff() {
    // Create a new PDF page and reset the cursor to the top-left corner.
    // pdf_new_page uses page_width/page_height already defined.
    pdf_new_page();
    if (debug_enabled) print_stderr("Advanced to page %d\n", pdf_pages);
    xpos = page_xmargin;
    ypos = page_ymargin;
    line_count = 0;  // Reset line count on new page
}

// Process backspace
void process_bs() {
    float xs = xstep * step60;
    if (mode_wide1line || mode_wide) {
        xpos -= xs * 24.0;
    } else {
        xpos -= xs * 12.0;
    }
    if (xpos < page_xmargin)
        xpos = page_xmargin;
}

// Process a character
int printer_process_char(int c) {
    if (!epson_initialized) {
        fprintf(stderr, "Error: printer not initialized.\n");
        return 1;
    }
    if (c == EOF) {
        print_stderr("End of file.\n");
        return 1;
    }

    // Translate certain UTF-8 characters to charset equivalents
    if (c == 0xC3) { // UTF-8 prefix for accented characters
        int c2 = file_get_char(fi);
        if (c2 == EOF) return 1;

        // Table-driven mapping for C3 xx codes
        struct {
            unsigned char code;
            char base;
            char accent;
        } utf8_map[] = {
            {0x81, 'A', '\''}, {0xA1, 'a', '\''}, // Á, á
            {0x89, 'E', '\''}, {0xA9, 'e', '\''}, // É, é
            {0x8D, 'I', '\''}, {0xAD, 'i', '\''}, // Í, í
            {0x93, 'O', '\''}, {0xB3, 'o', '\''}, // Ó, ó
            {0x9A, 'U', '\''}, {0xBA, 'u', '\''}, // Ú, ú
            {0x80, 'A', '`'},  {0xA0, 'a', '`'},  // À, à
            {0x88, 'E', '`'},  {0xA8, 'e', '`'},  // È, è
            {0x8C, 'I', '`'},  {0xAC, 'i', '`'},  // Ì, ì
            {0x92, 'O', '`'},  {0xB2, 'o', '`'},  // Ò, ò
            {0x99, 'U', '`'},  {0xB9, 'u', '`'},  // Ù, ù
            {0x83, 'A', '~'},  {0xA3, 'a', '~'},  // Ã, ã
            {0x95, 'O', '~'},  {0xB5, 'o', '~'},  // Õ, õ
            {0x82, 'A', '^'},  {0xA2, 'a', '^'},  // Â, â
            {0x8A, 'E', '^'},  {0xAA, 'e', '^'},  // Ê, ê
            {0x94, 'O', '^'},  {0xB4, 'o', '^'},  // Ô, ô
            {0x87, 'C', ','},  {0xA7, 'c', ','},  // Ç, ç
        };
        int found = 0;
        for (size_t i = 0; i < sizeof(utf8_map)/sizeof(utf8_map[0]); i++) {
            if (c2 == utf8_map[i].code) {
                printer_print_char(utf8_map[i].base);
                process_bs();
                printer_print_char(utf8_map[i].accent);
                found = 1;
                break;
            }
        }
        if (!found) {
            printer_print_char('?');
        }
        return 0;
    }

    // Print the character
    if (c > 31) {
        print_stderr("%c", c);
        printer_print_char(c);
        return 0;
    }

    // Process escape sequences
    if (c == 27)
        return printer_process_escape();

    // Process control characters
    print_control(c);
    switch (c) {
        case 8:     // BS
            process_bs();
            break;
        case 9:     // HT (Horizontal Tab)
            {
                // Tab stops at every TAB_STOPS characters (standard for Epson printers)
                float xs = xstep * step60; // width per dot column
                float char_width = xs * 12.0; // width of one character (12 dot columns)
                if (mode_wide || mode_wide1line) {
                    char_width = xs * 24.0; // double width
                }
                float current_col = (xpos - page_xmargin) / char_width;
                int next_tab_stop = ((int)(current_col / TAB_STOPS) + 1) * TAB_STOPS;
                xpos = page_xmargin + (next_tab_stop * char_width);
                // Don't go past right margin
                if (xpos > page_xmargin + page_width) {
                    xpos = page_xmargin;
                }
            }
            break;
        case 10:    // LF
            ypos += lstep;
            if (auto_cr)
                xpos = page_xmargin;
            line_count++;
            if (line_count >= PAGE_LINES) {
                process_ff();
            }
            break;
        case 12:    // FF
            print_stderr("\n");
            process_ff();
            break;
        case 13:    // CR
            xpos = page_xmargin;
            break;
        case 15:    // SI (compressed)
            mode_compressed = 1;
            if (mode_elite)
                xstep = 10.0 / 20.0 / 2.0;
            else
                xstep = 10.0 / 17.16 / 2.0;
            break;
        case 18:   // DC2 (pica)
            mode_compressed = 0;
            if (mode_elite)
                xstep = 10.0 / 12.0 / 2.0;
            else
                xstep = 0.5;
            break;
        case 14:    // SO (expanded on)
            mode_wide = 1;
            break;
        case 20:    // DC4 (expanded off)
            mode_wide = 0; 
            break;
        default:
            break;
    }
    return 0;
}

// Option flags available at runtime
// (TRACTOR_* macros are defined earlier near declarations)

#endif // EPSON_H