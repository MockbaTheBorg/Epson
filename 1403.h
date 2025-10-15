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
#define PAGE_YMARGIN 0.0    // in

// Printer flags
int auto_cr = 1;

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
extern float xstep;
extern float ystep;

// Printer modes
extern int mode_bold, mode_italic, mode_doublestrike, mode_wide, mode_wide1line, mode_underline, mode_subscript, mode_superscript, mode_elite, mode_compressed;

// Input file
extern FILE *fi;

// Forward declarations for tractor-edge option (defined later)
extern int draw_tractor_edges;
extern int draw_green_strips;
extern int wide_carriage;
extern int debug_enabled; // forward decl for debug flag
void pdf_draw_tractor_edges_page(void);
void printer_reset(void);

// Define an array with the names of control characters from 0 to 31
const char *control_names[] = {
    "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
    "BS", "HT", "LF", "VT", "FF", "CR", "SO", "SI",
    "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
    "CAN", "EM", "SUB", "ESC", "FS", "GS", "RS", "US"
};

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

// Reset the printer
void printer_reset() {
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
    xstep = 0.5;          // pc
    ystep = 1.0;          // in
    //
    line_count = 0;  // Initialize line count
    if (debug_enabled) print_stderr("Printer reset.\n");
}

// Get a character from the input file
int file_get_char(FILE *fi) {
    return fgetc(fi);
}

// Print one character
void printer_print_char(int c) {
    // Determine font based on modes
    int font_id = 1; // Courier
    // Draw the character
    pdf_draw_char(xpos, ypos, font_id, (char)c);
    // Advance cursor
    float char_width = 0.125f; // 10 cpi
    xpos += char_width;
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

int hammer_process_char(int c) {
    if (c == EOF) {
        print_stderr("End of file.\n");
        return 1;
    }

    // If c is a valid ASCII character, print it
    if (c >= 32 && c <= 126) {
        printer_print_char(c);
        return 0;
    }

    // Process control characters
    switch (c) {
        case 9:     // HT (Horizontal Tab)
            {
                // Tab stops at every 8 characters (standard)
                float char_width = 1.0f / page_cpi; // width of one character in inches
                float current_col = (xpos - page_xmargin) / char_width;
                int next_tab_stop = ((int)(current_col / 8) + 1) * 8;
                xpos = page_xmargin + (next_tab_stop * char_width);
                // Don't go past right margin
                if (xpos > page_width) {
                    xpos = page_xmargin;
                    ypos += 1.0 / page_lpi; // advance to next line
                    if (ypos >= page_height) {
                        pdf_new_page();
                        ypos = page_ymargin;
                    }
                }
            }
            break;
        case 10:    // LF
            ypos += 1.0 / page_lpi;
            if (ypos >= page_height) {
                pdf_new_page();
                ypos = page_ymargin;
            }
            xpos = page_xmargin;
            break;
        case 13:    // CR
            xpos = page_xmargin;
            break;
        case 12:    // FF
            pdf_new_page();
            xpos = page_xmargin;
            ypos = page_ymargin;
            line_count = 0;
            break;
        default:
            // ignore other control characters
            break;
    }
    return 0;
}

// (TRACTOR_* macros are defined earlier near declarations)

#endif // EPSON_H