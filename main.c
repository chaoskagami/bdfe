/*  BSD License
    Copyright (c) 2015 Andrey Chilikin https://github.com/achilikin

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
#include <unistd.h>

#include "bdf.h"
#include "rterm.h"

#define DISPLAY_FONT 0x80000000

/**
 sarg: short argument
 larg: long argument
 */
static int arg_is(const char *arg, const char *sarg, const char *larg)
{
	if (sarg && (strcmp(arg, sarg) == 0))
		return 1;
	if (larg && (strcmp(arg, larg) == 0))
		return 1;
	return 0;
}

static void usage(const char *name)
{
	printf("%s [options] <bdf file>\n", name);
	printf("  options are:\n");
	printf("  header:     print file header\n");
	printf("  verbose:    add extra info to the header\n");
	printf("  line:       one line per glyph\n");
	printf("  subset a-b: subset of glyphs to convert a to b, default 32-126\n");
	printf("  all:        print all glyphs, not just 32-126\n");
	printf("  native:     do not adjust font height 8 pixels\n");
	printf("  ascender H: add extra ascender of H pixels per glyph\n");
}

int main(int argc, char **argv)
{
	char *file;
	bdfe_t *font;
  	int flags = 0;
	unsigned ascender = 0;
	unsigned gmin = 32, gmax = 126;

	if (argc < 2) {
		usage(basename(argv[0]));
		return -1;
	}

	for(int i = 1; i < argc; i++) {
		if (arg_is(argv[i], "-?", "help")) {
			usage(basename(argv[0]));
			return 0;
		}

		if (arg_is(argv[i], "-h", "header"))
			flags |= BDF_HEADER;

		if (arg_is(argv[i], "-v", "verbose"))
			flags |= BDF_VERBOSE;

		if (arg_is(argv[i], "-a", "ascender")) {
			if (i < argc && isdigit(*argv[i+1]))
				ascender = atoi(argv[++i]);
		}

		if (arg_is(argv[i], "-l", "line"))
			flags |= BDF_GPL;

		if (arg_is(argv[i], "-s", "subset")) {
			if (i < argc && isdigit(*argv[i+1])) {
				i++;
				char *end;
				gmin = strtoul(argv[i], &end, 10);
				gmax = gmin;
				if (*end == '-')
					gmax = strtoul(end+1, &end, 10);
				if (gmax < gmin) {
					unsigned tmp = gmin;
					gmin = gmax;
					gmax = tmp;
				}
			}
		}

		if (arg_is(argv[i], "-a", "all")) {
			gmin = 0;
			gmax = 0xFFFFFFFF;
		}

		if (arg_is(argv[i], "-n", "native"))
			flags |= BDF_NATIVE;

		if (arg_is(argv[i], "-r", "rotate"))
			flags |= BDF_ROTATE;

		if (arg_is(argv[i], "-d", "display"))
			flags |= DISPLAY_FONT;
	}

	file = argv[argc - 1];
	font = bdf_convert(file, gmin, gmax, ascender, flags);

	if (font == NULL) {
		fprintf(stderr, "Unable to convert '%s'\n", file);
		return -1;
	}

	free(font);
	return 0;
}
