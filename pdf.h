#ifndef PDF_H
#define PDF_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

// External declarations
extern int draw_tractor_edges;
extern int draw_green_strips;
extern float page_width;
extern float page_height;

// Tractor constants
#define TRACTOR_WIDTH_IN 0.5f                 // width of each tractor strip (inches)
#define TRACTOR_HOLE_SPACING_IN 0.5f          // spacing between holes (inches)
#define TRACTOR_HOLE_MARGIN_IN 0.25f          // margin from edge to first hole (inches)
#define TRACTOR_HOLE_RADIUS_PT 7.0f           // radius of tractor holes (points)

void pdf_draw_tractor_edges_page(void);
int pdf_load_font(const char *font_file_path);

// --- Lightweight multi-page PDF generation ---
// We produce a small PDF with multiple pages. Dots are drawn as filled circles
// approximated using four cubic Bezier curves.

extern char **pdf_contents;   // per-page content buffers
extern size_t *pdf_lens;
extern size_t *pdf_caps;
extern int pdf_pages;

// Font data
static char *font_data = NULL;
static size_t font_data_len = 0;
static char *font_path_used = NULL;
static int font_needed = 0;  // Flag to track if font resources are needed

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

// Load a TrueType font file
int pdf_load_font(const char *font_file_path) {
    FILE *f = fopen(font_file_path, "rb");
    if (!f) {
        fprintf(stderr, "Warning: Could not open font file '%s', using built-in Courier\n", font_file_path);
        return 0;
    }
    // Get file size
    fseek(f, 0, SEEK_END);
    font_data_len = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    // Allocate and read
    font_data = (char*)malloc(font_data_len);
    if (!font_data) {
        fprintf(stderr, "Warning: Could not allocate memory for font, using built-in Courier\n");
        fclose(f);
        return 0;
    }
    size_t read_bytes = fread(font_data, 1, font_data_len, f);
    fclose(f);
    if (read_bytes != font_data_len) {
        fprintf(stderr, "Warning: Could not read font file completely, using built-in Courier\n");
        free(font_data);
        font_data = NULL;
        font_data_len = 0;
        return 0;
    }
    font_path_used = strdup(font_file_path);
    fprintf(stderr, "Loaded font: %s (%zu bytes)\n", font_file_path, font_data_len);
    return 1;
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

// Draw a character at the current position
void pdf_draw_char(float x_in, float y_in, int font_id, char c) {
    // Mark that fonts are needed for this PDF
    font_needed = 1;
    // If tractor edges are enabled, offset x position by the tractor width
    // so text remains within the printable area
    float x_offset = draw_tractor_edges ? TRACTOR_WIDTH_IN : 0.0f;
    // Convert to points (72 pt = 1 in). PDF origin is bottom-left.
    float cx = (x_in + x_offset) * 72.0f;
    // Add a top margin (font baseline offset) so first line is visible
    // For a 12pt font, we need about 12-14pt from the top edge
    float top_margin_pt = 12.0f;
    float cy = page_height * 72.0f - (y_in * 72.0f) - top_margin_pt;
    // Use the embedded font at a fixed size (10pt for 10 CPI)
    float font_size_pt = 12.0f;
    // Escape special PDF characters that have meaning inside parentheses strings
    if (c == '(' || c == ')' || c == '\\') {
        pdf_appendf("BT /F1 %.1f Tf %.3f %.3f Td (\\%c) Tj ET\n", font_size_pt, cx, cy, c);
    } else {
        pdf_appendf("BT /F1 %.1f Tf %.3f %.3f Td (%c) Tj ET\n", font_size_pt, cx, cy, c);
    }
}

// Write the PDF file to the given FILE*
void pdf_write(FILE *out) {
    if (!out) return;
    if (pdf_pages == 0) return;
    
    // Determine object count based on whether we need fonts
    // Objects: 1 Catalog + 1 Pages + Font objects + 2 per page (Page obj + Content obj)
    // If custom font: need Font dict (3), FontDescriptor (4), FontFile stream (5), then pages start at 6
    // If builtin font: need Font dict (3), then pages start at 4
    // If no font needed: pages start at 3
    int font_objs = 0;
    if (font_needed) {
        font_objs = font_data ? 3 : 1; // 3 objects for TTF (dict, descriptor, stream), 1 for builtin
    }
    int first_page_obj = 3 + font_objs;
    int totalObjs = 2 + font_objs + (2 * pdf_pages);
    long *offsets = (long*)malloc(sizeof(long) * (totalObjs + 1));
    memset(offsets, 0, sizeof(long) * (totalObjs + 1));

    // Header
    offsets[0] = ftell(out);
    fprintf(out, "%%PDF-1.4\n%%\xFF\xFF\xFF\xFF\n");

    // 1 0 obj Catalog
    offsets[1] = ftell(out);
    fprintf(out, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");

    // 2 0 obj Pages
    offsets[2] = ftell(out);
    fprintf(out, "2 0 obj\n<< /Type /Pages /Kids [");
    // list page object references
    for (int i = 0; i < pdf_pages; i++) {
        int pageObjId = first_page_obj + i * 2;
        fprintf(out, "%d 0 R ", pageObjId);
    }
    fprintf(out, "] /Count %d >>\nendobj\n", pdf_pages);

    // Only write font objects if fonts are needed
    if (font_needed) {
        if (font_data) {
            // 3 0 obj Font Dictionary (TrueType)
            offsets[3] = ftell(out);
            fprintf(out, "3 0 obj\n<< /Type /Font /Subtype /TrueType /BaseFont /CustomFont /FirstChar 32 /LastChar 126 /Widths [");
            // Simple uniform widths for monospace (600 units per character for typical monospace font at 1000 UPM)
            for (int i = 32; i <= 126; i++) {
                fprintf(out, "600 ");
            }
            fprintf(out, "] /FontDescriptor 4 0 R /Encoding /WinAnsiEncoding >>\nendobj\n");
            
            // 4 0 obj FontDescriptor
            offsets[4] = ftell(out);
            fprintf(out, "4 0 obj\n<< /Type /FontDescriptor /FontName /CustomFont /Flags 32 /FontBBox [-100 -200 1000 900] /ItalicAngle 0 /Ascent 800 /Descent -200 /CapHeight 700 /StemV 80 /FontFile2 5 0 R >>\nendobj\n");
            
            // 5 0 obj FontFile2 (TrueType font stream)
            offsets[5] = ftell(out);
            fprintf(out, "5 0 obj\n<< /Length %zu /Length1 %zu >>\nstream\n", font_data_len, font_data_len);
            fwrite(font_data, 1, font_data_len, out);
            fprintf(out, "\nendstream\nendobj\n");
        } else {
            // 3 0 obj Font (Courier builtin)
            offsets[3] = ftell(out);
            fprintf(out, "3 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Courier >>\nendobj\n");
        }
    }

    // Write each Page object
    for (int i = 0; i < pdf_pages; i++) {
        int pageObjId = first_page_obj + i * 2;
        int contentObjId = first_page_obj + i * 2 + 1;
        offsets[pageObjId] = ftell(out);
        // Page width should be page_width (printable) or page_width+2*tractor when edges enabled
        float media_width = draw_tractor_edges ? (page_width + (2.0f * TRACTOR_WIDTH_IN)) : page_width;
        float w_pt = media_width * 72.0f;
        float h_pt = page_height * 72.0f; // always 11 inches tall
        // Only include font resources if fonts are needed
        if (font_needed) {
            fprintf(out, "%d 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %.3f %.3f] /Contents %d 0 R /Resources << /Font << /F1 3 0 R >> >> >>\nendobj\n", pageObjId, w_pt, h_pt, contentObjId);
        } else {
            fprintf(out, "%d 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %.3f %.3f] /Contents %d 0 R /Resources << >> >>\nendobj\n", pageObjId, w_pt, h_pt, contentObjId);
        }
    }

    // Write each Content object (stream)
    for (int i = 0; i < pdf_pages; i++) {
        int contentObjId = first_page_obj + i * 2 + 1;
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

#endif // PDF_H