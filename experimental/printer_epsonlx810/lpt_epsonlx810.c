/*
 *  Copyright (C) 2002-2004  The DOSBox Team
 *  Copyright (C) 2020       Eluan Costa Miranda
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Most of this code was taken from an old (as of 2020) unfinished DOSBox patch.
 * I've fixed numerous issues with the interface, drawing and printer logic and
 * this should print most things correctly and with the right positioning.
 *
 * Tested under DOS (Print Screen key, Edit.exe, Word Perfect 5.1), Win 3.1
 * (Write, Paintbrush) and Win 95 (Word 6, Paint). I've adapted it (ESC/P
 * commands resolution, mainly) to emulate the Epson LX-810, which is the
 * printer that I have.
 *
 * Dependencies: Freetype (until I have a dump of the character ROM - or maybe
 *               I keep it for an inaccurate "high-res" output?)
 * Output: Postscript, BMP, PNG, TIFF, JPG.
 *         (Haven't tested the Windows printer forwarding yet.)
 *
 * These are the print modes for the Epson LX-810:
 * -Bitmap printing emulates the pins but can optionally keep the old behaviour
 *  from the DOSBox patch and output continuously. Pin emulation is naive for
 *  now.
 * -Draft printing uses freely redistributable fonts that imitate the Epson FX
 *  Printers, they look almost the same as the LX-810. Not all extended
 *  characters are available and sometimes a combination of styles and effects
 *  isn't available, so it will fallback to a similar one (in this case the dots
 *  can potentially have slightly different dimensions.) High-speed draft
 *  characters are not available. Should be OK for english ASCII printing.
 * -Provide your own monospaced Roman and Sans Serif truetype or opentype fonts.
 *  Most fonts are pretty complete unicode-wise, so these will look nice.
 *  There is also code to handle non-monospaced fonts automatically that should
 *  work OK.
 *
 * INSTRUCTIONS:
 * Currently, a "printer" directory on the pcem main path is used. On Linux this
 * would be "~/.pcem/printer" by default. There, the fonts roman.ttf,
 * roman_italics.ttf, sansserif.ttf and sansserif_italics.ttf should be present.
 * Also a subdirectory "fonts" with all the Epson FX-80 opentype fonts from
 * http://const-iterator.de/fxmatrix/ should exist. This same directory will be
 * used for all the output files.
 *
 * There is also a menu for pressing some of the printer's control panel
 * buttons. On Linux, access with a right-click while the mouse is ungrabbed.
 *
 * TODO/Notes:
 * -Dump/Recreate the character ROM. This should make it possible to use the
 *  bitgraph functions to print the characters too.
 * -Better TTF font handling. Use STB TTF?
 * -Bind the printer panel buttons to key combinations in addition to the menu?
 * -Menu commands should only work when the printer is not accepting data. (Use
 *  DC1 / DC3 for this? Or the select bit in the parallel port?)
 * -Make the Windows print redirection better (currently is a hack that shows
 *  the printing dialog too soon).
 * -Dump to PDF with GhostScript.
 * -Printer redirection on linux (GhostScript, CUPS)
 * -Update the Mingw Makefiles with FreeType.
 * -Search for TODO in this file. :-)
 */

#define MAX_PATH_STRING 1024
#define false 0
#define true (!false)
#define bool int

#include <stdlib.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#if defined(WIN32)
#include <windows.h>
#include <winspool.h>
#endif

#include <math.h>
#include "lpt_epsonlx810.h"

#include "config.h"
#include "device.h"

#include <pcem/devices.h>
#include <pcem/logging.h>
#include <SDL.h>

#define STYLE_CONDENSED 0x02
#define STYLE_BOLD 0x04
#define STYLE_DOUBLESTRIKE 0x08
#define STYLE_DOUBLEWIDTH 0x10
#define STYLE_ITALICS 0x20
#define STYLE_UNDERLINE 0x40
#define STYLE_SUPERSCRIPT 0x80
#define STYLE_SUBSCRIPT 0x100
#define STYLE_DOUBLEWIDTHONELINE 0x800

#define QUALITY_DRAFT 0x01
#define QUALITY_NLQ 0x02

#define OUTPUT_TYPE_POSTSCRIPT 0
#define OUTPUT_TYPE_BMP 1
#define OUTPUT_TYPE_PNG 2
#define OUTPUT_TYPE_TIFF 3
#define OUTPUT_TYPE_JPG 4
#define OUTPUT_TYPE_FORWARD_TO_REAL_PRINTER 5

typedef enum Typeface {
        roman = 0,
        sansserif,
} Typeface;

typedef struct lpt_epsonprinter_t {
        FT_Library FTlib; // FreeType2 library used to render the characters

        SDL_Surface *page; // Surface representing the current page
        FT_Face curFont;   // The font currently used to render characters

        // TODO: where exactly in relation to the pins are curX and curY?
        double curX, curY; // Position of the print head (in inch)

        uint16_t dpi;    // dpi of the page
        uint16_t ESCCmd; // ESC-command that is currently processed
        bool ESCSeen;    // True if last read character was an ESC (0x1B)

        uint8_t numParam, neededParam; // Numbers of parameters already read/needed to process command

        uint8_t params[20];             // Buffer for the read params
        uint16_t defaultStyle, style;   // Style of font (see STYLE_* constants)
        double defaultCPI, cpi, actcpi; // CPI value set by DIP switch, program and the actual one (taking in account font types)

        double topMargin, bottomMargin, rightMargin, leftMargin; // Margins of the page (in inch)
        double pageWidth, pageHeight;                            // Size of page (in inch)
        double defaultPageWidth, defaultPageHeight;              // Default size of page (in inch)
        double lineSpacing;                                      // Size of one line (in inch)

        double horiztabs[32]; // Stores the set horizontal tabs (in inch)
        uint8_t numHorizTabs; // Number of configured tabs

        double verttabs[16]; // Stores the set vertical tabs (in inch)
        uint8_t numVertTabs; // Number of configured tabs

        uint8_t defaultCharTable, curCharTable; // Default and currently used char table und charset
        uint8_t defaultI18NCharset, curI18NCharset;
        uint16_t defaultCodePage;
        uint8_t defaultQuality, printQuality; // Print quality (see QUALITY_* constants)

        Typeface defaultNLQtypeFace, NLQtypeFace; // Typeface used in NLQ printing mode

        bool charRead;        // True if a character was read since the printer was last initialized
        bool autoFeed;        // True if a LF should automatically added after a CR
        bool printUpperContr; // True if the upper command characters should be printed

        struct bitGraphicParams // Holds information about printing bit images
        {
                uint16_t horizDens, vertDens; // Density of image to print (in dpi)
                bool adjacent;                // Print adjacent pixels? (ignored)
                uint8_t bytesColumn;          // Bytes per column
                uint16_t remBytes;            // Bytes left to read before image is done
                uint8_t column[6];            // Bytes of the current and last column
                uint8_t readBytesColumn;      // Bytes read so far for the current column
        } bitGraph;

        uint8_t densk, densl, densy, densz; // Image density modes used in ESC K/L/Y/Z commands

        uint16_t curMap[256];   // Currently used ASCII => Unicode mapping
        uint16_t charTables[2]; // Charactertables

#if defined(WIN32)
        HDC printerDC; // Win32 printer device
#endif

        int output_type;           // Output method selected by user
        void *outputHandle;        // If not null, additional pages will be appended to the given handle
        bool multipageOutput;      // If true, all pages are combined to one file/print job etc. until the "eject page" button is
                                   // pressed
        uint16_t multiPageCounter; // Current page (when printing multipages)

        uint8_t ASCII85Buffer[4]; // Buffer used in ASCII85 encoding
        uint8_t ASCII85BufferPos; // Position in ASCII85 encode buffer
        uint8_t ASCII85CurCol;    // Columns printed so far in the current lines

        uint8_t last_data;
        uint8_t controlreg;
        char fontpath[MAX_PATH_STRING];
        char outputpath[MAX_PATH_STRING];

        int emulatePinsForBitmaps;
} lpt_epsonprinter_t;

// Process one character sent to virtual printer
void printChar(lpt_epsonprinter_t *printer);

// Hard Reset (like switching printer off and on)
void resetPrinterHard(lpt_epsonprinter_t *printer);

// Set Autofeed value
void setAutofeed(bool feed, lpt_epsonprinter_t *printer);

// Get Autofeed value
bool getAutofeed(lpt_epsonprinter_t *printer);

// True if printer is unable to process more data right now (do not use printChar)
bool isBusy(lpt_epsonprinter_t *printer);

// True if the last sent character was received
bool ack(lpt_epsonprinter_t *printer);

// Manual formfeed
void formFeed(lpt_epsonprinter_t *printer);

// Returns true if the current page is blank
bool isBlank(lpt_epsonprinter_t *printer);

// Checks if given char belongs to a command and process it. If false, the character
// should be printed
bool processCommandChar(uint8_t ch, lpt_epsonprinter_t *printer);

// Resets the printer to the factory settings
void resetPrinter(lpt_epsonprinter_t *printer, int software_command);

// Reload font. Must be called after changing dpi, style or cpi
void updateFont(lpt_epsonprinter_t *printer);

// Clears page. If save is true, saves the current page to a bitmap
void newPage(bool save, lpt_epsonprinter_t *printer);

// Blits the given glyph on the page surface. If add is true, the values of bitmap are
// added to the values of the pixels in the page
void blitGlyph(FT_Bitmap bitmap, uint16_t destx, uint16_t desty, bool add, lpt_epsonprinter_t *printer);

// Draws an anti-aliased line from (fromx, y) to (tox, y).
void drawLine(uint16_t fromx, uint16_t tox, uint16_t y, lpt_epsonprinter_t *printer);

// Setup the bitGraph structure
void setupBitImage(uint8_t dens, uint16_t numCols, lpt_epsonprinter_t *printer);

// Process a character that is part of bit image. Must be called iff bitGraph.remBytes > 0.
void printBitGraph(uint8_t ch, lpt_epsonprinter_t *printer);

// Copies the codepage mapping from the constant array to CurMap, along with international character sets for character < 0x80
void updateCharset(lpt_epsonprinter_t *printer);

// Output current page
void outputPage(lpt_epsonprinter_t *printer);

// Prints out a byte using ASCII85 encoding (only outputs something every four bytes). When b>255, closes the ASCII85 string
void fprintASCII85(FILE *f, uint16_t b, lpt_epsonprinter_t *printer);

// Closes a multipage document
void finishMultipage(lpt_epsonprinter_t *printer);

// Returns value of the num-th pixel (couting left-right, top-down) in a safe way
uint8_t getPixel(uint32_t num, lpt_epsonprinter_t *printer);

#define FREETYPE_FONT_MULTIPLIER 64

#define PARAM16(I) (printer->params[(I) + 1] * 256 + printer->params[(I)])
#define CURX_INCH_TO_PIXEL ((unsigned int)floor(printer->curX * printer->dpi + 0.5))
#define CURY_INCH_TO_PIXEL ((unsigned int)floor(printer->curY * printer->dpi + 0.5))
// Be careful to do this as the last step before outputting anything
#define CURX_INCH_TO_PIXEL_ADD(v) ((unsigned int)floor(printer->curX * printer->dpi + 0.5 + (v)))
#define CURY_INCH_TO_PIXEL_ADD(v) ((unsigned int)floor(printer->curY * printer->dpi + 0.5 + (v)))

// these are the bsd safe string handling functions
#ifndef HAVE_STRLCAT
size_t strlcat(char *dst, const char *src, size_t size) {
        size_t srclen;
        size_t dstlen;

        dstlen = strlen(dst);
        size -= dstlen + 1;

        if (!size)
                return (dstlen);

        srclen = strlen(src);

        if (srclen > size)
                srclen = size;

        memcpy(dst + dstlen, src, srclen);
        dst[dstlen + srclen] = '\0';

        return (dstlen + srclen);
}
#endif /* !HAVE_STRLCAT */

#ifndef HAVE_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t size) {
        size_t srclen;

        size--;
        srclen = strlen(src);

        if (srclen > size)
                srclen = size;

        memcpy(dst, src, srclen);
        dst[srclen] = '\0';

        return (srclen);
}
#endif /* !HAVE_STRLCPY */

// Various ASCII codepage to unicode maps

static const uint16_t italicsMap[256] = {
        0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e,
        0x000f, 0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x001a, 0x001b, 0x001c, 0x001d,
        0x001e, 0x001f, 0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c,
        0x002d, 0x002e, 0x002f, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b,
        0x003c, 0x003d, 0x003e, 0x003f, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004a,
        0x004b, 0x004c, 0x004d, 0x004e, 0x004f, 0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059,
        0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f, 0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068,
        0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
        0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x007f,
        // the rest of these should be rendered in italics
        0x00e0, 0x00e8, 0x00f9, 0x00f2, 0x00ec, 0x00b0, 0x00a3, 0x00a1, 0x00bf, 0x00d1, 0x00f1, 0x00a4, 0x20a7, 0x00c5, 0x00e5,
        0x00e7, // TODO: 0x00b0 may be 0x00ba? Also 0x20a7 is annother abbreviation (Pts) instead of the Pt from epson.
        0x00a7, 0x00df, 0x00c6, 0x00e6, 0x00d8, 0x00f8, 0x00a8, 0x00c4, 0x00d6, 0x00dc, 0x00e4, 0x00f6, 0x00fc, 0x00c9, 0x00e9,
        0x00a5, 0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d,
        0x002e, 0x002f, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b, 0x003c,
        0x003d, 0x003e, 0x003f, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004a, 0x004b,
        0x004c, 0x004d, 0x004e, 0x004f, 0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005a,
        0x005b, 0x005c, 0x005d, 0x005e, 0x005f, 0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069,
        0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078,
        0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e,
        0x0030, // 0xff should be the slashed zero in italics? some fonts have that in codepoint 0xe00f
};

static const uint16_t cp437Map[256] = {
        0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e,
        0x000f, 0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x001a, 0x001b, 0x001c, 0x001d,
        0x001e, 0x001f, // TODO: 0x0015 should be 0x00a7 in some revisions of the LX-810?
        0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e,
        0x002f, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d,
        0x003e, 0x003f, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004a, 0x004b, 0x004c,
        0x004d, 0x004e, 0x004f, 0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005a, 0x005b,
        0x005c, 0x005d, 0x005e, 0x005f, 0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006a,
        0x006b, 0x006c, 0x006d, 0x006e, 0x006f, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079,
        0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x007f, 0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7, 0x00ea,
        0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5, 0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9,
        0x00ff, 0x00d6, 0x00dc, 0x00a2, 0x00a3, 0x00a5, 0x20a7, 0x0192, 0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa,
        0x00ba, 0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb, 0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561,
        0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510, 0x2514, 0x2534, 0x252c, 0x251c, 0x2500,
        0x253c, 0x255e, 0x255f, 0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567, 0x2568, 0x2564, 0x2565, 0x2559,
        0x2558, 0x2552, 0x2553, 0x256b, 0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580, 0x03b1, 0x00df, 0x0393,
        0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4, 0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x2229, 0x2261, 0x00b1,
        0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248, 0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x25a0, 0x00a0};

static const uint16_t cp850Map[256] = {
        0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e,
        0x000f, 0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x001a, 0x001b, 0x001c, 0x001d,
        0x001e, 0x001f, 0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c,
        0x002d, 0x002e, 0x002f, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b,
        0x003c, 0x003d, 0x003e, 0x003f, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004a,
        0x004b, 0x004c, 0x004d, 0x004e, 0x004f, 0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059,
        0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f, 0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068,
        0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
        0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x007f, 0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5,
        0x00e7, 0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5, 0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2,
        0x00fb, 0x00f9, 0x00ff, 0x00d6, 0x00dc, 0x00f8, 0x00a3, 0x00d8, 0x00d7, 0x0192, 0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1,
        0x00d1, 0x00aa, 0x00ba, 0x00bf, 0x00ae, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb, 0x2591, 0x2592, 0x2593, 0x2502,
        0x2524, 0x00c1, 0x00c2, 0x00c0, 0x00a9, 0x2563, 0x2551, 0x2557, 0x255d, 0x00a2, 0x00a5, 0x2510, 0x2514, 0x2534, 0x252c,
        0x251c, 0x2500, 0x253c, 0x00e3, 0x00c3, 0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x00a4, 0x00f0, 0x00d0,
        0x00ca, 0x00cb, 0x00c8, 0x0131, 0x00cd, 0x00ce, 0x00cf, 0x2518, 0x250c, 0x2588, 0x2584, 0x00a6, 0x00cc, 0x2580, 0x00d3,
        0x00df, 0x00d4, 0x00d2, 0x00f5, 0x00d5, 0x00b5, 0x00fe, 0x00de, 0x00da, 0x00db, 0x00d9, 0x00fd, 0x00dd, 0x00af, 0x00b4,
        0x00ad, 0x00b1, 0x2017, 0x00be, 0x00b6, 0x00a7, 0x00f7, 0x00b8, 0x00b0, 0x00a8, 0x00b7, 0x00b9, 0x00b3, 0x00b2, 0x25a0,
        0x00a0};

static const uint16_t cp860Map[256] = {
        0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e,
        0x000f, 0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x001a, 0x001b, 0x001c, 0x001d,
        0x001e, 0x001f, 0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c,
        0x002d, 0x002e, 0x002f, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b,
        0x003c, 0x003d, 0x003e, 0x003f, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004a,
        0x004b, 0x004c, 0x004d, 0x004e, 0x004f, 0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059,
        0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f, 0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068,
        0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
        0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x007f, 0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e3, 0x00e0, 0x00c1,
        0x00e7, 0x00ea, 0x00ca, 0x00e8, 0x00cd, 0x00d4, 0x00ec, 0x00c3, 0x00c2, 0x00c9, 0x00c0, 0x00c8, 0x00f4, 0x00f5, 0x00f2,
        0x00da, 0x00f9, 0x00cc, 0x00d5, 0x00dc, 0x00a2, 0x00a3, 0x00d9, 0x20a7, 0x00d3, 0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1,
        0x00d1, 0x00aa, 0x00ba, 0x00bf, 0x00d2, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb, 0x2591, 0x2592, 0x2593, 0x2502,
        0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510, 0x2514, 0x2534, 0x252c,
        0x251c, 0x2500, 0x253c, 0x255e, 0x255f, 0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567, 0x2568, 0x2564,
        0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b, 0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580, 0x03b1,
        0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4, 0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x2229,
        0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248, 0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x25a0,
        0x00a0};

static const uint16_t cp863Map[256] = {
        0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e,
        0x000f, 0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x001a, 0x001b, 0x001c, 0x001d,
        0x001e, 0x001f, 0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c,
        0x002d, 0x002e, 0x002f, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b,
        0x003c, 0x003d, 0x003e, 0x003f, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004a,
        0x004b, 0x004c, 0x004d, 0x004e, 0x004f, 0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059,
        0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f, 0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068,
        0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
        0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x007f, 0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00c2, 0x00e0, 0x00b6,
        0x00e7, 0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x2017, 0x00c0, 0x00a7, 0x00c9, 0x00c8, 0x00ca, 0x00f4, 0x00cb, 0x00cf,
        0x00fb, 0x00f9, 0x00a4, 0x00d4, 0x00dc, 0x00a2, 0x00a3, 0x00d9, 0x00db, 0x0192, 0x00a6, 0x00b4, 0x00f3, 0x00fa, 0x00a8,
        0x00b8, 0x00b3, 0x00af, 0x00ce, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00be, 0x00ab, 0x00bb, 0x2591, 0x2592, 0x2593, 0x2502,
        0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510, 0x2514, 0x2534, 0x252c,
        0x251c, 0x2500, 0x253c, 0x255e, 0x255f, 0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567, 0x2568, 0x2564,
        0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b, 0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580, 0x03b1,
        0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4, 0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x2229,
        0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248, 0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x25a0,
        0x00a0};

static const uint16_t cp865Map[256] = {
        0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e,
        0x000f, 0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x001a, 0x001b, 0x001c, 0x001d,
        0x001e, 0x001f, 0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c,
        0x002d, 0x002e, 0x002f, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b,
        0x003c, 0x003d, 0x003e, 0x003f, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004a,
        0x004b, 0x004c, 0x004d, 0x004e, 0x004f, 0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059,
        0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f, 0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068,
        0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
        0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x007f, 0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5,
        0x00e7, 0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5, 0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2,
        0x00fb, 0x00f9, 0x00ff, 0x00d6, 0x00dc, 0x00f8, 0x00a3, 0x00d8, 0x20a7, 0x0192, 0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1,
        0x00d1, 0x00aa, 0x00ba, 0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00a4, 0x2591, 0x2592, 0x2593, 0x2502,
        0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510, 0x2514, 0x2534, 0x252c,
        0x251c, 0x2500, 0x253c, 0x255e, 0x255f, 0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567, 0x2568, 0x2564,
        0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b, 0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580, 0x03b1,
        0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4, 0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x2229,
        0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248, 0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x25a0,
        0x00a0};

static const uint16_t intCharSets[13][12] = {
        {0x0023, 0x0024, 0x0040, 0x005b, 0x005c, 0x005d, 0x005e, 0x0060, 0x007b, 0x007c, 0x007d, 0x007e}, // USA
        {0x0023, 0x0024, 0x00e0, 0x00b0, 0x00e7, 0x00a7, 0x005e, 0x0060, 0x00e9, 0x00f9, 0x00e8,
         0x00a8}, // France TODO: 0x00b0 may be 0x00ba?
        {0x0023, 0x0024, 0x00a7, 0x00c4, 0x00d6, 0x00dc, 0x005e, 0x0060, 0x00e4, 0x00f6, 0x00fc, 0x00df}, // Germany
        {0x00a3, 0x0024, 0x0040, 0x005b, 0x005c, 0x005d, 0x005e, 0x0060, 0x007b, 0x007c, 0x007d, 0x007e}, // UK
        {0x0023, 0x0024, 0x0040, 0x00c6, 0x00d8, 0x00c5, 0x005e, 0x0060, 0x00e6, 0x00f8, 0x00e5, 0x007e}, // Denmark I
        {0x0023, 0x00a4, 0x00c9, 0x00c4, 0x00d6, 0x00c5, 0x00dc, 0x00e9, 0x00e4, 0x00f6, 0x00e5, 0x00fc}, // Sweden
        {0x0023, 0x0024, 0x0040, 0x00b0, 0x005c, 0x00e9, 0x005e, 0x00f9, 0x00e0, 0x00f2, 0x00e8,
         0x00ec}, // Italy TODO: 0x00b0 may be 0x00ba?
        {0x20a7, 0x0024, 0x0040, 0x00a1, 0x00d1, 0x00bf, 0x005e, 0x0060, 0x00a8, 0x00f1, 0x007d,
         0x007e}, // Spain I TODO: 0x20a7 is annother abbreviation (Pts) instead of the Pt from epson.
        {0x0023, 0x0024, 0x0040, 0x005b, 0x00a5, 0x005d, 0x005e, 0x0060, 0x007b, 0x007c, 0x007d, 0x007e}, // Japan
        {0x0023, 0x00a4, 0x00c9, 0x00c6, 0x00d8, 0x00c5, 0x00dc, 0x00e9, 0x00e6, 0x00f8, 0x00e5, 0x00fc}, // Norway
        {0x0023, 0x0024, 0x00c9, 0x00c6, 0x00d8, 0x00c5, 0x00dc, 0x00e9, 0x00e6, 0x00f8, 0x00e5, 0x00fc}, // Denmark II
        {0x0023, 0x0024, 0x00e1, 0x00a1, 0x00d1, 0x00bf, 0x00e9, 0x0060, 0x00ed, 0x00f1, 0x00f3, 0x00fa}, // Spain II
        {0x0023, 0x0024, 0x00e1, 0x00a1, 0x00d1, 0x00bf, 0x00e9, 0x00fc, 0x00ed, 0x00f1, 0x00f3, 0x00fa}, // Latin America
};

void resetPrinterHard(lpt_epsonprinter_t *printer) {
        printer->charRead = false;
        resetPrinter(printer, false);
}

// TODO: see exaclty what is reset with ESC @
void resetPrinter(lpt_epsonprinter_t *printer, int software_command) {
        if (!software_command) {
                printer->curX = printer->curY = 0.0;
                printer->ESCSeen = false;
                printer->ESCCmd = 0;
                printer->numParam = printer->neededParam = 0;
                // TODO: PRINTABLE AREAS ARE BEING IGNORED! Ok because we "may" print to real paper and then use the areas in the
                // real one? Have two options: 1) Have printable areas 2) Reduce the paper width accordingly.
        }
        printer->topMargin = 0.0;
        printer->leftMargin = 0.0;
        printer->rightMargin = printer->pageWidth = printer->defaultPageWidth;
        printer->bottomMargin = printer->pageHeight = printer->defaultPageHeight;
        printer->lineSpacing = (double)1 / 6;
        printer->cpi = printer->defaultCPI;
        printer->curCharTable = printer->defaultCharTable;
        printer->curI18NCharset = printer->defaultI18NCharset;
        printer->style = printer->defaultStyle;
        if (printer->defaultCharTable == 0) // Italics
                printer->printUpperContr = false;
        else
                printer->printUpperContr = true;
        printer->bitGraph.remBytes = 0;
        printer->densk = 0;
        printer->densl = 1;
        printer->densy = 2;
        printer->densz = 3;
        printer->charTables[0] = 0; // Italics
        printer->charTables[1] = printer->defaultCodePage;
        printer->NLQtypeFace = printer->defaultNLQtypeFace;
        printer->printQuality = printer->defaultQuality;

        updateCharset(printer);

        updateFont(printer);

        if (!software_command)
                newPage(false, printer);

        // Default tabs => Each eight characters
        for (unsigned int i = 0; i < 32; i++)
                printer->horiztabs[i] = i * 8 * (1 / (double)printer->cpi);
        printer->numHorizTabs = 32;

        printer->numVertTabs = 255;

        printer->last_data = 0;
}

void updateCharset(lpt_epsonprinter_t *printer) {
        uint16_t *mapToUse = NULL;
        uint16_t cp = printer->charTables[printer->curCharTable];

        switch (cp) {
        case 0:
                mapToUse = (uint16_t *)&italicsMap;
                break;
        case 437:
                mapToUse = (uint16_t *)&cp437Map;
                break;
        case 850:
                mapToUse = (uint16_t *)&cp850Map;
                break;
        case 860:
                mapToUse = (uint16_t *)&cp860Map;
                break;
        case 863:
                mapToUse = (uint16_t *)&cp863Map;
                break;
        case 865:
                mapToUse = (uint16_t *)&cp865Map;
                break;
        default:
                pclog("Unsupported codepage %i. Using CP437 instead.\n", cp);
                mapToUse = (uint16_t *)&cp437Map;
        }

        for (int i = 0; i < 256; i++)
                printer->curMap[i] = mapToUse[i];

        // TODO: See if it works like this
        printer->curMap[0x23] = intCharSets[printer->curI18NCharset][0];
        printer->curMap[0x24] = intCharSets[printer->curI18NCharset][1];
        printer->curMap[0x40] = intCharSets[printer->curI18NCharset][2];
        printer->curMap[0x5b] = intCharSets[printer->curI18NCharset][3];
        printer->curMap[0x5c] = intCharSets[printer->curI18NCharset][4];
        printer->curMap[0x5d] = intCharSets[printer->curI18NCharset][5];
        printer->curMap[0x5e] = intCharSets[printer->curI18NCharset][6];
        printer->curMap[0x60] = intCharSets[printer->curI18NCharset][7];
        printer->curMap[0x7b] = intCharSets[printer->curI18NCharset][8];
        printer->curMap[0x7c] = intCharSets[printer->curI18NCharset][9];
        printer->curMap[0x7d] = intCharSets[printer->curI18NCharset][10];
        printer->curMap[0x7e] = intCharSets[printer->curI18NCharset][11];
}

// TODO: all these fonts are approximations! Even the FX one is not exactly equal to the LX-810 and it is 7-bit ascii only.
// TODO: maybe we are calling this unnecessarily (some parameters only affect drawing)
void updateFont(lpt_epsonprinter_t *printer) {
        if (printer->curFont != NULL)
                FT_Done_Face(printer->curFont);

        if (printer->printQuality == QUALITY_NLQ) {
                const char *fontName;

                switch (printer->NLQtypeFace) {
                case roman:
                        if (printer->style & STYLE_ITALICS)
                                fontName = "roman_italics.ttf";
                        else
                                fontName = "roman.ttf";
                        break;
                case sansserif:
                        if (printer->style & STYLE_ITALICS)
                                fontName = "sansserif_italics.ttf";
                        else
                                fontName = "sansserif.ttf";
                        break;
                default:
                        fatal("Unknown font: %d\n", printer->NLQtypeFace);
                }

                char fullpath[MAX_PATH_STRING];
                strlcpy(fullpath, printer->fontpath, MAX_PATH_STRING);
                strlcat(fullpath, fontName, MAX_PATH_STRING);

                if (FT_New_Face(printer->FTlib, fullpath, 0, &printer->curFont)) {
                        fatal("Unable to load font %s\n", fullpath);
                        printer->curFont = NULL;
                }

                double horizPoints = 10.5;
                double vertPoints = 10.5;

                printer->actcpi = printer->cpi;

                // TODO: condensed only for draft fonts?
                if (printer->cpi != 10 && !(printer->style & STYLE_CONDENSED)) {
                        horizPoints *= (double)10 / (double)printer->cpi;
                        // vertPoints *= (double)10 / (double)printer->cpi;
                }

                if (printer->cpi == 10 && (printer->style & STYLE_CONDENSED)) {
                        // TODO: 17.125, 17.16 or 17.14?
                        printer->actcpi = 17.125;
                        horizPoints *= (double)10 / (double)17.125;
                        // vertPoints *= (double)10 / (double)17.125;
                }

                if (printer->cpi == 12 && (printer->style & STYLE_CONDENSED)) {
                        printer->actcpi = 20.0;
                        horizPoints *= (double)10 / (double)20.0;
                        // vertPoints *= (double)10 / (double)20.0;
                }

                if ((printer->style & STYLE_DOUBLEWIDTH) || (printer->style & STYLE_DOUBLEWIDTHONELINE)) {
                        printer->actcpi /= 2;
                        horizPoints *= (double)2.0;
                }

                if ((printer->style & STYLE_SUPERSCRIPT) || (printer->style & STYLE_SUBSCRIPT)) {
                        // TODO: aparently the horizontal size is the same?
                        // horizPoints *= (double)2 / (double)3;
                        vertPoints *= (double)2 / (double)3;
                        // actcpi /= (double)2 / (double)3;
                }

                /* TODO: emphasized and double-strike also change width? maybe not */

                FT_Set_Char_Size(printer->curFont, (uint16_t)horizPoints * FREETYPE_FONT_MULTIPLIER,
                                 (uint16_t)vertPoints * FREETYPE_FONT_MULTIPLIER, printer->dpi, printer->dpi);
        } else if (printer->printQuality == QUALITY_DRAFT) // TODO: high-speed draft
        {
                const char *fontName_Base;
                // TODO: these fonts are ALL incomplete! Need one with all codepages and all 13 international characters for each.
                // Can I dump this from my LX-810 and do something?
                if (printer->charTables[printer->curCharTable] == 0)
                        fontName_Base = "FXMatrix105Mono";
                else
                        fontName_Base = "FXMatrix105Mono";

                const char *fontName_cpi;
                if (!(printer->style & STYLE_CONDENSED)) {
                        if (printer->cpi == 10)
                                fontName_cpi = "Pica";
                        else if (printer->cpi == 12)
                                fontName_cpi = "Elite";
                } else {
                        if (printer->cpi == 10)
                                fontName_cpi = "Compr";
                        else if (printer->cpi == 12)
                                fontName_cpi = "Compr"; // TODO: missing
                }

                const char *fontName_doublewidth;
                if ((printer->style & STYLE_DOUBLEWIDTH) || (printer->style & STYLE_DOUBLEWIDTHONELINE))
                        fontName_doublewidth = "Exp";
                else
                        fontName_doublewidth = "";

                const char *fontName_doublestrike;
                if (printer->style & STYLE_DOUBLESTRIKE)
                        fontName_doublestrike = "Dbl";
                else
                        fontName_doublestrike = "";

                const char *fontName_underline;
                if (printer->style & STYLE_UNDERLINE)
                        fontName_underline = "UL";
                else
                        fontName_underline = "";

                const char *fontName_subsup;
                if (printer->style & STYLE_SUPERSCRIPT)
                        fontName_subsup = "Sup";
                else if (printer->style & STYLE_SUBSCRIPT)
                        fontName_subsup = "Sub";
                else
                        fontName_subsup = "";

                const char *fontName_itemph;
                if (printer->style & STYLE_ITALICS) {
                        if (printer->style & STYLE_BOLD)
                                fontName_itemph = "BoldItalic";
                        else
                                fontName_itemph = "Italic";
                } else if (printer->style & STYLE_BOLD)
                        fontName_itemph = "Bold";
                else
                        fontName_itemph = "Regular";

                double horizPoints = 10.5;
                double vertPoints = 10.5;

                printer->actcpi = printer->cpi;

                if (printer->cpi != 10 && !(printer->style & STYLE_CONDENSED)) {
                        horizPoints *= (double)10 / (double)printer->cpi;
                        // vertPoints *= (double)10/ ( double)printer->cpi;
                }

                if (printer->cpi == 10 && (printer->style & STYLE_CONDENSED)) {
                        // TODO: 17.125, 17.16 or 17.14?
                        printer->actcpi = 17.125;
                        horizPoints *= (double)10 / (double)17.125;
                        // vertPoints *= (double)10 / (double)17.125;
                }

                if (printer->cpi == 12 && (printer->style & STYLE_CONDENSED)) {
                        printer->actcpi = 20.0;
                        horizPoints *= (double)10 / (double)20.0;
                        // vertPoints *= (double)10 / (double)20.0;
                }

                if ((printer->style & STYLE_DOUBLEWIDTH) || (printer->style & STYLE_DOUBLEWIDTHONELINE)) {
                        printer->actcpi /= 2;
                        //                        horizPoints *= (double)2.0; //  TODO: commenting this because of the font. See
                        //                        also where we adjusted cpi/points because the font already accounted for that in
                        //                        its sizes
                }

                if ((printer->style & STYLE_SUPERSCRIPT) || (printer->style & STYLE_SUBSCRIPT)) {
                        // TODO: aparently the horizontal size is the same?
                        // horizPoints *= (double)2/(double)3;
                        vertPoints *= (double)2 / (double)3;
                        // printer->actcpi /= (double)2/(double)3;
                }

                /* TODO: emphasized and double-strike also change width? maybe not */

                char fullpath[MAX_PATH_STRING];
                strlcpy(fullpath, printer->fontpath, MAX_PATH_STRING);
                strlcat(fullpath, "fonts/", MAX_PATH_STRING);
                strlcat(fullpath, fontName_Base, MAX_PATH_STRING);
                strlcat(fullpath, fontName_cpi, MAX_PATH_STRING);
                strlcat(fullpath, fontName_doublewidth, MAX_PATH_STRING);
                strlcat(fullpath, fontName_doublestrike, MAX_PATH_STRING);
                strlcat(fullpath, fontName_underline, MAX_PATH_STRING);
                strlcat(fullpath, fontName_subsup, MAX_PATH_STRING);
                strlcat(fullpath, fontName_itemph, MAX_PATH_STRING);
                strlcat(fullpath, ".otf", MAX_PATH_STRING);

                if (FT_New_Face(printer->FTlib, fullpath, 0, &printer->curFont)) {
                        pclog("Unable to load font %s, trying default\n", fullpath);
                        strlcpy(fullpath, printer->fontpath, MAX_PATH_STRING);
                        strlcat(fullpath, "fonts/", MAX_PATH_STRING);
                        strlcat(fullpath, fontName_Base, MAX_PATH_STRING);
                        strlcat(fullpath, fontName_cpi, MAX_PATH_STRING);
                        strlcat(fullpath, "Regular.otf", MAX_PATH_STRING);

                        if (FT_New_Face(printer->FTlib, fullpath, 0, &printer->curFont)) {
                                fatal("Unable to load font %s\n", fullpath);
                                printer->curFont = NULL;
                        }
                }

                FT_Set_Char_Size(printer->curFont, (uint16_t)horizPoints * FREETYPE_FONT_MULTIPLIER,
                                 (uint16_t)vertPoints * FREETYPE_FONT_MULTIPLIER, printer->dpi, printer->dpi);
        } else
                fatal("Unknown print quality: %d\n", printer->printQuality);
}

bool processCommandChar(uint8_t ch, lpt_epsonprinter_t *printer) {
        if (printer->ESCSeen) {
                printer->ESCCmd = ch;
                printer->ESCSeen = false;
                printer->numParam = 0;

                switch (printer->ESCCmd) {
                case 0x02: // Undocumented
                case 0x0e: // Select double-width printing (one line) (ESC SO)
                case 0x0f: // Select condensed printing (ESC SI)
                case 0x1b: // Load/Eject (effect: rewind page) THIS IS NOT AN EPSON COMMAND! (ESC ESC)
                case 0x30: // Select 1/8-inch line spacing (ESC 0)
                case 0x31: // Select 7/72-inch line spacing (ESC 1)
                case 0x32: // Select 1/6-inch line spacing (ESC 2)
                case 0x34: // Select italic font (ESC 4)
                case 0x35: // Cancel italic font (ESC 5)
                case 0x36: // Enable printing of upper control codes (ESC 6)
                case 0x37: // Enable upper control codes (ESC 7)
                case 0x38: // Disable paper out detection (ESC 8)
                case 0x39: // Enable paper out detection (ESC 9)
                case 0x3c: // Unidirectional mode (one line) (ESC <)
                case 0x40: // Initialize printer (ESC @)
                case 0x45: // Select bold font (ESC E)
                case 0x46: // Cancel bold font (ESC F)
                case 0x47: // Select double-strike printing (ESC G)
                case 0x48: // Cancel double-strike printing (ESC H)
                case 0x4d: // Select 10.5-point, 12-cpi (ESC M)
                case 0x4f: // Cancel bottom margin
                case 0x50: // Select 10.5-point, 10-cpi (ESC P)
                case 0x54: // Cancel superscript/subscript printing (ESC T)
                case 0x73: // Select low-speed mode (ESC s)
                        printer->neededParam = 0;
                        break;
                case 0x19: // Control paper loading/ejecting (ESC EM)
                case 0x21: // Master select (ESC !)
                case 0x2d: // Turn underline on/off (ESC -)
                case 0x2f: // Select vertical tab channel (ESC /)
                case 0x33: // Set n/216-inch line spacing (ESC 3)
                case 0x41: // Set n/72-inch line spacing
                case 0x43: // Set page length in lines (ESC C)
                case 0x49: // Enable printing of control codes (ESC I n)
                case 0x4a: // Advance print position vertically (ESC J n)
                case 0x4e: // Set bottom margin (ESC N)
                case 0x51: // Set right margin (ESC Q)
                case 0x52: // Select an international character set (ESC R)
                case 0x53: // Select superscript/subscript printing (ESC S)
                case 0x55: // Turn unidirectional mode on/off (ESC U)
                case 0x57: // Turn double-width printing on/off (ESC W)
                case 0x61: // Select justification (ESC a)
                case 0x6b: // Select typeface (ESC k)
                case 0x6c: // Set left margin (ESC 1)
                case 0x70: // Turn proportional mode on/off (ESC p)
                case 0x74: // Select character table (ESC t)
                case 0x78: // Select NLQ or draft (ESC x)
                        printer->neededParam = 1;
                        break;
                case 0x3f: // Reassign bit-image mode (ESC ?)
                case 0x4b: // Select 60-dpi graphics (ESC K)
                case 0x4c: // Select 120-dpi graphics (ESC L)
                case 0x59: // Select 120-dpi, double-speed graphics (ESC Y)
                case 0x5a: // Select 240-dpi graphics (ESC Z)
                        printer->neededParam = 2;
                        break;
                case 0x2a: // Select bit image (ESC *)
                        printer->neededParam = 3;
                        break;
                case 0x62: // Set vertical tabs in VFU channels (ESC b)
                case 0x42: // Set vertical tabs (ESC B)
                        printer->numVertTabs = 0;
                        return true;
                case 0x44: // Set horizontal tabs (ESC D)
                        printer->numHorizTabs = 0;
                        return true;
                case 0x25: // Select user-defined set (ESC %)
                case 0x26: // Define user-defined characters (ESC &)
                case 0x3a: // Copy ROM to RAM (ESC :)
                        fatal("User-defined characters not supported!\n");
                        return true;
                case 0x28: // Two bytes sequence
                        return true;
                default:
                        fatal("PRINTER: Unknown command ESC %c (%02X). Unable to skip parameters.\n", printer->ESCCmd,
                              printer->ESCCmd);
                        printer->neededParam = 0;
                        printer->ESCCmd = 0;
                        return true;
                }

                if (printer->neededParam > 0)
                        return true;
        }

        // Ignore VFU channel setting
        if (printer->ESCCmd == 0x62) {
                // TODO: do not ignore this and ESC /?
                printer->ESCCmd = 0x42;
                return true;
        }

        // Collect vertical tabs
        if (printer->ESCCmd == 0x42) {
                if (ch == 0 || (printer->numVertTabs > 0 &&
                                printer->verttabs[printer->numVertTabs - 1] > (double)ch * printer->lineSpacing)) // Done
                        printer->ESCCmd = 0;
                else if (printer->numVertTabs < 16)
                        printer->verttabs[printer->numVertTabs++] = (double)ch * printer->lineSpacing;
        }

        // Collect horizontal tabs
        if (printer->ESCCmd == 0x44) {
                if (ch == 0 || (printer->numHorizTabs > 0 &&
                                printer->horiztabs[printer->numHorizTabs - 1] > (double)ch * (1 / (double)printer->cpi))) // Done
                        printer->ESCCmd = 0;
                else if (printer->numHorizTabs < 32)
                        printer->horiztabs[printer->numHorizTabs++] = (double)ch * (1 / (double)printer->cpi);
        }

        if (printer->numParam < printer->neededParam) {
                printer->params[printer->numParam++] = ch;

                if (printer->numParam < printer->neededParam)
                        return true;
        }

        if (printer->ESCCmd != 0) {
                switch (printer->ESCCmd) {
                case 0x02: // Undocumented
                        // Ignore
                        break;
                case 0x0e: // Select double-width printing (one line) (ESC SO)
                        printer->style |= STYLE_DOUBLEWIDTHONELINE;
                        updateFont(printer);
                        break;
                case 0x0f: // Select condensed printing (ESC SI)
                        printer->style |= STYLE_CONDENSED;
                        updateFont(printer);
                        break;
                case 0x19: // Control paper loading/ejecting (ESC EM)
                        // We are not really loading paper, so most commands can be ignored
                        if (printer->params[0] == 'R')
                                newPage(true, printer);
                        break;
                case 0x1b: // Load/Eject (effect: rewind page) THIS IS NOT AN EPSON COMMAND! (ESC ESC)
                        // TODO: check if this is the true behaviour when having continuous paper.
                        if (printer->style & STYLE_DOUBLEWIDTHONELINE) {
                                printer->style &= 0xFFFF - STYLE_DOUBLEWIDTHONELINE;
                                updateFont(printer);
                        }
                        printer->curX = printer->leftMargin;
                        printer->curY = printer->topMargin;
                        break;
                case 0x21: // Master select (ESC !)
                        printer->cpi = printer->params[0] & 0x01 ? 12 : 10;

                        // Reset first seven bits
                        printer->style &= 0xFF80;
                        if (printer->params[0] & 0x04)
                                printer->style |= STYLE_CONDENSED;
                        if (printer->params[0] & 0x08)
                                printer->style |= STYLE_BOLD;
                        if (printer->params[0] & 0x10)
                                printer->style |= STYLE_DOUBLESTRIKE;
                        if (printer->params[0] & 0x20)
                                printer->style |= STYLE_DOUBLEWIDTH;
                        if (printer->params[0] & 0x40)
                                printer->style |= STYLE_ITALICS;
                        if (printer->params[0] & 0x80)
                                printer->style |= STYLE_UNDERLINE;

                        updateFont(printer);
                        break;
                case 0x2a: // Select bit image (ESC *)
                        setupBitImage(printer->params[0], PARAM16(1), printer);
                        break;
                case 0x2d: // Turn underline on/off (ESC -)
                        if (printer->params[0] == 0 || printer->params[0] == 48)
                                printer->style &= 0xFFFF - STYLE_UNDERLINE;
                        if (printer->params[0] == 1 || printer->params[0] == 49)
                                printer->style |= STYLE_UNDERLINE;
                        updateFont(printer);
                        break;
                case 0x2f: // Select vertical tab channel (ESC /)
                        // Ignore
                        break;
                case 0x30: // Select 1/8-inch line spacing (ESC 0)
                        printer->lineSpacing = (double)1 / 8;
                        break;
                case 0x31: // Select 7/72-inch line spacing (ESC 1)
                        printer->lineSpacing = (double)7 / 72;
                        break;
                case 0x32: // Select 1/6-inch line spacing (ESC 2)
                        printer->lineSpacing = (double)1 / 6;
                        break;
                case 0x33: // Set n/216-inch line spacing (ESC 3)
                        printer->lineSpacing = (double)printer->params[0] / 216;
                        break;
                case 0x34: // Select italic font (ESC 4)
                        printer->style |= STYLE_ITALICS;
                        updateFont(printer);
                        break;
                case 0x35: // Cancel italic font (ESC 5)
                        printer->style &= 0xFFFF - STYLE_ITALICS;
                        updateFont(printer);
                        break;
                case 0x36: // Enable printing of upper control codes (ESC 6)
                        printer->printUpperContr = true;
                        break;
                case 0x37: // Enable upper control codes (ESC 7)
                        printer->printUpperContr = false;
                        break;
                case 0x38: // Disable paper out detection (ESC 8)
                        // We have infinite paper, so just ignore this
                        break;
                case 0x39: // Enable paper out detection (ESC 9)
                        // We have infinite paper, so just ignore this
                        break;
                case 0x3c: // Unidirectional mode (one line) (ESC <)
                        // We don't have a print head, so just ignore this
                        break;
                case 0x3f: // Reassign bit-image mode (ESC ?)
                        if (printer->params[0] == 75)
                                printer->densk = printer->params[1];
                        if (printer->params[0] == 76)
                                printer->densl = printer->params[1];
                        if (printer->params[0] == 89) // TODO: this option here is not present on the LX-810? (Even though the
                                                      // default mode for Y exists)
                                printer->densy = printer->params[1];
                        if (printer->params[0] == 90)
                                printer->densz = printer->params[1];
                        break;
                case 0x40: // Initialize printer (ESC @) TODO: one more command that should clear the current line buffer
                        resetPrinter(printer, true);
                        break;
                case 0x41: // Set n/72-inch line spacing
                        printer->lineSpacing = (double)printer->params[0] / 72;
                        break;
                case 0x43: // Set page length in lines (ESC C)
                        if (printer->params[0] != 0)
                                printer->pageHeight = printer->bottomMargin =
                                        (double)printer->params[0] *
                                        printer->lineSpacing; // TODO: is this right? Also, we have ALREADY allocated the SDL
                                                              // surface! This can result in memory corruption
                        else                                  // == 0 => Set page length in inches
                        {
                                printer->neededParam = 1;
                                printer->numParam = 0;
                                printer->ESCCmd = 0x100;
                                return true;
                        }
                        break;
                case 0x45: // Select bold font (ESC E)
                        printer->style |= STYLE_BOLD;
                        updateFont(printer);
                        break;
                case 0x46: // Cancel bold font (ESC F)
                        printer->style &= 0xFFFF - STYLE_BOLD;
                        updateFont(printer);
                        break;
                case 0x47: // Select dobule-strike printing (ESC G)
                        printer->style |= STYLE_DOUBLESTRIKE;
                        updateFont(printer);
                        break;
                case 0x48: // Cancel double-strike printing (ESC H)
                        printer->style &= 0xFFFF - STYLE_DOUBLESTRIKE;
                        updateFont(printer);
                        break;
                case 0x49: // Enable printing of control codes (ESC I n)
                        // TODO: which program/driver did this? Is it undocumented but SHOULD WORK on the LX-810?
                        pclog("EPSON ESC I: printing of control codes not implemented on the LX-810!\n");
                        break;
                case 0x4a: // Advance print position vertically (ESC J n)
                        printer->curY +=
                                (double)((double)printer->params[0] /
                                         216); // TODO: this command produces an immediate line feed without carriage return?
                        if (printer->curY > printer->bottomMargin)
                                newPage(true, printer);
                        break;
                case 0x4b: // Select 60-dpi graphics (ESC K)
                        setupBitImage(printer->densk, PARAM16(0), printer);
                        break;
                case 0x4c: // Select 120-dpi graphics (ESC L)
                        setupBitImage(printer->densl, PARAM16(0), printer);
                        break;
                case 0x4d: // Select 10.5-point, 12-cpi (ESC M)
                        printer->cpi = 12;
                        updateFont(printer);
                        break;
                case 0x4e: // Set bottom margin (ESC N) // TODO: does this work like this in the LX-810?
                        printer->topMargin = 0.0;
                        printer->bottomMargin = (double)printer->params[0] * printer->lineSpacing;
                        break;
                case 0x4f: // Cancel bottom (and top) margin // TODO: does this work like this in the LX-810?
                        printer->topMargin = 0.0;
                        printer->bottomMargin = printer->pageHeight;
                        break;
                case 0x50: // Select 10.5-point, 10-cpi (ESC P)
                        printer->cpi = 10;
                        updateFont(printer);
                        break;
                case 0x51: // Set right margin
                        // TODO: clear previous tab settings and all characters in the current line? Minimum space between the
                        // margins?
                        printer->rightMargin = (double)(printer->params[0] - 1.0) / (double)printer->cpi;
                        break;
                case 0x52: // Select an international character set (ESC R)
                        if (printer->params[0] <= 12) {
                                printer->curI18NCharset = printer->params[0];
                                updateCharset(printer);
                        }
                        break;
                case 0x53: // Select superscript/subscript printing (ESC S)
                        printer->style &= 0xFFFF - STYLE_SUPERSCRIPT - STYLE_SUBSCRIPT;
                        if (printer->params[0] == 0 || printer->params[0] == 48)
                                printer->style |= STYLE_SUPERSCRIPT;
                        if (printer->params[0] == 1 || printer->params[1] == 49)
                                printer->style |= STYLE_SUBSCRIPT;
                        updateFont(printer);
                        break;
                case 0x54: // Cancel superscript/subscript printing (ESC T)
                        printer->style &= 0xFFFF - STYLE_SUPERSCRIPT - STYLE_SUBSCRIPT;
                        updateFont(printer);
                        break;
                case 0x55: // Turn unidirectional mode on/off (ESC U)
                        // We don't have a print head, so just ignore this
                        break;
                case 0x57: // Turn double-width printing on/off (ESC W)
                        // TODO: see if this is right
                        if (printer->params[0] == 0 || printer->params[0] == 48)
                                printer->style &= 0xFFFF - STYLE_DOUBLEWIDTH;
                        if (printer->params[0] == 1 || printer->params[0] == 49)
                                printer->style |= STYLE_DOUBLEWIDTH;
                        updateFont(printer);
                        break;
                case 0x59: // Select 120-dpi, double-speed graphics (ESC Y)
                        setupBitImage(printer->densy, PARAM16(0), printer);
                        break;
                case 0x5a: // Select 240-dpi graphics (ESC Z)
                        setupBitImage(printer->densz, PARAM16(0), printer);
                        break;
                case 0x61: // Select justification (ESC a)
                        // TODO: do this?
                        break;
                case 0x6b: // Select typeface (ESC k)
                        if (printer->params[0] <= 11 || printer->params[0] == 30 || printer->params[0] == 31)
                                printer->NLQtypeFace = (Typeface)printer->params[0];
                        updateFont(printer);
                        break;
                case 0x6c: // Set left margin (ESC l)
                        // TODO: clear previous tab settings and all characters in the current line? Minimum space between the
                        // margins?
                        printer->leftMargin = (double)(printer->params[0] - 1.0) / (double)printer->cpi;
                        if (printer->curX < printer->leftMargin)
                                printer->curX = printer->leftMargin;
                        break;
                case 0x70: // Turn proportional mode on/off (ESC p)
                        // TODO: Win3.1 driver did this. Is it undocumented but SHOULD WORK on the LX-810?
                        pclog("EPSON ESC p: proportional mode not supported by the LX-810!\n");
                        break;
                case 0x73: // Select low-speed mode (ESC s)
                        // Ignore
                        break;
                case 0x74: // Select character table (ESC t)
                        if (printer->params[0] < 2)
                                printer->curCharTable = printer->params[0];
                        updateCharset(printer);
                        updateFont(printer);
                        break;
                case 0x78: // Select NLQ or draft (ESC x)
                        if (printer->params[0] == 0 || printer->params[0] == 48)
                                printer->printQuality = QUALITY_DRAFT;
                        if (printer->params[0] == 1 || printer->params[0] == 49)
                                printer->printQuality = QUALITY_NLQ;
                        break;
                case 0x100: // Set page length in inches (ESC C NUL)
                        printer->pageHeight = (double)printer->params[0];
                        printer->bottomMargin = printer->pageHeight; // TODO: is this right?
                        printer->topMargin = 0.0;
                        break;
                default:
                        pclog("PRINTER: Skipped unsupported command ESC %c (%02X)\n", printer->ESCCmd, printer->ESCCmd);
                }

                printer->ESCCmd = 0;
                return true;
        }

        // TODO: Set Tab Increments (ESC e)
        // TODO: Skip (ESC f)
        // TODO: Select 9-Pin Graphics Mode (ESC ^) (has two bytes per column)

        int control_code_shifted = false;
        if (!printer->printUpperContr) {
                if ((ch >= 0x80 && ch <= 0x9f) || ch == 0xff)
                        ch -= 0x80;
                control_code_shifted = true;
        }

        switch (ch) {
        case 0x07: // Beeper (BEL)
                // BEEEP!
                pclog("PRINTER: BEEP!\n");
                return true;
        case 0x08: // Backspace (BS)
        {
                // TODO: seems more complex than this
                double newX = printer->curX - (1 / (double)printer->actcpi);
                if (newX >= printer->leftMargin)
                        printer->curX = newX;
        }
                return true;
        case 0x09: // Tab horizontally (HT)
        {
                // Find tab right to current pos
                double moveTo = -1;
                for (uint8_t i = 0; i < printer->numHorizTabs; i++)
                        if (printer->horiztabs[i] > printer->curX)
                                moveTo = printer->horiztabs[i];
                // Nothing found => Ignore
                if (moveTo > 0 && moveTo < printer->rightMargin)
                        printer->curX = moveTo;
        }
                return true;
        case 0x0b:                             // Tab vertically (VT)
                if (printer->numVertTabs == 0) // All tabs cancelled => Act like CR
                        printer->curX = printer->leftMargin;
                else if (printer->numVertTabs == 255) // No tabs set since reset => Act like LF
                {
                        printer->curX = printer->leftMargin;
                        printer->curY += printer->lineSpacing;
                        if (printer->curY > printer->bottomMargin)
                                newPage(true, printer);
                } else {
                        // Find tab below current pos
                        double moveTo = -1;
                        for (uint8_t i = 0; i < printer->numVertTabs; i++)
                                if (printer->verttabs[i] > printer->curY)
                                        moveTo = printer->verttabs[i];

                        // Nothing found => Act like FF
                        if (moveTo > printer->bottomMargin || moveTo < 0)
                                newPage(true, printer);
                        else
                                printer->curY = moveTo;
                }
                if (printer->style & STYLE_DOUBLEWIDTHONELINE) {
                        printer->style &= 0xFFFF - STYLE_DOUBLEWIDTHONELINE;
                        updateFont(printer);
                }
                return true;
        case 0x0c: // Form feed (FF)
                if (printer->style & STYLE_DOUBLEWIDTHONELINE) {
                        printer->style &= 0xFFFF - STYLE_DOUBLEWIDTHONELINE;
                        updateFont(printer);
                }
                newPage(true, printer);
                return true;
        case 0x0d: // Carriage Return (CR)
                printer->curX = printer->leftMargin;
                if (!printer->autoFeed)
                        return true;
        case 0x0a: // Line feed
                if (printer->style & STYLE_DOUBLEWIDTHONELINE) {
                        printer->style &= 0xFFFF - STYLE_DOUBLEWIDTHONELINE;
                        updateFont(printer);
                }
                printer->curX = printer->leftMargin; // TODO: \n does this too or just \r?
                printer->curY += printer->lineSpacing;

                if (printer->curY > printer->bottomMargin)
                        newPage(true, printer);
                return true;
        case 0x0e: // Select double-width printing (one line) (SO)
                printer->style |= STYLE_DOUBLEWIDTHONELINE;
                updateFont(printer);
                return true;
        case 0x0f: // Select condensed printing (SI)
                printer->style |= STYLE_CONDENSED;
                updateFont(printer);
                return true;
        case 0x11: // Select printer (DC1)
                // Ignore
                return true;
        case 0x12: // Cancel condensed printing (DC2)
                printer->style &= 0xFFFF - STYLE_CONDENSED;
                updateFont(printer);
                return true;
        case 0x13: // Deselect printer (DC3)
                // Ignore
                return true;
        case 0x14: // Cancel double-width printing (one line) (DC4)
                printer->style &= 0xFFFF - STYLE_DOUBLEWIDTHONELINE;
                updateFont(printer);
                return true;
        case 0x18:           // Cancel line (CAN)
                return true; // TODO: is this used? should we cancel the line being printed?
        case 0x1b:           // ESC
                printer->ESCSeen = true;
                return true;
        case 0x7f:           // Delete character (DEL)
                return true; // TODO: is this used? should we cancel the character being printed?
        default:
                if (control_code_shifted) {
                        // TODO: see how this should really happen when the control code isn't processed (there are codepages with
                        // printable symbols < 0x20)
                        ch += 0x80;
                }
                return false;
        }

        return false;
}

void newPage(bool save, lpt_epsonprinter_t *printer) {
        if (save)
                outputPage(printer);

        printer->curY = printer->topMargin;

        SDL_Rect rect;
        rect.x = 0;
        rect.y = 0;
        rect.w = printer->page->w;
        rect.h = printer->page->h;
        SDL_FillRect(printer->page, &rect, SDL_MapRGB(printer->page->format, 255, 255, 255));
}

void printChar(lpt_epsonprinter_t *printer) {
        uint8_t ch = printer->last_data;
        printer->charRead = true;
        if (printer->page == NULL)
                return;

        // Are we currently printing a bit graphic?
        if (printer->bitGraph.remBytes > 0) {
                printBitGraph(ch, printer);
                return;
        }

        // See if it was a command
        if (processCommandChar(ch, printer))
                return;

        // Do not print if no font is available
        if (!printer->curFont)
                return;
        if (ch == 0)
                ch = 0x20;

        // TODO: real italics for the italics charset, where the upper half is (mostly) the lower half italicized.
        int italicize = false;
        if (!(printer->style & STYLE_ITALICS) && printer->charTables[printer->curCharTable] == 0 &&
            ch > 0x7f) // prevent double italics (they are present on the font)
        {
                italicize = true;
        }

        if (italicize) {
                FT_Matrix matrix;
                matrix.xx = 0x10000L;
                matrix.xy = (FT_Fixed)(0.20 * 0x10000L);
                matrix.yx = 0;
                matrix.yy = 0x10000L;
                FT_Set_Transform(printer->curFont, &matrix, NULL);
        } else
                FT_Set_Transform(printer->curFont, NULL, NULL);

        // Find the glyph for the char to render
        FT_UInt index = FT_Get_Char_Index(printer->curFont, printer->curMap[ch]);

        // Load the glyph
        FT_Load_Glyph(printer->curFont, index, FT_LOAD_DEFAULT);

        // Render a high-quality bitmap
        FT_Render_Glyph(printer->curFont->glyph, FT_RENDER_MODE_NORMAL);

        // TODO: some fonts have negative left and/or top and as of this writing we don't do printable areas! So do this hack on
        // unsigned ints...
        uint16_t penX = CURX_INCH_TO_PIXEL_ADD(fmax(0.0, printer->curFont->glyph->bitmap_left));
        uint16_t penY = CURY_INCH_TO_PIXEL_ADD(fmax(
                0.0, -printer->curFont->glyph->bitmap_top + printer->curFont->size->metrics.ascender / FREETYPE_FONT_MULTIPLIER));
        // handle proportional fonts
        penX += (((double)printer->dpi / printer->actcpi) - printer->curFont->glyph->bitmap.width) / 2;

        if (printer->style & STYLE_SUBSCRIPT) // TODO: this and other placementes are guesswork
                penY += printer->curFont->glyph->metrics.vertAdvance / FREETYPE_FONT_MULTIPLIER * (double)1 / (double)3;

        // Copy bitmap into page
        SDL_LockSurface(printer->page);

        blitGlyph(printer->curFont->glyph->bitmap, penX, penY, false, printer);

        // Doublestrike => Print the glyph a second time one pixel below
        if (printer->printQuality != QUALITY_DRAFT) // TODO: have the font variations for the NLQ fonts too
                if (printer->style & STYLE_DOUBLESTRIKE)
                        blitGlyph(printer->curFont->glyph->bitmap, penX, penY + 1, true, printer);

        // Bold => Print the glyph a second time one pixel to the right
        if (printer->printQuality != QUALITY_DRAFT) // TODO: have the font variations for the NLQ fonts too
                if (printer->style & STYLE_BOLD)
                        blitGlyph(printer->curFont->glyph->bitmap, penX + 1, penY, true, printer);

        SDL_UnlockSurface(printer->page);

        // For line printing
        uint16_t lineStart = CURX_INCH_TO_PIXEL;

        printer->curX += 1 / (double)printer->actcpi;

        // Draw lines if desired
        if (printer->printQuality != QUALITY_DRAFT) // TODO: have the font variations for the NLQ fonts too
                if (printer->style & STYLE_UNDERLINE) {
                        // Find out where to put the line
                        uint16_t lineY =
                                CURY_INCH_TO_PIXEL_ADD(printer->curFont->size->metrics.height / FREETYPE_FONT_MULTIPLIER);

                        // TODO: use previous CURX_INCH_TO_PIXEL or penX here?
                        drawLine(lineStart, CURX_INCH_TO_PIXEL, lineY, printer);
                }
}

void blitGlyph(FT_Bitmap bitmap, uint16_t destx, uint16_t desty, bool add, lpt_epsonprinter_t *printer) {
        for (unsigned int y = 0; y < bitmap.rows; y++) {
                for (unsigned int x = 0; x < bitmap.width; x++) {
                        // Read pixel from glyph bitmap
                        uint8_t *source = bitmap.buffer + x + y * bitmap.pitch;

                        // Ignore background and don't go over the border
                        // TODO: This should not be linear!
                        if (*source != 0 && (destx + x < (unsigned int)printer->page->w) &&
                            (desty + y < (unsigned int)printer->page->h)) {
                                uint8_t *target =
                                        (uint8_t *)printer->page->pixels + (x + destx) + (y + desty) * printer->page->pitch;
                                if (add) {
                                        if (*target + *source > 255)
                                                *target = 255;
                                        else
                                                *target += *source;
                                } else
                                        *target = *source;
                        }
                }
        }
}

void drawLine(uint16_t fromx, uint16_t tox, uint16_t y, lpt_epsonprinter_t *printer) {
        SDL_LockSurface(printer->page);

        // Draw anti-aliased line
        for (unsigned int x = fromx; x <= tox; x++) {
                // Skip parts if going over the border
                if (x < (unsigned int)printer->page->w) {
                        if (y > 0 && (y - 1) < printer->page->h)
                                *((uint8_t *)printer->page->pixels + x + (y - 1) * printer->page->pitch) = 120;
                        if (y < printer->page->h)
                                *((uint8_t *)printer->page->pixels + x + y * printer->page->pitch) = 255;
                        if (y + 1 < printer->page->h)
                                *((uint8_t *)printer->page->pixels + x + (y + 1) * printer->page->pitch) = 120;
                }
        }

        SDL_UnlockSurface(printer->page);
}

void setAutofeed(bool feed, lpt_epsonprinter_t *printer) { printer->autoFeed = feed; }

bool getAutofeed(lpt_epsonprinter_t *printer) { return printer->autoFeed; }

bool isBusy(lpt_epsonprinter_t *printer) {
        // We're never busy
        return false;
}

bool ack(lpt_epsonprinter_t *printer) {
        // Acknowledge last char read
        return printer->charRead;
}

// TODO: see if densities are OK
// TODO: 240x144 setting under windows 95 looks bad when printing bitmap fonts because of the "no adjancent dots" rule? Shouldn't
// it look a little better?
void setupBitImage(uint8_t dens, uint16_t numCols, lpt_epsonprinter_t *printer) {
        switch (dens) {
        case 0: // Single-density
                printer->bitGraph.horizDens = 60;
                printer->bitGraph.vertDens = 72;
                printer->bitGraph.adjacent = true;
                printer->bitGraph.bytesColumn = 1;
                break;
        case 1: // Double-density
                printer->bitGraph.horizDens = 120;
                printer->bitGraph.vertDens = 72;
                printer->bitGraph.adjacent = true;
                printer->bitGraph.bytesColumn = 1;
                break;
        case 2: // High-speed double-density
                printer->bitGraph.horizDens = 120;
                printer->bitGraph.vertDens = 72;
                printer->bitGraph.adjacent = false;
                printer->bitGraph.bytesColumn = 1;
                break;
        case 3: // Quadruple-density
                printer->bitGraph.horizDens = 240;
                printer->bitGraph.vertDens = 72;
                printer->bitGraph.adjacent = false;
                printer->bitGraph.bytesColumn = 1;
                break;
        case 4: // CRT I
                printer->bitGraph.horizDens = 80;
                printer->bitGraph.vertDens = 72;
                printer->bitGraph.adjacent = true;
                printer->bitGraph.bytesColumn = 1;
                break;
        case 5: // Plotter (1:1
                printer->bitGraph.horizDens = 72;
                printer->bitGraph.vertDens = 72;
                printer->bitGraph.adjacent = true;
                printer->bitGraph.bytesColumn = 1;
                break;
        case 6: // CRT II
                printer->bitGraph.horizDens = 90;
                printer->bitGraph.vertDens = 72;
                printer->bitGraph.adjacent = true;
                printer->bitGraph.bytesColumn = 1;
                break;
        default:
                fatal("PRINTER: Unsupported bit image density %i\n", dens);
        }

        printer->bitGraph.remBytes = numCols * printer->bitGraph.bytesColumn;
        printer->bitGraph.readBytesColumn = 0;
}

// TODO: anti-alias because of high dpi, also be mindful that additions when superimposing pin strikes are not linear
// TODO: use this for printing when we have the character roms
void printBitGraph(uint8_t ch, lpt_epsonprinter_t *printer) {
        printer->bitGraph.column[printer->bitGraph.readBytesColumn++] = ch;
        printer->bitGraph.remBytes--;

        // Only print after reading a full column
        if (printer->bitGraph.readBytesColumn < printer->bitGraph.bytesColumn)
                return;

        double oldY = printer->curY;

        SDL_LockSurface(printer->page);

        // TODO: other rasterizations where we should do start->end instead of start + steps
        double pixsizeX;
        double pixsizeY;
        if (printer->emulatePinsForBitmaps) {
                pixsizeX = 0.01141732283464566929 * printer->dpi;
                pixsizeY = 0.01141732283464566929 * printer->dpi;
        } else {
                pixsizeX = printer->dpi / printer->bitGraph.horizDens > 0 ? printer->dpi / printer->bitGraph.horizDens : 1;
                pixsizeY = printer->dpi / printer->bitGraph.vertDens > 0 ? printer->dpi / printer->bitGraph.vertDens : 1;
        }

        for (unsigned int i = 0; i < printer->bitGraph.bytesColumn; i++) {
                for (int j = 7; j >= 0; j--) {
                        uint8_t pixel = (printer->bitGraph.column[i] >> j) & 0x01;

                        if (pixel != 0) {
                                int startx;
                                int endx;
                                int starty;
                                int endy;

                                if (printer->emulatePinsForBitmaps) {
                                        startx = CURX_INCH_TO_PIXEL_ADD(-pixsizeX);
                                        endx = CURX_INCH_TO_PIXEL_ADD(pixsizeX * 2);
                                        starty = CURY_INCH_TO_PIXEL_ADD(-pixsizeY);
                                        endy = CURY_INCH_TO_PIXEL_ADD(pixsizeY * 2);
                                } else {
                                        startx = CURX_INCH_TO_PIXEL;
                                        endx = CURX_INCH_TO_PIXEL_ADD(pixsizeX);
                                        starty = CURY_INCH_TO_PIXEL;
                                        endy = CURY_INCH_TO_PIXEL_ADD(pixsizeY);
                                }

                                for (int xx = startx; xx < endx; xx++)
                                        for (int yy = starty; yy < endy; yy++)
                                                if ((xx < printer->page->w) && (yy < printer->page->h)) {
                                                        if (printer->emulatePinsForBitmaps) {
                                                                double middlex = CURX_INCH_TO_PIXEL_ADD(pixsizeX / 2);
                                                                double middley = CURY_INCH_TO_PIXEL_ADD(pixsizeY / 2);
                                                                double dist = sqrt((xx - middlex) * (xx - middlex) +
                                                                                   (yy - middley) * (yy - middley));
                                                                double radius = pixsizeY / 2;
                                                                double ratio = dist / radius;
                                                                // pin diameter == 0.29mm == 0.01141732283464566929 inches. 0.35mm
                                                                // or 1/72 inch between pins.
                                                                uint8_t source = (ratio <= 1.0) ? (255 * (1 - ratio * ratio)) : 0;
                                                                // uint8_t source = (ratio <= 1.0) ? (255 * (1 - ratio)) : 0;
                                                                // uint8_t source = (ratio <= 1.0) ? 255 : 0;
                                                                uint8_t *target = (uint8_t *)printer->page->pixels + xx +
                                                                                  yy * printer->page->pitch;
                                                                if ((int)*target + (int)source > 255)
                                                                        *target = 255;
                                                                else
                                                                        *target += source;
                                                        } else
                                                                *((uint8_t *)printer->page->pixels + xx +
                                                                  yy * printer->page->pitch) = 255;
                                                }
                        }

                        printer->curY += (double)1 / (double)printer->bitGraph.vertDens;
                }
        }

        SDL_UnlockSurface(printer->page);

        printer->curY = oldY;

        printer->bitGraph.readBytesColumn = 0;

        // Advance the cursor
        printer->curX += (double)1 / (double)printer->bitGraph.horizDens;
}

void formFeed(lpt_epsonprinter_t *printer) {
        // Don't output blank pages
        newPage(!isBlank(printer), printer);

        finishMultipage(printer);
}

static void findNextName(const char *front, const char *ext, char *filename, size_t filename_size, const char *base_path) {
        char buffer[10];
        uint16_t num = 1;
        FILE *test = NULL;
        do {
                snprintf(&buffer[0], 10, "%d", num++);
                strlcpy(filename, base_path, filename_size);
                strlcat(filename, front, filename_size);
                strlcat(filename, buffer, filename_size);
                strlcat(filename, ext, filename_size);

                test = fopen(filename, "rb");
                if (test != NULL)
                        fclose(test);
                if (num == 999999999)
                        fatal("Too many printed pages! Please make space!\n");
        } while (test != NULL);
}

void outputPage(lpt_epsonprinter_t *printer) {
        char filename[MAX_PATH_STRING];

        if (printer->output_type == OUTPUT_TYPE_FORWARD_TO_REAL_PRINTER) {
#if defined(WIN32)
                // You'll need the mouse for the print dialog
                //if (mouselocked)
                //        CaptureMouse();

                uint16_t physW = GetDeviceCaps(printer->printerDC, PHYSICALWIDTH);
                uint16_t physH = GetDeviceCaps(printer->printerDC, PHYSICALHEIGHT);

                double scaleW, scaleH;

                if (printer->page->w > physW)
                        scaleW = (double)printer->page->w / (double)physW;
                else
                        scaleW = (double)physW / (double)printer->page->w;

                if (printer->page->h > physH)
                        scaleH = (double)printer->page->h / (double)physH;
                else
                        scaleH = (double)physH / (double)printer->page->h;

                HDC memHDC = CreateCompatibleDC(printer->printerDC);
                HBITMAP bitmap = CreateCompatibleBitmap(memHDC, printer->page->w, printer->page->h);
                SelectObject(memHDC, bitmap);

                // Start new printer job?
                if (printer->outputHandle == NULL) {
                        DOCINFO docinfo;
                        docinfo.cbSize = sizeof(docinfo);
                        docinfo.lpszDocName = "PCem Printer";
                        docinfo.lpszOutput = NULL;
                        docinfo.lpszDatatype = NULL;
                        docinfo.fwType = 0;

                        StartDoc(printer->printerDC, &docinfo);
                        printer->multiPageCounter = 1;
                }

                StartPage(printer->printerDC);
                SDL_LockSurface(printer->page);

                SDL_Palette *sdlpal = printer->page->format->palette;

                for (uint16_t y = 0; y < printer->page->h; y++) {
                        for (uint16_t x = 0; x < printer->page->w; x++) {
                                uint8_t pixel = *((uint8_t *)printer->page->pixels + x + (y * printer->page->pitch));
                                uint32_t color = 0;
                                color |= sdlpal->colors[pixel].r;
                                color |= ((uint32_t)sdlpal->colors[pixel].g) << 8;
                                color |= ((uint32_t)sdlpal->colors[pixel].b) << 16;
                                SetPixel(memHDC, x, y, color);
                        }
                }

                SDL_UnlockSurface(printer->page);

                StretchBlt(printer->printerDC, 0, 0, physW, physH, memHDC, 0, 0, printer->page->w, printer->page->h, SRCCOPY);

                EndPage(printer->printerDC);

                if (printer->multipageOutput) {
                        printer->multiPageCounter++;
                        printer->outputHandle = printer->printerDC;
                } else {
                        EndDoc(printer->printerDC);
                        printer->outputHandle = NULL;
                }

                DeleteDC(memHDC);
#else
                fatal("PRINTER: Direct printing not supported under this OS\n");
#endif
        } else if (printer->output_type == OUTPUT_TYPE_POSTSCRIPT) {
                // TODO: this was working even when the last entry on the palette was undefined - see if there is any workaround
                // in this code
                FILE *psfile = NULL;

                // Continue postscript file?
                if (printer->outputHandle != NULL)
                        psfile = (FILE *)printer->outputHandle;

                // Create new file?
                if (psfile == NULL) {
                        if (!printer->multipageOutput)
                                findNextName("page", ".ps", filename, MAX_PATH_STRING, printer->outputpath);
                        else
                                findNextName("doc", ".ps", filename, MAX_PATH_STRING, printer->outputpath);

                        psfile = fopen(filename, "wb");
                        if (!psfile) {
                                fatal("PRINTER: Can't open file %s for printer output\n", filename);
                                return;
                        }

                        // Print header
                        fprintf(psfile, "%%!PS-Adobe-3.0\n");
                        fprintf(psfile, "%%%%Pages: (atend)\n");
                        fprintf(psfile, "%%%%BoundingBox: 0 0 %i %i\n", (uint16_t)(printer->defaultPageWidth * printer->dpi),
                                (uint16_t)(printer->defaultPageHeight * printer->dpi)); // TODO: use current height here?
                        fprintf(psfile, "%%%%Creator: PCem Virtual Printer\n");
                        fprintf(psfile, "%%%%DocumentData: Clean7Bit\n");
                        fprintf(psfile, "%%%%LanguageLevel: 2\n");
                        fprintf(psfile, "%%%%EndComments\n");
                        printer->multiPageCounter = 1;
                }

                fprintf(psfile, "%%%%Page: %i %i\n", printer->multiPageCounter, printer->multiPageCounter);
                fprintf(psfile, "%i %i scale\n", (uint16_t)(printer->defaultPageWidth * printer->dpi),
                        (uint16_t)(printer->defaultPageHeight * printer->dpi)); // TODO: use current height here?
                fprintf(psfile, "%i %i 8 [%i 0 0 -%i 0 %i]\n", printer->page->w, printer->page->h, printer->page->w,
                        printer->page->h, printer->page->h);
                fprintf(psfile, "currentfile\n");
                fprintf(psfile, "/ASCII85Decode filter\n");
                fprintf(psfile, "/RunLengthDecode filter\n");
                fprintf(psfile, "image\n");

                SDL_LockSurface(printer->page);

                uint32_t pix = 0;
                uint32_t numpix = printer->page->h * printer->page->w;
                printer->ASCII85BufferPos = printer->ASCII85CurCol = 0;

                while (pix < numpix) {
                        // Compress data using RLE
                        if ((pix < numpix - 2) && (getPixel(pix, printer) == getPixel(pix + 1, printer)) &&
                            (getPixel(pix, printer) == getPixel(pix + 2, printer))) {
                                // Found three or more pixels with the same color
                                uint8_t sameCount = 3;
                                uint8_t col = getPixel(pix, printer);
                                while (sameCount < 128 && sameCount + pix < numpix && col == getPixel(pix + sameCount, printer))
                                        sameCount++;

                                fprintASCII85(psfile, 257 - sameCount, printer);
                                fprintASCII85(psfile, 255 - col, printer);

                                // Skip ahead
                                pix += sameCount;
                        } else {
                                // Find end of heterogenous area
                                uint8_t diffCount = 1;
                                while (diffCount < 128 && diffCount + pix < numpix &&
                                       ((diffCount + pix < numpix - 2) ||
                                        (getPixel(pix + diffCount, printer) != getPixel(pix + diffCount + 1, printer)) ||
                                        (getPixel(pix + diffCount, printer) != getPixel(pix + diffCount + 2, printer))))
                                        diffCount++;

                                fprintASCII85(psfile, diffCount - 1, printer);
                                for (uint8_t i = 0; i < diffCount; i++)
                                        fprintASCII85(psfile, 255 - getPixel(pix++, printer), printer);
                        }
                }

                // Write EOD for RLE and ASCII85
                fprintASCII85(psfile, 128, printer);
                fprintASCII85(psfile, 256, printer);

                SDL_UnlockSurface(printer->page);

                fprintf(psfile, "showpage\n");

                if (printer->multipageOutput) {
                        printer->multiPageCounter++;
                        printer->outputHandle = psfile;
                } else {
                        fprintf(psfile, "%%%%Pages: 1\n");
                        fprintf(psfile, "%%%%EOF\n");
                        fclose(psfile);
                        printer->outputHandle = NULL;
                }
        } else {
                const char *ext;
                const char *output_format;
                switch (printer->output_type) {
                case OUTPUT_TYPE_BMP:
                        ext = ".bmp";
                        output_format = IMAGE_BMP;
                        break;
                case OUTPUT_TYPE_PNG:
                        ext = ".png";
                        output_format = IMAGE_PNG;
                        break;
                case OUTPUT_TYPE_TIFF:
                        ext = ".tiff";
                        output_format = IMAGE_TIFF;
                        break;
                case OUTPUT_TYPE_JPG:
                        ext = ".jpg";
                        output_format = IMAGE_JPG;
                        break;
                default:
                        fatal("PRINTER: Unknown output type: %d\n", printer->output_type);
                }

                // Find a page that does not exists
                findNextName("page", ext, filename, MAX_PATH_STRING, printer->outputpath);

                char *rgb = malloc(printer->page->w * printer->page->h * 3);
                char *rgbptr = rgb;

                SDL_LockSurface(printer->page);

                SDL_Palette *sdlpal = printer->page->format->palette;

                for (int y = 0; y < printer->page->h; y++) {
                        for (int x = 0; x < printer->page->w; x++) {
                                uint8_t pixel = *((uint8_t *)printer->page->pixels + x + (y * printer->page->pitch));

                                *rgbptr++ = sdlpal->colors[pixel].r;
                                *rgbptr++ = sdlpal->colors[pixel].g;
                                *rgbptr++ = sdlpal->colors[pixel].b;
                        }
                }

                SDL_UnlockSurface(printer->page);

                // TODO: save as 8bpp instead of converting to rgb
                // TODO: image will be copied twice (here and in the function below)
                wx_image_save_fullpath(filename, output_format, rgb, printer->page->w, printer->page->h, false);
                free(rgb);
        }
}

void fprintASCII85(FILE *f, uint16_t b, lpt_epsonprinter_t *printer) {
        if (b != 256) {
                if (b < 256)
                        printer->ASCII85Buffer[printer->ASCII85BufferPos++] = (uint8_t)b;

                if (printer->ASCII85BufferPos == 4 || b == 257) {
                        uint32_t num = (uint32_t)printer->ASCII85Buffer[0] << 24 | (uint32_t)printer->ASCII85Buffer[1] << 16 |
                                       (uint32_t)printer->ASCII85Buffer[2] << 8 | (uint32_t)printer->ASCII85Buffer[3];

                        // Deal with special case
                        if (num == 0 && b != 257) {
                                fprintf(f, "z");
                                if (++printer->ASCII85CurCol >= 79) {
                                        printer->ASCII85CurCol = 0;
                                        fprintf(f, "\n");
                                }
                        } else {
                                char buffer[5];
                                for (int8_t i = 4; i >= 0; i--) {
                                        buffer[i] = (uint8_t)((uint32_t)num % (uint32_t)85);
                                        buffer[i] += 33;
                                        num /= (uint32_t)85;
                                }

                                // Make sure a line never starts with a % (which may be mistaken as start of a comment)
                                if (printer->ASCII85CurCol == 0 && buffer[0] == '%')
                                        fprintf(f, " ");

                                for (int i = 0; i < ((b != 257) ? 5 : printer->ASCII85BufferPos + 1); i++) {
                                        fprintf(f, "%c", buffer[i]);
                                        if (++printer->ASCII85CurCol >= 79) {
                                                printer->ASCII85CurCol = 0;
                                                fprintf(f, "\n");
                                        }
                                }
                        }

                        printer->ASCII85BufferPos = 0;
                }

        } else // Close string
        {
                // Partial tupel if there are still bytes in the buffer
                if (printer->ASCII85BufferPos > 0) {
                        for (uint8_t i = printer->ASCII85BufferPos; i < 4; i++)
                                printer->ASCII85Buffer[i] = 0;

                        fprintASCII85(f, 257, printer);
                }

                fprintf(f, "~");
                fprintf(f, ">\n");
        }
}

void finishMultipage(lpt_epsonprinter_t *printer) {
        if (printer->outputHandle != NULL) {
                if (printer->output_type == OUTPUT_TYPE_POSTSCRIPT) {
                        FILE *psfile = (FILE *)printer->outputHandle;
                        fprintf(psfile, "%%%%Pages: %i\n", printer->multiPageCounter);
                        fprintf(psfile, "%%%%EOF\n");
                        fclose(psfile);
                } else if (printer->output_type == OUTPUT_TYPE_FORWARD_TO_REAL_PRINTER) {
#if defined(WIN32)
                        EndDoc(printer->printerDC);
#endif
                }
                printer->outputHandle = NULL;
        }
}

bool isBlank(lpt_epsonprinter_t *printer) {
        bool blank = true;

        SDL_LockSurface(printer->page);

        for (uint16_t y = 0; y < printer->page->h; y++)
                for (uint16_t x = 0; x < printer->page->w; x++)
                        if (*((uint8_t *)printer->page->pixels + x + (y * printer->page->pitch)) != 0)
                                blank = false;

        SDL_UnlockSurface(printer->page);

        return blank;
}

uint8_t getPixel(uint32_t num, lpt_epsonprinter_t *printer) {
        // Respect the pitch
        return *((uint8_t *)printer->page->pixels + (num % printer->page->w) + ((num / printer->page->w) * printer->page->pitch));
}

static void epsonprinter_write_data(uint8_t val, void *p) {
        lpt_epsonprinter_t *lpt_epsonprinter = (lpt_epsonprinter_t *)p;
#ifdef PRINTER_DEBUG
        pclog("lx-810 write data %02X\n", val);
#endif
        lpt_epsonprinter->last_data = val;
}

static void epsonprinter_write_ctrl(uint8_t val, void *p) {
        lpt_epsonprinter_t *lpt_epsonprinter = (lpt_epsonprinter_t *)p;
#ifdef PRINTER_DEBUG
        pclog("lx-810 write control %02X\n", val);
#endif

        setAutofeed((val & 0x02) != 0, lpt_epsonprinter);
        if ((val & 0x01) && (!(lpt_epsonprinter->controlreg & 0x01)))
                printChar(lpt_epsonprinter);
        if ((val & 0x04) && (!(lpt_epsonprinter->controlreg & 0x04)))
                resetPrinterHard(lpt_epsonprinter);

        lpt_epsonprinter->controlreg = val;
}

static uint8_t epsonprinter_read_status(void *p) {
        lpt_epsonprinter_t *lpt_epsonprinter = (lpt_epsonprinter_t *)p;
        uint8_t temp;

        // Printer is always online and never reports an error
        temp = 0x1f; // 0x18;

        //        if (lpt_epsonprinter->controlreg & 0x08 == 0)
        //                temp |= 0x10;

        if (!isBusy(lpt_epsonprinter))
                temp |= 0x80;

        if (!ack(lpt_epsonprinter))
                temp |= 0x40;
#ifdef PRINTER_DEBUG
        pclog("lx-810 read status %02X\n", temp);
#endif
        return temp;
}

static uint8_t epsonprinter_read_ctrl(void *p) {
        lpt_epsonprinter_t *lpt_epsonprinter = (lpt_epsonprinter_t *)p;
        uint8_t temp = 0xe0 | (getAutofeed(lpt_epsonprinter) ? 0x02 : 0x00) | (lpt_epsonprinter->controlreg & 0xfd);
#ifdef PRINTER_DEBUG
        pclog("lx-810 read control %02X\n", temp);
#endif

        return temp;
}

static void *epsonprinter_init() {
        lpt_epsonprinter_t *lpt_epsonprinter = (lpt_epsonprinter_t *)malloc(sizeof(lpt_epsonprinter_t));
        memset(lpt_epsonprinter, 0, sizeof(lpt_epsonprinter_t));

        lpt_epsonprinter->controlreg = 0x04;

        // TODO: printable area? (make it an option because the user may want to REALLY print the results and we don't want to add
        // two forced margins?)
        // TODO: also follow all mode overrides (relation of dip switches, front panel, software cmds) to the letter from the user
        // manual
        // TODO: see if defaults are right everywhere
        // TODO: possible overflows in 8-bit and 16-bit ints when doing high dpis
        // TODO: auto-form feed after inactivity option
        // TODO: ink intensity (high speed draft, dry ribbon, etc)
        // TODO: simulate errors and out of paper just for fun?
        // TODO: I've seen a postscript non-multipage in win95/word6 come out blank once. Couldn't repeat
        // width and height unit is 1/10 inch, multipageoutput == keep appending on postscript or print job until manual formfeed
        int dpi = device_get_config_int("dpi");
        int output_type = device_get_config_int("outputtype");
        int multipage = device_get_config_int("multipage");
        int selected_papersize = device_get_config_int("papersize");
        int width;
        int height;
        switch (selected_papersize) {
        case 0:
                width = 85;
                height = 110;
                break;
        case 1:
                // TODO: should we care about this precision loss?
                width = 210 / 254;
                height = 297 / 254;
                break;
        default:
                fatal("PRINTER: Invalid paper size\n");
        }
        int cpi = device_get_config_int("cpi");
        int selected_fontstyle = device_get_config_int("font");
        uint16_t style;
        uint8_t quality;
        Typeface font;
        // TODO: we are just setting the default NLQ font to Roman when starting with draft, see if this is right.
        switch (selected_fontstyle) {
        case 0:
                style = 0;
                quality = QUALITY_DRAFT;
                font = roman;
                break;
        case 1:
                style = STYLE_CONDENSED;
                quality = QUALITY_DRAFT;
                font = roman;
                break;
        case 2:
                style = 0;
                quality = QUALITY_NLQ;
                font = roman;
                break;
        case 3:
                style = 0;
                quality = QUALITY_NLQ;
                font = sansserif;
                break;
        default:
                fatal("PRINTER: Invalid initial font setting: %d\n", selected_fontstyle);
        }
        int selected_charset = device_get_config_int("characterset");
        uint8_t chartable, i18ncharset;
        uint16_t codepage;
        switch (selected_charset) {
        case 0:
                chartable = 0;
                i18ncharset = 0;
                codepage = 437;
                break;
        case 1:
                chartable = 1;
                i18ncharset = 0;
                codepage = 437;
                break;
        case 2:
                chartable = 1;
                i18ncharset = 1;
                codepage = 437;
                break;
        case 3:
                chartable = 1;
                i18ncharset = 2;
                codepage = 437;
                break;
        case 4:
                chartable = 1;
                i18ncharset = 3;
                codepage = 437;
                break;
        case 5:
                chartable = 1;
                i18ncharset = 4;
                codepage = 437;
                break;
        case 6:
                chartable = 1;
                i18ncharset = 5;
                codepage = 437;
                break;
        case 7:
                chartable = 1;
                i18ncharset = 6;
                codepage = 437;
                break;
        case 8:
                chartable = 1;
                i18ncharset = 7;
                codepage = 437;
                break;
        case 9:
                chartable = 1;
                i18ncharset = 8;
                codepage = 437;
                break;
        case 10:
                chartable = 1;
                i18ncharset = 9;
                codepage = 437;
                break;
        case 11:
                chartable = 1;
                i18ncharset = 10;
                codepage = 437;
                break;
        case 12:
                chartable = 1;
                i18ncharset = 11;
                codepage = 437;
                break;
        case 13:
                chartable = 1;
                i18ncharset = 12;
                codepage = 437;
                break;
        case 14:
                chartable = 1;
                i18ncharset = 0;
                codepage = 850;
                break;
        case 15:
                chartable = 1;
                i18ncharset = 0;
                codepage = 860;
                break;
        case 16:
                chartable = 1;
                i18ncharset = 0;
                codepage = 863;
                break;
        case 17:
                chartable = 1;
                i18ncharset = 0;
                codepage = 865;
                break;
        default:
                fatal("PRINTER: Invalid initial charset setting: %d\n", selected_charset);
        }
        int emulatepins = device_get_config_int("bitmaps_emulate_pins");

        char s[512];
        strlcpy(lpt_epsonprinter->fontpath, printer_path, MAX_PATH_STRING - 2);
        put_backslash(lpt_epsonprinter->fontpath);

        strlcpy(lpt_epsonprinter->outputpath, printer_path, MAX_PATH_STRING - 2);
        put_backslash(lpt_epsonprinter->outputpath);

        if (FT_Init_FreeType(&lpt_epsonprinter->FTlib)) {
                fatal("PRINTER: Unable to init Freetype2. Printing disabled\n");
                lpt_epsonprinter->page = NULL;
        } else {
                lpt_epsonprinter->dpi = dpi;
                lpt_epsonprinter->output_type = output_type;
                lpt_epsonprinter->multipageOutput = multipage;

                lpt_epsonprinter->defaultPageWidth = (double)width / (double)10;
                lpt_epsonprinter->defaultPageHeight = (double)height / (double)10;

                lpt_epsonprinter->defaultCPI = cpi;
                lpt_epsonprinter->defaultStyle = style;
                lpt_epsonprinter->defaultQuality = quality;
                lpt_epsonprinter->defaultNLQtypeFace = font;
                lpt_epsonprinter->defaultCharTable = chartable;
                lpt_epsonprinter->defaultI18NCharset = i18ncharset;
                lpt_epsonprinter->defaultCodePage = codepage;
                lpt_epsonprinter->emulatePinsForBitmaps = emulatepins;

                // Create page
                lpt_epsonprinter->page = SDL_CreateRGBSurface(
                        0, (unsigned int)(lpt_epsonprinter->defaultPageWidth * lpt_epsonprinter->dpi),
                        (unsigned int)(lpt_epsonprinter->defaultPageHeight * lpt_epsonprinter->dpi), 8, 0, 0, 0, 0);

                // Set a grey palette
                SDL_Palette *palette = lpt_epsonprinter->page->format->palette;

                for (unsigned int i = 0; i < 256; i++)
                        palette->colors[i].r = palette->colors[i].g = palette->colors[i].b = 255 - i;

                lpt_epsonprinter->curFont = NULL;
                lpt_epsonprinter->autoFeed = false;
                lpt_epsonprinter->outputHandle = NULL;

                resetPrinterHard(lpt_epsonprinter);

                if (lpt_epsonprinter->output_type == OUTPUT_TYPE_FORWARD_TO_REAL_PRINTER) {
#if defined(WIN32)
                        // TODO: this looks like a hack
                        // Show Print dialog to obtain a printer device context

                        PRINTDLG pd;
                        pd.lStructSize = sizeof(PRINTDLG);
                        pd.hDevMode = (HANDLE)NULL;
                        pd.hDevNames = (HANDLE)NULL;
                        pd.Flags = PD_RETURNDC;
                        pd.hwndOwner = NULL;
                        pd.hDC = (HDC)NULL;
                        pd.nFromPage = 1;
                        pd.nToPage = 1;
                        pd.nMinPage = 0;
                        pd.nMaxPage = 0;
                        pd.nCopies = 1;
                        pd.hInstance = NULL;
                        pd.lCustData = 0L;
                        pd.lpfnPrintHook = (LPPRINTHOOKPROC)NULL;
                        pd.lpfnSetupHook = (LPSETUPHOOKPROC)NULL;
                        pd.lpPrintTemplateName = (LPSTR)NULL;
                        pd.lpSetupTemplateName = (LPSTR)NULL;
                        pd.hPrintTemplate = (HANDLE)NULL;
                        pd.hSetupTemplate = (HANDLE)NULL;
                        PrintDlg(&pd);
                        // TODO: what if user presses cancel?
                        lpt_epsonprinter->printerDC = pd.hDC;
#endif
                }
                pclog("PRINTER: Enabled\n");
        }

        // TODO: MAPPER_AddHandler(FormFeed,MK_f2,MMOD1,"ejectpage","formfeed");

        return lpt_epsonprinter;
}

static void epsonprinter_close(void *p) {
        lpt_epsonprinter_t *lpt_epsonprinter = (lpt_epsonprinter_t *)p;

        formFeed(lpt_epsonprinter); // eject any pending page
        if (lpt_epsonprinter->page != NULL) {
                SDL_FreeSurface(lpt_epsonprinter->page);
                lpt_epsonprinter->page = NULL;
                FT_Done_FreeType(lpt_epsonprinter->FTlib);
        }
#if defined(WIN32)
        DeleteDC(lpt_epsonprinter->printerDC);
#endif

        free(lpt_epsonprinter);
}

void *epsonprinter_init_lpt1() {
        current_device = (device_t *)&lpt_epsonprinter_device;
        void *p = epsonprinter_init();
        current_device = NULL;
        // lpt1_device_attach(&lpt_epsonprinter_device, p);

        return p;
}
void epsonprinter_close_lpt1(void *p) {
        epsonprinter_close(p);
        // lpt1_device_detach();
}

static device_config_t epsonprinter_config[] = {
        {.name = "dpi",
         .description = "Output DPI",
         .type = CONFIG_SELECTION,
         .selection = {{.description = "240 DPI", .value = 240},
                       {.description = "360 DPI", .value = 360},
                       {.description = "480 DPI", .value = 480},
                       {.description = "600 DPI", .value = 600},
                       {.description = "720 DPI", .value = 720},
                       {.description = "840 DPI", .value = 840},
                       {.description = "960 DPI", .value = 960},
                       {.description = "1080 DPI", .value = 1080},
                       {.description = ""}},
         .default_int = 360},
        {.name = "outputtype",
         .description = "Output type",
         .type = CONFIG_SELECTION,
         .selection = {{.description = "Postscript (.ps)", .value = OUTPUT_TYPE_POSTSCRIPT},
                       {.description = "Bitmap (.bmp)", .value = OUTPUT_TYPE_BMP},
                       {.description = "Portable Network Graphics (.png)", .value = OUTPUT_TYPE_PNG},
                       {.description = "Tagged Image File Format (.tiff)", .value = OUTPUT_TYPE_TIFF},
                       {.description = "JPEG (.jpg)", .value = OUTPUT_TYPE_JPG},
#if defined(WIN32)
                       {.description = "Redirect to real printer", .value = OUTPUT_TYPE_FORWARD_TO_REAL_PRINTER},
#endif
                       {.description = ""}},
         .default_int = OUTPUT_TYPE_PNG},
        {.name = "multipage",
         .description = "Multipage for Postscript and real printer",
         .type = CONFIG_BINARY,
         .default_int = 1},
        {.name = "papersize",
         .description = "Default paper size",
         .type = CONFIG_SELECTION,
         .selection =
                 {// TODO: more default lengths are supported, see DIP switch settings on the manual and see to with paper types
                  // they map
                  {.description = "Letter (8 1/2\" x 11\")", .value = 0},
                  {.description = "A4 (210mm x 297mm)", .value = 1},
                  {.description = ""}},
         .default_int = 0},
        {.name = "cpi",
         .description = "Default CPI (Characters per inch)",
         .type = CONFIG_SELECTION,
         .selection = {{.description = "10 CPI", .value = 10}, {.description = "12 CPI", .value = 12}, {.description = ""}},
         .default_int = 10},
        /*{ TODO
                .name = "shapeofzero",
                .description = "Shape of zero",
                .type = CONFIG_SELECTION,
                .selection =
                {
                        {
                                .description = "Not Slashed",
                                .value = 0
                        },
                        {
                                .description = "Slashed",
                                .value = 1
                        },
                        {
                                .description = ""
                        }
                },
                .default_int = 0
        },*/
        {.name = "font",
         .description = "Default font",
         .type = CONFIG_SELECTION,
         .selection = {{.description = "Draft", .value = 0},
                       {.description = "Draft condensed", .value = 1},
                       {.description = "NLQ Roman", .value = 2},
                       {.description = "NLQ Sans Serif", .value = 3},
                       {.description = ""}},
         .default_int = 0},
        /*{ TODO
                .name = "draftmode",
                .description = "Draft mode",
                .type = CONFIG_SELECTION,
                .selection =
                {
                        {
                                .description = "Normal",
                                .value = 0
                        },
                        {
                                .description = "High-speed", // TODO: different font, only works at 10 CPI! Reverts do "Draft"
        otherwise (what about styles?) .value = 1
                        },
                        {
                                .description = ""
                        }
                },
                .default_int = 0
        },*/
        {.name = "characterset",
         .description = "Default character set",
         .type = CONFIG_SELECTION,
         .selection = {{.description = "Italics", .value = 0},
                       {.description = "CP437 + U.S.A.", .value = 1},
                       {.description = "CP437 + France", .value = 2},
                       {.description = "CP437 + Germany", .value = 3},
                       {.description = "CP437 + U.K.", .value = 4},
                       {.description = "CP437 + Denmark I", .value = 5},
                       {.description = "CP437 + Sweden", .value = 6},
                       {.description = "CP437 + Italy", .value = 7},
                       {.description = "CP437 + Spain I", .value = 8},
                       {.description = "CP437 + Japan (Only available as a printer command on real hardware)", .value = 9},
                       {.description = "CP437 + Norway (Only available as a printer command on real hardware)", .value = 10},
                       {.description = "CP437 + Denmark II (Only available as a printer command on real hardware)", .value = 11},
                       {.description = "CP437 + Spain II (Only available as a printer command on real hardware)", .value = 12},
                       {.description = "CP437 + Latin America (Only available as a printer command on real hardware)",
                        .value = 13},
                       // The international character sets above can be selected (via software) with the codepages below, but I
                       // don't know if it makes sense.
                       {.description = "CP850 (Multilingual)", .value = 14},
                       {.description = "CP860 (Portugal)", .value = 15},
                       {.description = "CP863 (Canada-French)", .value = 16},
                       {.description = "CP865 (Norway)", .value = 17},
                       {.description = ""}},
         .default_int = 1},
        /*{ TODO: Didn't implement because I didn't test if this is FORCE or just START ON
                .name = "autolinefeed",
                .description = "Force auto linefeed",
                .type = CONFIG_BINARY,
                .default_int = 0
        },*/
        /*{ TODO: this forces an upper and lower margin by default - half and inch at each. Can be overridden by software - check
        if it goes back to these values after a software/command reset. .name = "skipperforation", .description = "1-inch skip
        over perforation", .type = CONFIG_BINARY, .default_int = 0
        },*/
        {.name = "bitmaps_emulate_pins",
         .description = "Emulate pin spacing when printing graphics",
         .type = CONFIG_BINARY,
         .default_int = 1},
        {.type = -1}};

lpt_device_t lpt_epsonprinter_device = {"Epson LX-810 Printer",
                                        0,
                                        epsonprinter_init_lpt1,
                                        epsonprinter_close_lpt1,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL,
                                        epsonprinter_config,
                                        epsonprinter_write_data,
                                        epsonprinter_write_ctrl,
                                        epsonprinter_read_status};
