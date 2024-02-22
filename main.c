#define STB_DS_IMPLEMENTATION
#include "all.h"
#include <stdbool.h>
#include <stdio.h>
#include "ansi.h"

// leak everything
const char* __asan_default_options(void) { return "detect_leaks=0"; }

rsym_t __main_init;

#define TRY_IF if (!err && setjmp(err_diag.unwind)) { err = true; } else

int main(int argc, const char *argv[]) {
	bool err = false;

	// const char *arch = "stdc99_64";
	const char *arch = "stdc_64";
	const char *platform = "libc";

	if (argc == 2) {
		TRY_IF {
			goto ret;
		}

		// god this needs to be standardised in an interface
		fs_entrypoint(argv[1]);
		fs_target(arch, platform, false);

		// queue can grow
		for (fs_rfile_t i = 0; i < fs_files_queue_len; i++) {
			u32 old_sz = fs_files_queue_len;
			eprintf("parsing file '%s'\n", fs_make_relative(fs_files_queue[i].fp));
			compiler_process_file(i);
			if (old_sz != fs_files_queue_len) {
				eprintf("  %u new files added\n", fs_files_queue_len - old_sz);
			}
		}
	} else {
		eprintf("usage: %s <file|dir>\n", argv[0]);
	}
ret:;
	fs_dump_tree();

	return err;
}
