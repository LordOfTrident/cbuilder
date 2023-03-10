/*
 * https://github.com/LordOfTrident/clog
 *
 * #define CLOG_IMPLEMENTATION
 *
 */

#ifndef CLOG_H_HEADER_GUARD
#define CLOG_H_HEADER_GUARD

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>   /* FILE, fprintf, stderr */
#include <stdlib.h>  /* exit, EXIT_FAILURE */
#include <time.h>    /* time_t, time, localtime */
#include <stdarg.h>  /* va_list, va_start, va_end, vsnprintf */
#include <stdbool.h> /* bool, true, false */

#define CLOG_VERSION_MAJOR 1
#define CLOG_VERSION_MINOR 2
#define CLOG_VERSION_PATCH 0

#ifndef WIN32
#	if defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#		define WIN32
#	endif
#endif

#ifdef WIN32
#	include <windows.h>
#endif

enum {
	LOG_NONE = 0,
	LOG_TIME = 1 << 0,
	LOG_LOC  = 1 << 1,
};

void log_into(FILE *file);
void log_set_flags(int flags);

#define LOG_INFO( ...) log_info( __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN( ...) log_warn( __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) log_error(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...) log_fatal(__FILE__, __LINE__, __VA_ARGS__)

#define LOG_CUSTOM(TITLE, ...) log_custom(TITLE, __FILE__, __LINE__, __VA_ARGS__)

void log_info( const char *path, size_t line, const char *fmt, ...);
void log_warn( const char *path, size_t line, const char *fmt, ...);
void log_error(const char *path, size_t line, const char *fmt, ...);
void log_fatal(const char *path, size_t line, const char *fmt, ...);

void log_custom(const char *title, const char *path, size_t line, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif

#ifdef CLOG_IMPLEMENTATION

#ifdef __cplusplus
extern "C" {
#endif

enum {
	CLOG_COLOR_INFO = 0,
	CLOG_COLOR_WARN,
	CLOG_COLOR_ERROR,
	CLOG_COLOR_FATAL,
};

#ifdef WIN32
#	define CLOG_RESET_COLOR      _log_color_default
#	define CLOG_TIME_COLOR       FOREGROUND_INTENSITY
#	define CLOG_HIGHLIGHT_COLOR (CLOG_RESET_COLOR | FOREGROUND_INTENSITY)
#	define CLOG_MSG_COLOR       (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)

static WORD _log_color_default = CLOG_MSG_COLOR;

static WORD _log_colors[] = {
	[CLOG_COLOR_INFO]  = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
	[CLOG_COLOR_WARN]  = FOREGROUND_GREEN | FOREGROUND_RED  | FOREGROUND_INTENSITY,
	[CLOG_COLOR_ERROR] = FOREGROUND_RED   | FOREGROUND_INTENSITY,
	[CLOG_COLOR_FATAL] = FOREGROUND_RED   | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
};
#else
#	define CLOG_RESET_COLOR     "\x1b[0m"
#	define CLOG_TIME_COLOR      "\x1b[1;90m"
#	define CLOG_HIGHLIGHT_COLOR "\x1b[1;97m"
#	define CLOG_MSG_COLOR       "\x1b[0m"

static const char *_log_colors[] = {
	[CLOG_COLOR_INFO]  = "\x1b[1;96m",
	[CLOG_COLOR_WARN]  = "\x1b[1;93m",
	[CLOG_COLOR_ERROR] = "\x1b[1;91m",
	[CLOG_COLOR_FATAL] = "\x1b[1;95m",
};
#endif

static FILE *_log_file  = NULL;
static int   _log_flags = LOG_NONE;

void log_set_flags(int flags) {
	_log_flags = flags;
}

void log_into(FILE *file) {
	_log_file = file;
}

static void log_reset_color(void) {
	if (_log_file != stderr && _log_file != stdout)
		return;

#ifdef WIN32
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), CLOG_RESET_COLOR);
#else
	fprintf(_log_file, "%s", CLOG_RESET_COLOR);
#endif
}

static void log_print_title(int color, const char *title) {
	log_reset_color();

	if (_log_file == stderr || _log_file == stdout)
#ifdef WIN32
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), _log_colors[color]);
#else
		fprintf(_log_file, "%s", _log_colors[color]);
#endif

	fprintf(_log_file, "[%s]", title);
	log_reset_color();
}

static void log_print_time(void) {
	time_t raw;
	time(&raw);
	struct tm *info = localtime(&raw);

	if (_log_file == stderr || _log_file == stdout)
#ifdef WIN32
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), CLOG_TIME_COLOR);
#else
		fprintf(_log_file, "%s", CLOG_TIME_COLOR);
#endif

	fprintf(_log_file, "%d:%d:%d", info->tm_hour, info->tm_min, info->tm_sec);
	log_reset_color();
}

static void log_print_loc(const char *path, size_t line) {
#ifdef WIN32
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), CLOG_HIGHLIGHT_COLOR);
#else
	fprintf(_log_file, "%s", CLOG_HIGHLIGHT_COLOR);
#endif
	fprintf(_log_file, " %s:%zu:", path, line);
	log_reset_color();
}

static void log_print_msg(const char *msg) {
#ifdef WIN32
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), CLOG_MSG_COLOR);
#else
	fprintf(_log_file, "%s", CLOG_MSG_COLOR);
#endif

	fprintf(_log_file, " %s\n", msg);
	log_reset_color();
}

static void log_template(int color, const char *title, const char *msg,
                         const char *path, size_t line) {
	if (_log_file == NULL)
		_log_file = stderr;

#ifdef WIN32
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	_log_color_default = csbi.wAttributes;
#endif

	if (_log_flags & LOG_TIME) {
		log_print_time();
		fprintf(_log_file, " ");
	}
	log_print_title(color, title);

	if (_log_flags & LOG_LOC)
		log_print_loc(path, line);

	log_print_msg(msg);
}

void log_info(const char *path, size_t line, const char *fmt, ...) {
	char    buf[256];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	log_template(CLOG_COLOR_INFO, "INFO", buf, path, line);
}

void log_warn(const char *path, size_t line, const char *fmt, ...) {
	char    buf[256];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	log_template(CLOG_COLOR_WARN, "WARN", buf, path, line);
}

void log_error(const char *path, size_t line, const char *fmt, ...) {
	char    buf[256];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	log_template(CLOG_COLOR_ERROR, "ERROR", buf, path, line);
}

void log_fatal(const char *path, size_t line, const char *fmt, ...) {
	char    buf[256];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	log_template(CLOG_COLOR_FATAL, "FATAL", buf, path, line);
	exit(EXIT_FAILURE);
}

void log_custom(const char *title, const char *path, size_t line, const char *fmt, ...) {
	char    buf[256];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	log_template(CLOG_COLOR_INFO, title, buf, path, line);
}

#ifdef __cplusplus
}
#endif

#endif
