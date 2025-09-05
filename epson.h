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

// --- Lightweight multi-page PDF generation ---
// We produce a small PDF with multiple pages. Dots are drawn as filled circles
// approximated using four cubic Bezier curves.

char **pdf_contents = NULL;   // per-page content buffers
size_t *pdf_lens = NULL;
size_t *pdf_caps = NULL;
int pdf_pages = 0;

void pdf_new_page() {
    // add a new empty page buffer
    int new_pages = pdf_pages + 1;
    pdf_contents = (char**)realloc(pdf_contents, sizeof(char*) * new_pages);
    pdf_lens = (size_t*)realloc(pdf_lens, sizeof(size_t) * new_pages);
    pdf_caps = (size_t*)realloc(pdf_caps, sizeof(size_t) * new_pages);
    // initialize new page buffer
    pdf_contents[pdf_pages] = NULL;
    pdf_caps[pdf_pages] = 0;
    pdf_lens[pdf_pages] = 0;
    // ensure a small initial capacity
    pdf_caps[pdf_pages] = 8192;
    pdf_contents[pdf_pages] = (char*)malloc(pdf_caps[pdf_pages]);
    pdf_lens[pdf_pages] = 0;
    pdf_pages = new_pages;
}

void pdf_ensure(size_t extra) {
    if (pdf_pages == 0) pdf_new_page();
    int idx = pdf_pages - 1;
    if (pdf_contents[idx] == NULL) {
        pdf_caps[idx] = 8192;
        pdf_contents[idx] = (char*)malloc(pdf_caps[idx]);
        pdf_lens[idx] = 0;
    }
    if (pdf_lens[idx] + extra + 1 > pdf_caps[idx]) {
        while (pdf_lens[idx] + extra + 1 > pdf_caps[idx]) pdf_caps[idx] *= 2;
        pdf_contents[idx] = (char*)realloc(pdf_contents[idx], pdf_caps[idx]);
    }
}

void pdf_appendf(const char *fmt, ...) {
    if (pdf_pages == 0) pdf_new_page();
    int idx = pdf_pages - 1;
    va_list args;
    va_start(args, fmt);
    va_list args2;
    va_copy(args2, args);
    int needed = vsnprintf(NULL, 0, fmt, args2);
    va_end(args2);
    if (needed < 0) needed = 0;
    pdf_ensure((size_t)needed);
    vsnprintf(pdf_contents[idx] + pdf_lens[idx], pdf_caps[idx] - pdf_lens[idx], fmt, args);
    pdf_lens[idx] += (size_t)needed;
    va_end(args);
}

void pdf_init() {
    // free any existing pages
    if (pdf_contents) {
        for (int i = 0; i < pdf_pages; i++) {
            free(pdf_contents[i]);
        }
        free(pdf_contents);
        free(pdf_lens);
        free(pdf_caps);
    }
    pdf_contents = NULL;
    pdf_lens = NULL;
    pdf_caps = NULL;
    pdf_pages = 0;
    // create first page
    pdf_new_page();
}

// Draw a filled circle centered at (x_in inches, y_in inches) with radius in points.
// We approximate circle with 4 cubic Bézier curves using kappa.
void pdf_draw_dot_inch(float x_in, float y_in, float radius_pt) {
    // Convert to points (72 pt = 1 in). PDF origin is bottom-left.
    float cx = x_in * 72.0f;
    float cy = page_height * 72.0f - (y_in * 72.0f);
    float r = radius_pt;
    const float k = 0.552284749831f; // approximation constant
    float ox = r * k;
    // Points for four segments
    float x0 = cx + r; float y0 = cy;
    float x1 = cx + r; float y1 = cy + ox;
    float x2 = cx + ox; float y2 = cy + r;
    float x3 = cx;     float y3 = cy + r;
    float x4 = cx - ox; float y4 = cy + r;
    float x5 = cx - r; float y5 = cy + ox;
    float x6 = cx - r; float y6 = cy;
    float x7 = cx - r; float y7 = cy - ox;
    float x8 = cx - ox; float y8 = cy - r;
    float x9 = cx;     float y9 = cy - r;
    float x10 = cx + ox; float y10 = cy - r;
    float x11 = cx + r; float y11 = cy - ox;
    // Build path: move to x0,y0 then four 'c' operators
    pdf_appendf("%.3f %.3f m\n", x0, y0);
    pdf_appendf("%.3f %.3f %.3f %.3f %.3f %.3f c\n", x1, y1, x2, y2, x3, y3);
    pdf_appendf("%.3f %.3f %.3f %.3f %.3f %.3f c\n", x4, y4, x5, y5, x6, y6);
    pdf_appendf("%.3f %.3f %.3f %.3f %.3f %.3f c\n", x7, y7, x8, y8, x9, y9);
    pdf_appendf("%.3f %.3f %.3f %.3f %.3f %.3f c\n", x10, y10, x11, y11, x0, y0);
    pdf_appendf("f\n");
}

// Write the PDF file to the given FILE*
void pdf_write(FILE *out) {
    if (!out) return;
    if (pdf_pages == 0) return;
    // total objects: 1 Catalog + 1 Pages + 2 per page (Page obj + Content obj)
    int totalObjs = 2 + (2 * pdf_pages);
    long *offsets = (long*)malloc(sizeof(long) * (totalObjs + 1));
    memset(offsets, 0, sizeof(long) * (totalObjs + 1));

    // Header
    offsets[0] = ftell(out);
    fprintf(out, "%%PDF-1.1\n%%\xFF\xFF\xFF\xFF\n");

    // 1 0 obj Catalog
    offsets[1] = ftell(out);
    fprintf(out, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");

    // 2 0 obj Pages
    offsets[2] = ftell(out);
    fprintf(out, "2 0 obj\n<< /Type /Pages /Kids [");
    // list page object references
    for (int i = 0; i < pdf_pages; i++) {
        int pageObjId = 3 + i * 2;
        fprintf(out, "%d 0 R ", pageObjId);
    }
    fprintf(out, "] /Count %d >>\nendobj\n", pdf_pages);

    // Write each Page object (reserve IDs)
    for (int i = 0; i < pdf_pages; i++) {
        int pageObjId = 3 + i * 2;
        int contentObjId = 4 + i * 2;
        offsets[pageObjId] = ftell(out);
        float w_pt = page_width * 72.0f;
        float h_pt = page_height * 72.0f;
        fprintf(out, "%d 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %.3f %.3f] /Contents %d 0 R /Resources << >> >>\nendobj\n", pageObjId, w_pt, h_pt, contentObjId);
    }

    // Write each Content object (stream)
    for (int i = 0; i < pdf_pages; i++) {
        int contentObjId = 4 + i * 2;
        offsets[contentObjId] = ftell(out);
        fprintf(out, "%d 0 obj\n<< /Length %zu >>\nstream\n", contentObjId, pdf_lens[i]);
        if (pdf_lens[i] > 0) {
            fwrite(pdf_contents[i], 1, pdf_lens[i], out);
        }
        fprintf(out, "\nendstream\nendobj\n");
    }

    // xref
    long xref_pos = ftell(out);
    fprintf(out, "xref\n0 %d\n0000000000 65535 f \n", totalObjs + 1);
    for (int i = 1; i <= totalObjs; i++) {
        // If offsets entry is zero (shouldn't), print zeros
        fprintf(out, "%010ld 00000 n \n", offsets[i]);
    }

    // trailer
    fprintf(out, "trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%ld\n%%%%EOF\n", totalObjs + 1, xref_pos);

    free(offsets);
}

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
            // Draw a small filled rectangle for each dot in the PDF content stream
            pdf_draw_dot_inch(xpos + adj, ypos + yoffset + adj + (i * ys), DOT_RADIUS);
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
    // Create a new PDF page and reset the cursor to the top-left corner.
    // pdf_new_page uses page_width/page_height already defined.
    pdf_new_page();
    print_stderr("Advanced to page %d\n", pdf_pages);
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