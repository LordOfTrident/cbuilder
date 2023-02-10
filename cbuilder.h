/*
 * https://github.com/LordOfTrident/cbuilder
 *
 * #define CBUILDER_IMPLEMENTATION
 *
 */

#ifndef CBUILDER_H_HEADER_GUARD
#define CBUILDER_H_HEADER_GUARD

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>  /* stderr, fprintf, fopen, fclose, fgetc, EOF */
#include <stdarg.h> /* va_list, va_start, va_end, vsnprintf */
#include <assert.h> /* assert */
#include <stdlib.h> /* exit, EXIT_FAILURE, EXIT_SUCCESS, malloc */

#include "lib/clog.h"
#include "lib/cargs.h"
#include "lib/cfs.h"

#define CBUILDER_VERSION_MAJOR 1
#define CBUILDER_VERSION_MINOR 0
#define CBUILDER_VERSION_PATCH 1

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#	define BUILD_PLATFORM_WINDOWS
#elif defined(__APPLE__)
#	define BUILD_PLATFORM_APPLE
#elif defined(__linux__) || defined(__gnu_linux__) || defined(linux)
#	define BUILD_PLATFORM_LINUX
#elif defined(__unix__) || defined(unix)
#	define BUILD_PLATFORM_UNIX
#else
#	define BUILD_PLATFORM_UNKNOWN
#endif

#ifdef BUILD_PLATFORM_WINDOWS
#	include <windows.h>

#	define CC  "gcc"
#	define CXX "g++"
#else
#	include <unistd.h>
#	include <sys/types.h>
#	include <sys/wait.h>

#	define CC  "cc"
#	define CXX "c++"
#endif

#define BUILD_APP_NAME "./build"

void build_set_usage(const char *usage);
void build_parse_args(args_t *a, args_t *stripped);

void build_arg_error(const char *fmt, ...);

#define CMD(NAME, ...) \
	do { \
		const char *argv[] = {NAME, __VA_ARGS__, NULL}; \
		cmd(argv); \
	} while (0)

#define COMPILE(NAME, SRCS, SRCS_COUNT, ...) \
	do { \
		const char *args[] = {__VA_ARGS__}; \
		compile(NAME, (const char **)SRCS, SRCS_COUNT, args, sizeof(args) / sizeof(args[0])); \
	} while (0)

void cmd(const char **argv);
void compile(const char *compiler, const char **srcs, size_t srcs_count,
             const char **args, size_t args_count);

enum {
	STRING_ARRAY = 0,
	BYTE_ARRAY,
};

void embed(const char *path, const char *out, int type);

#ifdef __cplusplus
}
#endif

#endif

#ifdef CBUILDER_IMPLEMENTATION

#ifdef __cplusplus
extern "C" {
#endif

#define CLOG_IMPLEMENTATION
#include "lib/clog.h"

#define CARGS_IMPLEMENTATION
#include "lib/cargs.h"

#define CFS_IMPLEMENTATION
#include "lib/cfs.h"

static bool _build_help = false;
static bool _build_ver  = false;

static const char *_build_usage = "[OPTIONS]";

args_t build_init(int argc, const char **argv) {
	args_t a = new_args(argc, argv);
	args_shift(&a);

	flag_bool("h", "help",    "Show the usage",   &_build_help);
	flag_bool("v", "version", "Show the version", &_build_ver);

	log_set_flags(LOG_TIME);

	return a;
}

void build_arg_error(const char *fmt, ...) {
	char    msg[256];
	va_list args;

	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	fprintf(stderr, "Error: %s\n", msg);
	fprintf(stderr, "Try '"BUILD_APP_NAME" -h'\n");
}

void build_set_usage(const char *usage) {
	_build_usage = usage;
}

void build_parse_args(args_t *a, args_t *stripped) {
	int where;
	int err = args_parse_flags(a, &where, stripped);
	if (err != ARG_OK) {
		switch (err) {
		case ARG_OUT_OF_MEM:    assert(0 && "malloc() fail");
		case ARG_UNKNOWN:       build_arg_error("Unknown flag '%s'", a->v[where]);            break;
		case ARG_MISSING_VALUE: build_arg_error("Flag '%s' is a missing value", a->v[where]); break;

		default: build_arg_error("Incorrect type for flag '%s'", a->v[where]);
		}

		exit(EXIT_FAILURE);
	}

	if (_build_help) {
		args_print_usage(stdout, BUILD_APP_NAME, _build_usage);
		exit(EXIT_SUCCESS);
	} else if (_build_ver) {
		printf("cbuilder v%i.%i.%i\n",
		       CBUILDER_VERSION_MAJOR, CBUILDER_VERSION_MINOR, CBUILDER_VERSION_PATCH);
		exit(EXIT_SUCCESS);
	}
}

void cmd(const char **argv) {
	char buf[1024] = {0};
	for (const char **next = argv; *next != NULL; ++ next) {
		strcat(buf, *next);
		strcat(buf, " ");
	}

	LOG_CUSTOM("CMD", "%s", buf);
	pid_t pid;
	int   status;

	pid_t pid_ = fork();
	if (pid_ == 0) {
		if (execvp(argv[0], (char**)argv) == -1)
			LOG_FATAL("execvp() fail");

		exit(EXIT_SUCCESS);
	} else if (pid_ == -1)
		LOG_FATAL("fork() fail");
	else {
		int status;
		pid_ = wait(&status);
		if (status != 0)
			LOG_FATAL("Command '%s' exited with exitcode '%i'", argv[0], status);
	}
}

void compile(const char *compiler, const char **srcs, size_t srcs_count,
             const char **args, size_t args_count) {
	const char **argv = (const char**)malloc((srcs_count + args_count + 2) * sizeof(*argv));
	if (argv == NULL)
		LOG_FATAL("malloc() fail");

	argv[0] = compiler;
	size_t pos = 1;

	for (size_t i = 0; i < srcs_count; ++ i, ++ pos)
		argv[pos] = srcs[i];

	for (size_t i = 0; i < args_count; ++ i, ++ pos)
		argv[pos] = args[i];

	argv[pos] = NULL;

	cmd(argv);
	free(argv);
}

static void embed_str_arr(FILE *f, FILE *o) {
	fprintf(o, "static const char *EMBED_NAME[] = {\n\t\"");

	int ch;
	while ((ch = fgetc(f)) != EOF) {
		switch (ch) {
		case '\t': fprintf(o, "\\t");  break;
		case '\r': fprintf(o, "\\r");  break;
		case '\v': fprintf(o, "\\v");  break;
		case '\f': fprintf(o, "\\f");  break;
		case '\b': fprintf(o, "\\b");  break;
		case '\0': fprintf(o, "\\0");  break;
		case '"':  fprintf(o, "\\\""); break;
		case '\\': fprintf(o, "\\\\"); break;

		case '\n': {
			int next = fgetc(f);
			if (next != EOF)
				fprintf(o, "\",\n\t\"");

			ungetc(next, f);
		} break;

		default:
			if (ch >= ' ' && ch <= '~')
				fprintf(o, "%c", (char)ch);
			else
				fprintf(o, "\\x%02X", ch);
		}
	}

	fprintf(o, "\",\n};\n#undef EMBED_NAME\n");
}

static void embed_bytes(FILE *f, FILE *o) {
	fprintf(o, "static unsigned char EMBED_NAME[] = {\n");

	int byte;
	for (size_t i = 0; (byte = fgetc(f)) != EOF; ++ i) {
		if (i % 10 == 0) {
			if (i > 0)
				fprintf(o, "\n");

			fprintf(o, "\t");
		}

		fprintf(o, "0x%02X, ", byte);
	}

	fprintf(o, "\n};\n#undef EMBED_NAME\n");
}

void embed(const char *path, const char *out, int type) {
	LOG_CUSTOM("EMBED", "'%s' into '%s'", path, out);

	FILE *f = fopen(path, "r");
	if (f == NULL) {
		LOG_ERROR("Failed to open '%s' for embedding", path);
		return;
	}

	FILE *o = fopen(out, "w");
	if (o == NULL) {
		LOG_ERROR("Failed to open '%s' to embed '%s' into it", out, path);
		return;
	}

	fprintf(o, "/* %s */\n", path);

	if (type == STRING_ARRAY)
		embed_str_arr(f, o);
	else
		embed_bytes(f, o);

	fclose(o);
	fclose(f);
}

#ifdef __cplusplus
}
#endif

#endif
