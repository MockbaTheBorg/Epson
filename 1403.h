#ifndef EPSON_H
#define EPSON_H

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
float page_width = PAGE_WIDTH;
float page_height = PAGE_HEIGHT;
int page_cpi = PAGE_CPI;
int page_lpi = PAGE_LPI;
float page_xmargin = PAGE_XMARGIN;
float page_ymargin = PAGE_YMARGIN;

// Line counter for automatic pagination
int line_count = 0;

// Page cursor position
float xpos = PAGE_XMARGIN;
float ypos = PAGE_YMARGIN;
float step60 = 0.0;         // in
float step72 = 0.0;         // in
float xstep = 0.5;          // in
float ystep = 1.0;          // in
float lstep = 0.0;          // in

// Forward declarations for tractor-edge option (defined later)
extern int draw_tractor_edges;
extern int draw_green_strips;
extern int wide_carriage;
extern int debug_enabled; // forward decl for debug flag
void pdf_draw_tractor_edges_page(void);
void printer_reset(void);

// Tractor edge parameters (standard continuous-form defaults)
#define TRACTOR_WIDTH_IN 0.5f                 // width of each tractor strip (inches)
#define TRACTOR_HOLE_SPACING_IN 0.5f          // center-to-center spacing of holes (inches)
#define TRACTOR_HOLE_MARGIN_IN 0.25f          // margin from top/bottom to first hole (inches)
// Hole diameter 5/32" => radius in points: (0.15625 * 72) / 2 = 5.625 pt
#define TRACTOR_HOLE_RADIUS_PT 5.625f

// Define an array with the names of control characters from 0 to 31
const char *control_names[] = {
    "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
    "BS", "HT", "LF", "VT", "FF", "CR", "SO", "SI",
    "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
    "CAN", "EM", "SUB", "ESC", "FS", "GS", "RS", "US"
};

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

    // If requested, draw tractor edges or green background immediately on the new page so they appear under dots
    if (draw_tractor_edges || draw_green_strips) {
        // the drawing routines append to the current page buffer
        pdf_draw_tractor_edges_page();
    }
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

// Draw a character at (x_in inches, y_in inches) using font_id (1=normal, 2=bold, 3=italic)
void pdf_draw_char(float x_in, float y_in, int font_id, char c) {
    // Add tractor offset if enabled, but move left by 0.1 inches
    float x_offset_in = draw_tractor_edges ? TRACTOR_WIDTH_IN : 0.0f;
    x_in += x_offset_in - 0.075f;
    // Convert to points (72 pt = 1 in). PDF origin is bottom-left.
    float x_pt = x_in * 72.0f;
    float y_pt = page_height * 72.0f - (y_in * 72.0f) - 12.0f; // baseline 12 pt below top
    pdf_appendf("BT\n/F%d 12 Tf\n%.3f %.3f Td\n(%c) Tj\nET\n", font_id, x_pt, y_pt, c);
}

// Write the PDF file to the given FILE*
void pdf_write(FILE *out) {
    if (!out) return;
    if (pdf_pages == 0) return;

    // Check if printer.ttf exists and load it
    int embed_font = 0;
    size_t font_size = 0;
    char *font_data = NULL;
    FILE *font_file = fopen("Printer.ttf", "rb");
    if (font_file) {
        fseek(font_file, 0, SEEK_END);
        font_size = ftell(font_file);
        fseek(font_file, 0, SEEK_SET);
        font_data = (char*)malloc(font_size);
        fread(font_data, 1, font_size, font_file);
        fclose(font_file);
        embed_font = 1;
    }

    // total objects: 1 Catalog + 1 Pages + 3 Fonts + (2 if embed) + 2 per page
    int totalObjs = 5 + (embed_font ? 2 : 0) + (2 * pdf_pages);
    int first_page_id = embed_font ? 8 : 6;
    int font1_id = 3;
    int font2_id = 4;
    int font3_id = 5;
    int fontfile_id = 6;
    int fontdesc_id = 7;

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
        int pageObjId = first_page_id + i * 2;
        fprintf(out, "%d 0 R ", pageObjId);
    }
    fprintf(out, "] /Count %d >>\nendobj\n", pdf_pages);

    // Write each Page object (reserve IDs)
    for (int i = 0; i < pdf_pages; i++) {
        int pageObjId = first_page_id + i * 2;
        int contentObjId = first_page_id + 1 + i * 2;
        offsets[pageObjId] = ftell(out);
        // Page width should be page_width (printable) or page_width+2*tractor when edges enabled
        float media_width = draw_tractor_edges ? (page_width + (2.0f * TRACTOR_WIDTH_IN)) : page_width;
        float w_pt = media_width * 72.0f;
        float h_pt = page_height * 72.0f; // always 11 inches tall
        fprintf(out, "%d 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %.3f %.3f] /Contents %d 0 R /Resources << /Font << /F1 %d 0 R /F2 %d 0 R /F3 %d 0 R >> >> >>\nendobj\n", pageObjId, w_pt, h_pt, contentObjId, font1_id, font2_id, font3_id);
    }

    // Write each Content object (stream)
    for (int i = 0; i < pdf_pages; i++) {
        int contentObjId = first_page_id + 1 + i * 2;
        offsets[contentObjId] = ftell(out);
        fprintf(out, "%d 0 obj\n<< /Length %zu >>\nstream\n", contentObjId, pdf_lens[i]);
        if (pdf_lens[i] > 0) {
            fwrite(pdf_contents[i], 1, pdf_lens[i], out);
        }
        fprintf(out, "\nendstream\nendobj\n");
    }

    // Write font objects
    if (embed_font) {
        // FontFile2
        offsets[fontfile_id] = ftell(out);
        fprintf(out, "%d 0 obj\n<< /Length %zu >>\nstream\n", fontfile_id, font_size);
        fwrite(font_data, 1, font_size, out);
        fprintf(out, "\nendstream\nendobj\n");
        // FontDescriptor
        offsets[fontdesc_id] = ftell(out);
        fprintf(out, "%d 0 obj\n<< /Type /FontDescriptor /FontName /Printer /Flags 4 /FontBBox [0 0 1000 1000] /FontFile2 %d 0 R >>\nendobj\n", fontdesc_id, fontfile_id);
        // Font1 (embedded)
        offsets[font1_id] = ftell(out);
        fprintf(out, "%d 0 obj\n<< /Type /Font /Subtype /TrueType /BaseFont /Printer /FontDescriptor %d 0 R >>\nendobj\n", font1_id, fontdesc_id);
    } else {
        // Font1 (standard)
        offsets[font1_id] = ftell(out);
        fprintf(out, "%d 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Courier >>\nendobj\n", font1_id);
    }
    // Font2 (bold)
    offsets[font2_id] = ftell(out);
    fprintf(out, "%d 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Courier-Bold >>\nendobj\n", font2_id);
    // Font3 (italic)
    offsets[font3_id] = ftell(out);
    fprintf(out, "%d 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Courier-Oblique >>\nendobj\n", font3_id);

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
    if (embed_font) free(font_data);
}

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

// Process LPI sequence
void process_lpi(float ppi) {
    int n = file_get_char(fi);
    if (n == EOF)
        return;
    lstep = (float)n / ppi;
    // print the lstep
    print_stderr("<%f>", lstep);
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

// Option flags available at runtime
int draw_tractor_edges = 0;
int draw_green_strips = 0;
int wide_carriage = 0;
int debug_enabled = 0; // when non-zero, print debug/info messages to stderr

// (TRACTOR_* macros are defined earlier near declarations)

// Draw tractor edges (holes + microperforation lines) for the current page
void pdf_draw_tractor_edges_page() {
    if (!draw_tractor_edges && !draw_green_strips) return;
    // calculate in points
    float tw = TRACTOR_WIDTH_IN;
    // full_width should be printable page_width + tractor strips (only if enabled)
    float full_width = page_width + (draw_tractor_edges ? (tw * 2.0f) : 0.0f); // full paper width including strips when enabled
    float w_pt = full_width * 72.0f;
    float h_pt = page_height * 72.0f;
    // seam positioned inside line of holes, between holes and printable area; place at 1/8" (0.125in) from printable edge
    float seam_offset_in = 0.125f;
    float seam_left_in = (tw - seam_offset_in); // inches from left edge
    float seam_right_in = (full_width - tw + seam_offset_in); // inches from left edge for right side

    // If green strips requested, draw them across the full paper width.
    if (draw_green_strips) {
        // Draw alternating horizontal green bands across the full paper width.
        // Standard band height: 0.5 in (matches 6 LPI -> 6 lines per inch). Bands alternate green/white.
        float band_h_in = 0.5f; // inches
        float full_w_in = full_width; // inches
        float y = 0.0f;
        // set fill color to a soft green (use RGB 0.85 1 0.85 for light green)
        pdf_appendf("0.85 1 0.85 rg\n");
        while (y < page_height - 1e-6f) {
            // draw a green band from y to y+band_h_in
            float h_band = band_h_in;
            if (y + h_band > page_height) h_band = page_height - y;
            pdf_appendf("%.3f %.3f %.3f %.3f re\nf\n", 0.0f, y * 72.0f, full_w_in * 72.0f, h_band * 72.0f);
            // advance to the next band (green + white)
            y += band_h_in * 2.0f;
        }
        // reset fill color to black
        pdf_appendf("0 0 0 rg\n");
    }

    // Only draw microperforation vertical and tractor holes if tractor edges requested
    if (draw_tractor_edges) {
        // Draw microperforation vertical as many small dots at seams
        float micro_spacing_in = 0.03125f; // spacing between microperforation dots (1/32")
        float micro_radius_pt = 0.45f; // small dot radius in points (smaller)
        for (float y = 0.0f; y <= page_height + 0.0001f; y += micro_spacing_in) {
            pdf_draw_dot_inch(seam_left_in, y, micro_radius_pt);
            pdf_draw_dot_inch(seam_right_in, y, micro_radius_pt);
        }

        // Draw tractor holes along left and right edges (centered in the strip)
        float hole_spacing = TRACTOR_HOLE_SPACING_IN;
        float hole_margin = TRACTOR_HOLE_MARGIN_IN;
        float hole_radius = TRACTOR_HOLE_RADIUS_PT;
        // place hole centers at the midpoint between the seam and the outer paper edge
        // left seam is at seam_left_in (inches from left edge); left edge is at 0.0
        float left_center_x = (seam_left_in + 0.0f) / 2.0f;
        // right seam is at seam_right_in; right edge is at full_width
        float right_center_x = (seam_right_in + full_width) / 2.0f;

        for (float y = hole_margin; y <= page_height - hole_margin + 0.0001f; y += hole_spacing) {
            // draw circles in inches -- pdf_draw_dot_inch expects inches for x,y and points for radius
            pdf_draw_dot_inch(left_center_x, y, hole_radius);
            pdf_draw_dot_inch(right_center_x, y, hole_radius);
        }
    }

    // (horizontal perforations removed) 
}

#endif // EPSON_H