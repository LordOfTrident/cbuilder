/*
 * Compile and run me with:
 *   $ cc build.c -o build
 *   $ ./build
 *
 */

#include <string.h> /* strcmp, strcpy, strlen */
#include <malloc.h> /* malloc, free */
#include <assert.h> /* assert */

#define CBUILDER_IMPLEMENTATION
#include "../cbuilder.h"

/* Directories */
#define RES "res"
#define SRC "src"
#define BIN "bin"
#define OUT "app"

#define CARGS "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic", \
              "-Wno-deprecated-declarations", "-std=c99"

char *cc = CC;

void create_dir_if_missing(const char *path) {
	if (!fs_exists(path))
		fs_create_dir(path);
}

/* Header containing macros for embed paths */
#include "embeds.h"

void prepare(void) {
	create_dir_if_missing(SRC"/embed");
	create_dir_if_missing(BIN);

	EMBED(); /* Macro from 'embeds.h' to create the embeds */
}

char *build_file(build_cache_t *c, const char *name, bool rebuild_all) {
	char *str = fs_replace_ext(name, "o"); /* Change *.c to *.o */
	if (str == NULL)
		LOG_FAIL("malloc()");

	char *out = FS_JOIN_PATH(BIN, str);
	char *src = FS_JOIN_PATH(SRC, name);
	if (src == NULL || out == NULL)
		LOG_FAIL("malloc()");

	free(str);

	/* Get the file last modified time */
	int64_t m;
	if (fs_time(src, &m, NULL) != 0)
		LOG_FATAL("Failed to get last modified time of file '%s'", src);

	/* Check the cache to see if the last modified time changed */
	int64_t cached_m = build_cache_get(c, src);
	if (m == cached_m && !rebuild_all) { /* If it did not, return, otherwise rebuild */
		free(src);
		return out;
	}

	build_cache_set(c, src, m);

	/* Compile the object file */
	CMD(cc, "-c", src, "-o", out, CARGS);

	free(src);
	return out;
}

void build(void) {
	/* List of compiled object files */
	char  *o_files[32];
	size_t o_files_count = 0;

	/* Build cache for optimized building */
	build_cache_t c; /* Cache is stored in BUILD_CACHE_PATH, which is ".cbuilder-cache". Dont
	                    forget to put this path into .gitignore if youre using git */
	if (build_cache_load(&c) != 0)
		LOG_FATAL("Build cache is corrupted");

	bool rebuild_all = false;

	int status;
	FOREACH_IN_DIR(SRC, dir, ent, {
		if (strcmp(fs_ext(ent.name), "h") != 0)
			continue;

		char *src = FS_JOIN_PATH(SRC, ent.name);
		if (src == NULL)
			LOG_FAIL("malloc()");

		int64_t m;
		if (fs_time(src, &m, NULL) != 0)
			LOG_FATAL("Failed to get last modified time of file '%s'", src);

		int64_t cached_m = build_cache_get(&c, src);
		if (m != cached_m) {
			build_cache_set(&c, src, m);
			rebuild_all = true;
		}

		free(src);
	}, status);

	if (status != 0)
		LOG_FATAL("Failed to open directory '%s'", SRC);

	FOREACH_IN_DIR(SRC, dir, ent, {
		if (strcmp(fs_ext(ent.name), "c") != 0)
			continue;

		assert(o_files_count < sizeof(o_files) / sizeof(o_files[0]));

		char *out = build_file(&c, ent.name, rebuild_all);
		o_files[o_files_count ++] = out;
	}, status);

	if (status != 0)
		LOG_FATAL("Failed to open directory '%s'", SRC);

	if (build_cache_save(&c) != 0)
		LOG_FATAL("Failed to save build cache");

	/* For compiling a variable list of files, we use the COMPILE macro */
	COMPILE(cc, o_files, o_files_count, "-o", BIN"/"OUT, CARGS);

	for (size_t i = 0; i < o_files_count; ++ i)
		free(o_files[i]);

	build_cache_free(&c);
}

void clean(void) {
	bool found = false;
	int  status; /* Directory status, 0 if it was opened succesfully */
	FOREACH_IN_DIR(BIN, dir, ent, {
		/* Only clean .o files and 'app' */
		if (strcmp(fs_ext(ent.name), "o") != 0 && strcmp(ent.name, OUT) != 0)
			continue;

		if (!found)
			found = true;

		char *path = FS_JOIN_PATH(dir.path, ent.name);
		if (path == NULL)
			LOG_FAIL("malloc()");

		fs_remove_file(path);

		free(path);
	}, status);

	if (status != 0)
		LOG_FATAL("Failed to open directory '%s'", SRC);

	build_cache_delete();

	if (!found)
		LOG_ERROR("Nothing to clean");
	else
		LOG_INFO("Cleaned '%s'", BIN);
}

int main(int argc, const char **argv) {
	args_t a = build_init(argc, argv);
	build_set_usage("[clean] [OPTIONS]"); /* Add the 'clean' subcommand to the usage */

	flag_cstr(NULL, "CC", "The C compiler path", &cc);

	args_t stripped; /* The flagless arguments */
	build_parse_args(&a, &stripped);
	if (stripped.c > 0) {
		const char *subcmd = stripped.v[0];
		if (strcmp(subcmd, "clean") == 0) {
			if (stripped.c > 1) {
				build_arg_error("Unexpected argument '%s'", stripped.v[1]);
				exit(EXIT_FAILURE);
			}

			clean();
		} else {
			build_arg_error("Unknown subcommand '%s'", subcmd);
			exit(EXIT_FAILURE);
		}
	} else {
		prepare();
		build();
	}

	free(stripped.base);
	return EXIT_SUCCESS;
}
