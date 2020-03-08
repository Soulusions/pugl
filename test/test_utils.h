/*
  Copyright 2012-2019 David Robillard <http://drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "pugl/pugl.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
	int  samples;
	int  doubleBuffer;
	int  sync;
	bool continuous;
	bool help;
	bool ignoreKeyRepeat;
	bool resizable;
	bool verbose;
	bool errorChecking;
} PuglTestOptions;

static inline int
logError(const char* fmt, ...)
{
	fprintf(stderr, "error: ");

	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	return 1;
}

static inline int
printModifiers(const uint32_t mods)
{
	return fprintf(stderr, "Modifiers:%s%s%s%s\n",
	               (mods & PUGL_MOD_SHIFT) ? " Shift"   : "",
	               (mods & PUGL_MOD_CTRL)  ? " Ctrl"    : "",
	               (mods & PUGL_MOD_ALT)   ? " Alt"     : "",
	               (mods & PUGL_MOD_SUPER) ? " Super" : "");
}

static inline int
printEvent(const PuglEvent* event, const char* prefix, const bool verbose)
{
#define FFMT            "%6.1f"
#define PFMT            FFMT " " FFMT
#define PRINT(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)

	switch (event->type) {
	case PUGL_NOTHING:
		return 0;
	case PUGL_KEY_PRESS:
		return PRINT("%sKey press   code %3u key  U+%04X\n",
		             prefix,
		             event->key.keycode,
		             event->key.key);
	case PUGL_KEY_RELEASE:
		return PRINT("%sKey release code %3u key  U+%04X\n",
		             prefix,
		             event->key.keycode,
		             event->key.key);
	case PUGL_TEXT:
		return PRINT("%sText entry  code %3u char U+%04X (%s)\n",
		             prefix,
		             event->text.keycode,
		             event->text.character,
		             event->text.string);
	case PUGL_BUTTON_PRESS:
	case PUGL_BUTTON_RELEASE:
		return (PRINT("%sMouse %d %s at " PFMT " ",
		              prefix,
		              event->button.button,
		              (event->type == PUGL_BUTTON_PRESS) ? "down" : "up  ",
		              event->button.x,
		              event->button.y) +
		        printModifiers(event->scroll.state));
	case PUGL_SCROLL:
		return (PRINT("%sScroll %5.1f %5.1f at " PFMT " ",
		              prefix,
		              event->scroll.dx,
		              event->scroll.dy,
		              event->scroll.x,
		              event->scroll.y) +
		        printModifiers(event->scroll.state));
	case PUGL_ENTER_NOTIFY:
		return PRINT("%sMouse enter  at " PFMT "\n",
		             prefix,
		             event->crossing.x,
		             event->crossing.y);
	case PUGL_LEAVE_NOTIFY:
		return PRINT("%sMouse leave  at " PFMT "\n",
		             prefix,
		             event->crossing.x,
		             event->crossing.y);
	case PUGL_FOCUS_IN:
		return PRINT("%sFocus in%s\n",
		             prefix,
		             event->focus.grab ? " (grab)" : "");
	case PUGL_FOCUS_OUT:
		return PRINT("%sFocus out%s\n",
		             prefix,
		             event->focus.grab ? " (ungrab)" : "");
	default:
		break;
	}

	if (verbose) {
		switch (event->type) {
		case PUGL_CONFIGURE:
			return PRINT("%sConfigure " PFMT " " PFMT "\n",
			             prefix,
			             event->configure.x,
			             event->configure.y,
			             event->configure.width,
			             event->configure.height);
		case PUGL_EXPOSE:
			return PRINT("%sExpose    " PFMT " " PFMT "\n",
			             prefix,
			             event->expose.x,
			             event->expose.y,
			             event->expose.width,
			             event->expose.height);
		case PUGL_CLOSE:
			return PRINT("%sClose\n", prefix);
		case PUGL_MOTION_NOTIFY:
			return PRINT("%sMouse motion at " PFMT "\n",
			             prefix,
			             event->motion.x,
			             event->motion.y);
		default:
			fprintf(stderr, "%sUnknown event type %d\n", prefix, event->type);
			break;
		}
	}

#undef PRINT
#undef PFMT
#undef FFMT

	return 0;
}

static inline void
puglPrintTestUsage(const char* prog, const char* posHelp)
{
	printf("Usage: %s [OPTION]... %s\n\n"
	       "  -a  Enable anti-aliasing\n"
	       "  -c  Continuously animate and draw\n"
	       "  -d  Enable double-buffering\n"
	       "  -e  Enable platform error-checking\n"
	       "  -f  Fast drawing, explicitly disable vertical sync\n"
	       "  -h  Display this help\n"
	       "  -i  Ignore key repeat\n"
	       "  -v  Print verbose output\n"
	       "  -r  Resizable window\n"
	       "  -s  Explicitly enable vertical sync\n",
	       prog, posHelp);
}

static inline PuglTestOptions
puglParseTestOptions(int* pargc, char*** pargv)
{
	PuglTestOptions opts = {
	    0,
	    0,
	    PUGL_DONT_CARE,
	    false,
	    false,
	    false,
	    false,
	    false,
	    false,
	};

	char** const argv = *pargv;
	int          i    = 1;
	for (; i < *pargc; ++i) {
		if (!strcmp(argv[i], "-a")) {
			opts.samples = 4;
		} else if (!strcmp(argv[i], "-c")) {
			opts.continuous = true;
		} else if (!strcmp(argv[i], "-d")) {
			opts.doubleBuffer = PUGL_TRUE;
		} else if (!strcmp(argv[i], "-e")) {
			opts.errorChecking = PUGL_TRUE;
		} else if (!strcmp(argv[i], "-f")) {
			opts.sync = PUGL_FALSE;
		} else if (!strcmp(argv[i], "-h")) {
			opts.help = true;
			return opts;
		} else if (!strcmp(argv[i], "-i")) {
			opts.ignoreKeyRepeat = true;
		} else if (!strcmp(argv[i], "-r")) {
			opts.resizable = true;
		} else if (!strcmp(argv[i], "-s")) {
			opts.sync = PUGL_TRUE;
		} else if (!strcmp(argv[i], "-v")) {
			opts.verbose = true;
		} else if (argv[i][0] != '-') {
			break;
		} else {
			opts.help = true;
			logError("Unknown option: %s\n", argv[i]);
		}
	}

	*pargc -= i;
	*pargv += i;

	return opts;
}
