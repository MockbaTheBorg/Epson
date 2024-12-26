#ifndef EPSON_H
#define EPSON_H

// Global program settings
#define DEBUG 0

// Printer page settings (defaults)
#define PAGE_WIDTH 8.5      // in
#define PAGE_HEIGHT 11.0    // in
#define PAGE_CPI 10         // cpi
#define PAGE_LPI 6          // lpi
#define PAGE_XMARGIN 0.0    // in
#define PAGE_YMARGIN 0.0    // in

// Printer dot
#define DOT_RADIUS 0.5      // pt
#define DOT_OPACITY 0.5     // 0.0 to 1.0

// Printer flags
int auto_cr = 1;

// Printer modes
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

// Global printer variables
float page_width = PAGE_WIDTH;
float page_height = PAGE_HEIGHT;
int page_cpi = PAGE_CPI;
int page_lpi = PAGE_LPI;
float page_xmargin = PAGE_XMARGIN;
float page_ymargin = PAGE_YMARGIN;

// Page cursor position
float xpos = PAGE_XMARGIN;
float ypos = PAGE_YMARGIN;
float step60 = 0.0;         // in
float step72 = 0.0;         // in
float xstep = 0.5;          // in
float ystep = 1.0;          // in
float lstep = 0.0;          // in
float yoffset = 0;          // in   (subscript/superscript)

// SVG file definitions
const char *svg_header = "<?xml version=\"1.0\" encoding=\"ISO-8859-1\" standalone=\"no\"?>\n<svg xmlns=\"http://www.w3.org/2000/svg\" height=\"%.1fin\" width=\"%.1fin\" version=\"1.1\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" >\n";
const char *svg_footer = "</svg>\n";
const char *svg_line = "<circle cx=\"%.4fin\" cy=\"%.4fin\" r=\"%.1fpt\" style=\"stroke-width:0px; fill-opacity:%.1f; fill:black;\" />\n";

// Is the printer initialized?
int epson_initialized = 0;

// Print a message to stderr
void print_stderr(const char *msg, ...) {
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
}

// Print a control character to stderr
void print_control(int c) {
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

// Dump the charset to stderr as bitmap (for debugging)
void dump_charset() {
    int index = 0;
    int c;
    for (int i = 0; i < 256; i++) {
        for (int j = 0; j < 9; j++) {
            c = charset[index++];
            for (int k = 8; k >= 0; k--) {
                if (c & (1 << k)) {
                    print_stderr("#");
                } else {
                    print_stderr("_");
                }
            }
            print_stderr("\n");
        }
        print_stderr("\n");
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

// Reset the printer
void printer_reset() {
    if (!epson_initialized) {
        print_stderr("Error: printer not initialized.\n");
        return;
    }
    // Restore default settings
    page_width = PAGE_WIDTH;
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
    step60 = 1.0 / 60.0;    // in (1 pc = 1/6 in)
    step72 = 1.0 / 72.0;    // in (1 pt = 1/72 in)
    xstep = 0.5;          // pc
    ystep = 1.0;          // pc
    lstep = 1.0 / 6.0;    // in
    //
    print_stderr("Printer reset.\n");
}

// Initialize the printer
void printer_init() {
    epson_initialized = 1;
    rotate_charset();
    print_stderr("Printer initialized.\n");
    if (DEBUG) {
        dump_charset();
    }
    printer_reset();
}

// Get a character from the input file
int file_get_char(FILE *fi) {
    if (!epson_initialized) {
        print_stderr("Error: printer not initialized.\n");
        return EOF;
    }
    return fgetc(fi);
}

// Print one column of a character
void printer_print_column(int c) {
    float ys = ystep * step72;
    float adj = 1.0 / 72.0 * 0.5;
    for (int i = 0; i < 9; i++) {
        if (c & (1 << i)) {
            file_output(fo, svg_line, xpos + adj, ypos + yoffset + adj + (i * ys), DOT_RADIUS, DOT_OPACITY);
        }
    }
}

// Print one character
void printer_print_char(int c) {
    if (mode_italic)
        c += 128;
    int index = c * 9;
    int lc = 0;
    float xs = xstep * step60;
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
    // if 'page' > 1, close the current file, then open a new one and reset the cursor to the top left corner
    if (page > 1) {
        file_output(fo, svg_footer);
        fclose(fo);
        sprintf(pagename, "page_%03d.svg", page);
        page++;
        fo = fopen(pagename, "w");
        if (fo == NULL) {
            fprintf(stderr, "Error opening file %s\n", pagename);
            exit(1);
        }
        file_output(fo, svg_header, page_height, page_width);
    }
    xpos = page_xmargin;
    ypos = page_ymargin;
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
        print_stderr("Error: printer not initialized.\n");
        return 1;
    }
    if (c == EOF) {
        print_stderr("End of file.\n");
        return 1;
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
        case 10:    // LF
            ypos += lstep;
            if (auto_cr)
                xpos = page_xmargin;
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

#endif // EPSON_H