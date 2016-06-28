/*  BSD License
    Copyright (c) 2014 Andrey Chilikin https://github.com/achilikin

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
	this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
	BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
	OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

/**
	Basic BDF Exporter - converts BDF files to C structures.
	Only 8 bits wide fonts are supported.
*/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "bdf.h"

static int printable(unsigned character) {
    // This is a workaround for older glibc - isprint doesn't
    // return the needed results (and it's only standard behavior for 0-255 anyways)

    if (character < 0x20 || character == 0x7f || character > 0xff)
        return 0;
    return 1;
}

// check if 'buf' starts with 'key' and store pointer to the argument
static char *key_arg(char *buf, const char *key, char **arg)
{
	char *end;
	size_t len = strlen(key);

	if (strncmp(buf, key, len) == 0) {
		end = buf + len;
		if (*end > ' ')
			return NULL;
		if (arg == NULL)
			return buf;
		for(; *end <= ' ' && *end != '\0'; end++);
		*arg = end;
		return buf;
	}

	return NULL;
}

// rotate glyph bitmap CCW
static uint32_t rotate_glyph(const uint8_t *glyph, unsigned gw, int gh, uint8_t *grot)
{
	uint32_t dy = 0;

	do {
		for(uint8_t n = 0; n < gw; n++) {
			uint8_t out = 0;
			uint8_t mask = 0x80 >> n;
			for(uint8_t b = 0; b < 8; b++) {
				if (glyph[b] & mask)
					out |= 1 << b;
			}
			grot[dy++] = out;
		}
		glyph += 8;
	} while((gh -= 8) > 0);

	return dy;
}

bdfe_t *bdf_convert(const char *name, unsigned gmin, unsigned gmax, unsigned ascender, int flags)
{
	FILE *fp;
	char *arg;
	bdfe_t *font = NULL;
	char buf[BUFSIZ];
	char startchar[BUFSIZ];

	if (name == NULL || (fp = fopen(name, "r")) == NULL)
		return NULL;

	memset(buf, 0, sizeof(buf));
	int mute = flags & BDF_MUTE;

	if (!mute && (flags & BDF_HEADER)) {
		printf("/* File generated by 'bdfe");
		if (flags & BDF_NATIVE)
			printf(" -n");
		if (flags & BDF_ROTATE)
			printf(" -r");
		if (ascender)
			printf(" -a %d", ascender);
		printf(" -s %d-%d %s' */\n", gmin, gmax, name);
	}

	// parse file header up to 'CHARS' keyword
	unsigned nchars = 0, dx = 0, dy = 0, descent = 0;
	while(fgets(buf, sizeof(buf) - 2, fp) != NULL) {
		arg = strchr(buf, '\0');
		while(*arg < ' ' && arg != buf) *arg-- = '\0';

		if (!mute && (flags & BDF_HEADER)) {
			if (key_arg(buf, "FONT", &arg))
				printf("/* %s */\n", buf);
			if (key_arg(buf, "COMMENT", &arg))
				printf("/* %s */\n", buf);
			if (key_arg(buf, "COPYRIGHT", &arg))
				printf("/* %s */\n", buf);
			if (key_arg(buf, "FONT_ASCENT", &arg))
				printf("/* %s */\n", buf);
		}

		if (key_arg(buf, "FONTBOUNDINGBOX", &arg)) {
			if (!mute && (flags & BDF_HEADER))
				printf("/* %s */\n", buf);
			dx = strtoul(arg, &arg, 10);
			dy = strtoul(arg, &arg, 10);
			strtoul(arg, &arg, 10);
			descent = -strtoul(arg, &arg, 10);
		}

		if (key_arg(buf, "FONT_DESCENT", &arg)) {
			if (!mute && (flags & BDF_HEADER))
				printf("/* %s */\n", buf);
			descent = strtoul(arg, &arg, 10);
		}

		if (key_arg(buf, "CHARS", &arg)) {
			nchars = atoi(arg);
			break;
		}
	}

	if (dy == 0) {
		fclose(fp);
		return NULL;
	}

	// recalculate glyphs x size as we cannot use FONTBOUNDINGBOX
	// as it is for our gmin/gmax range
	dx = 0;
	while(fgets(buf, sizeof(buf) - 2, fp) != NULL) {
		if (key_arg(buf, "STARTCHAR", &arg)) {
			unsigned idx = 0, dw;

			while(fgets(buf, sizeof(buf) - 2, fp) != NULL) {
				if (key_arg(buf, "ENCODING", &arg)) {
					idx = atoi(arg);
					if (idx < gmin || idx > gmax)
						break;
				}
				if (key_arg(buf, "DWIDTH", &arg)) {
					dw = atoi(arg);
					if (dw > dx)
						dx = dw;
					break;
				}
			}
		}
	}

	// limit vertical size to 16 pixels or 2 lines on SSD1306
	if (dy > 16)
		dy = 16;

	unsigned gh = dy, gw = dx;
	// round y size to 8 pixels for SSD1306 text mode
	if ((flags & BDF_ROTATE) || !(flags & BDF_NATIVE)) {
		dy = (dy+7) & ~0x07;
		descent += dy - gh;
		gh = dy;
	}

	if (!mute && (flags & BDF_HEADER))
		printf("// Converted Font Size %dx%d\n\n", dx, dy);
	if (ascender > dy/2)
		ascender = dy/2;
	// rewind the file pointer ans start parsing glyphs
	fseek(fp, 0, SEEK_SET);

	unsigned gsize = dy * ((dx + 7) / 8);
	font = (bdfe_t *)malloc(sizeof(bdfe_t) + gsize*nchars);
	font->chars = 0;
	// glyphs output buffer
	uint8_t *gout = font->font;
	// glyph storage buffer, big enough to allow displacement manipulations
	uint8_t *gbuf = (uint8_t *)malloc(gsize*2);
	memset(gbuf, 0, gsize*2);

	while(fgets(buf, sizeof(buf) - 2, fp) != NULL) {
		if (key_arg(buf, "STARTCHAR", &arg)) {
			unsigned displacement = 0;
			unsigned bitmap = 0, i = 0, idx = 0;
			unsigned bbw = 0;
			int bbox = 0, bboy = 0;
			uint8_t *gin = gbuf + gsize;

			memset(gin, 0, gsize);
			// store a copy in case if verbose output is on
			arg = strchr(buf, '\0');
			while(*arg < ' ' && arg != buf) *arg-- = '\0';
			strcpy(startchar, buf);

			while(fgets(buf, sizeof(buf) - 2, fp) != NULL) {
				arg = strchr(buf, '\0');
				while(*arg < ' ' && arg != buf) *arg-- = '\0';

				if (key_arg(buf, "ENCODING", &arg)) {
					idx = atoi(arg);
					if (idx < gmin || idx > gmax)
						break;
					if (!mute && !(flags & BDF_GPL) && (flags & BDF_VERBOSE)) {
						printf("/* %s */\n", startchar);
						printf("/* %s */\n", buf);
					}
				}
				if (key_arg(buf, "DWIDTH", &arg)) {
					gw = atoi(arg);
					if (gw > 8)
						break;
				}
				if (key_arg(buf, "BBX", &arg)) {
					if (!mute && !(flags & BDF_GPL) && (flags & BDF_VERBOSE))
						printf("/* %s */\n", buf);
					bbw = strtol(arg, &arg, 10);
					strtol(arg, &arg, 10); // skip bbh, we'll calculate it (i)
					bbox = strtol(arg, &arg, 10);
					bboy = strtol(arg, &arg, 10);
				}
				if (key_arg(buf, "ENDCHAR", &arg)) {
					font->chars++;
					// check if baseline alignment is needed
					if (bboy > 0)
						i += bboy;
					if ((bboy < 0) && (i < dy)) {
						displacement = dy - descent - i; // move BBX to origin
						displacement -= bboy;
						i += displacement;
					}
					if (i < (dy - descent))
						displacement += dy - descent - i;
					gin -= displacement + ascender;
					if (flags & BDF_ROTATE) {
						gh = rotate_glyph(gin, dx, dy, gout);
						gw = 8;
					}
					else
						memcpy(gout, gin, dy);

					if (!mute) {
						// glyph per line
						if (flags & BDF_GPL) {
							printf("\t");
							for(i = 0; i < gh; i++) {
								if ((i == gh/2) && (flags & BDF_ROTATE))
									printf("\n\t");
								printf("0x%02X,", gout[i]);
							}
							printf(" /* %5d", idx);
							if (printable(idx))
								printf(" '%c'", idx);
							printf(" */\n");
						}
						else {
							printf("/* %5d '%c' |", idx, printable(idx) ? idx : ' ');
							for(i = 0; i < gw; i++)
								printf("%d", i);
							printf("| */\n");
							for(i = 0; i < gh; i++) {
								printf(" 0x%02X, /*  %2d|", gout[i], i);
								for(unsigned bit = 0; bit < gw; bit++) {
									if (gout[i] & (0x80 >> bit))
										printf("#");
									else
										printf(" ");
								}
								printf("| */\n");
							}
							printf("\n");
						}
					}
					gout += gh;
					break;
				}
				if (key_arg(buf, "BITMAP", &arg)) {
					bitmap = 1;
					continue;
				}
				if (bitmap && (i < dy)) {
					gin[i] = (uint8_t)strtoul(buf, NULL, 16);
					if ((bbw < 8) && (bbox > 0) && (bbox < 8))
						gin[i] = gin[i] >> bbox;
					i++;
				}
			}
		}
	}
	font->gw  = dx;
	font->bpg = gh;

	free(gbuf);
	fclose(fp);
	return font;
}
