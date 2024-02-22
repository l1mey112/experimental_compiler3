#include "all.h"
#include <stdarg.h>
#include <unistd.h>

err_diag_t err_diag;

void print_diag_with_pos(const char *type, loc_t loc, const char *fmt, ...) {
	char err_string[256];
	
	va_list args;
	va_start(args, fmt);
	vsnprintf(err_string, sizeof(err_string), fmt, args);
	va_end(args);

	lineinfo_t lineinfo = fs_reconstruct_lineinfo(loc);
	fs_file_t *file = &fs_files_queue[lineinfo.file];

	if (isatty(fileno(stdout))) {
		eprintf("\033[1;31m%s:\033[0m %s:%u:%u: %s\n", type, fs_make_relative(file->fp), lineinfo.line_nr + 1, lineinfo.col + 1, err_string);	
	} else {
		eprintf("%s: %s:%u:%u: %s\n", type, fs_make_relative(file->fp), lineinfo.line_nr + 1, lineinfo.col + 1, err_string);	
	}
}

void print_diag_without_pos(const char *type, const char *fmt, ...) {
	char err_string[256];
	
	va_list args;
	va_start(args, fmt);
	vsnprintf(err_string, sizeof(err_string), fmt, args);
	va_end(args);

	if (isatty(fileno(stdout))) {
		eprintf("\033[1;31m%s:\033[0m %s\n", type, err_string);
	} else {
		eprintf("%s: %s\n", type, err_string);
	}
}
