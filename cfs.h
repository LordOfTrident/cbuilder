/*
 * https://github.com/LordOfTrident/cfs
 *
 * #define CFS_IMPLEMENTATION
 *
 */

#ifndef CFS_HEADER_GUARD
#define CFS_HEADER_GUARD

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h> /* bool, true, false */
#include <string.h>  /* strlen, strcpy, memcpy, strcat, memset */
#include <stdlib.h>  /* malloc */
#include <stdarg.h>  /* va_list, va_start, va_end, va_arg */
#include <stdint.h>  /* int64_t */

#define CFS_VERSION_MAJOR 1
#define CFS_VERSION_MINOR 7
#define CFS_VERSION_PATCH 2

#ifndef WIN32
#	if defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#		define WIN32
#	endif
#endif

#ifdef WIN32
/* WINVER and _WIN32_WINNT have to be greater or equal to 0x0600 for
   CreateSymbolicLinkA to work on MinGW */
#	ifdef WINVER
#		if WINVER < 0x0600
#			undef  WINVER
#			define WINVER 0x0600
#		endif
#	endif

#	ifdef _WIN32_WINNT
#		if _WIN32_WINNT < 0x0600
#			undef  _WIN32_WINNT
#			define _WIN32_WINNT 0x0600
#		endif
#	endif

#	include <windows.h>

/* Guarantee PATH_MAX to be defined */
#	ifndef PATH_MAX
#		ifndef MAX_PATH
#			define PATH_MAX 1024
#		else
#			define PATH_MAX MAX_PATH
#		endif
#	endif

#	define PATH_SEP "\\"
#else
#	include <unistd.h>
#	include <dirent.h>
#	include <fcntl.h>
#	include <sys/stat.h>
#	include <sys/types.h>

/* Guarantee PATH_MAX to be defined */
#	ifndef PATH_MAX
#		define PATH_MAX 1024
#	endif

#	define PATH_SEP "/"
#endif

enum {
	FS_REGULAR = 0,
	FS_HIDDEN  = 1 << 0,
	FS_DIR     = 1 << 1,
	FS_LINK    = 1 << 2,

	FS_INVALID_ATTR = 1 << 3,
};

typedef struct {
#ifdef WIN32
	WIN32_FIND_DATA _data;
	HANDLE          _handle;

	bool _first;
#else
	DIR *_d;
#endif

	const char *path;
} fs_dir_t;

typedef struct {
#ifdef WIN32
	WIN32_FIND_DATA _data;
#else
	struct dirent *_e;
#endif

	const char *name;
	int         attr;
} fs_ent_t;

#define FOREACH_IN_DIR(PATH, DIR_VAR, ENT_VAR, BODY, STATUS) \
	do { \
		fs_dir_t DIR_VAR; \
		STATUS = fs_dir_open(&DIR_VAR, PATH); \
		if (STATUS == 0) { \
			fs_ent_t ENT_VAR; \
			while (fs_dir_next(&DIR_VAR, &ENT_VAR) == 0) \
				BODY \
			fs_dir_close(&DIR_VAR); \
		} \
	} while (0)

#define FS_JOIN_PATH(...) fs_join_path(__VA_ARGS__, NULL)
char *fs_join_path(const char *base, ...);

int         fs_time(    const char *path, int64_t *m, int64_t *a);
int         fs_attr(    const char *path);
const char *fs_basename(const char *path);
const char *fs_ext(     const char *path);
bool        fs_exists(  const char *path);

char *fs_remove_ext(const char *path);
char *fs_replace_ext(const char *path, const char *new_ext);

int fs_read_link(const char *path, char *buf, size_t size, size_t *written);

int fs_create_link(const char *path, const char *target, bool is_dir);
int fs_create_dir( const char *path);

int fs_remove_file(const char *path);
int fs_remove_dir( const char *path);

int fs_copy_file(const char *path, const char *new_);
int fs_move_file(const char *path, const char *new_);

int fs_dir_open( fs_dir_t *d, const char *path);
int fs_dir_close(fs_dir_t *d);
int fs_dir_next( fs_dir_t *d, fs_ent_t *e);

#ifdef __cplusplus
}
#endif

#endif

#ifdef CFS_IMPLEMENTATION

#ifdef __cplusplus
extern "C" {
#endif

char *fs_join_path(const char *base, ...) {
	size_t len = strlen(base);

	va_list args;
	va_start(args, base);
	for (const char *next = va_arg(args, const char*);
	     next != NULL;
	     next = va_arg(args, const char*)) {
		len += strlen(next) + 1;
	}
	va_end(args);

	char *buf = (char*)malloc(len + 1);
	if (buf == NULL)
		return NULL;

	strcpy(buf, base);

	va_start(args, base);
	for (const char *next = va_arg(args, const char*);
	     next != NULL;
	     next = va_arg(args, const char*)) {
		strcat(buf, PATH_SEP);
		strcat(buf, next);
	}
	va_end(args);

	return buf;
}

bool fs_exists(const char *path) {
#ifdef WIN32
	DWORD attr = GetFileAttributesA(path);
	return attr != INVALID_FILE_ATTRIBUTES;
#else
	return access(path, F_OK) == 0;
#endif
}

const char *fs_basename(const char *path) {
	for (size_t i = strlen(path) - 1; i > 0; -- i) {
		if (path[i] == '\\' || path[i] == '/')
			return path + i + 1;
	}
	return path;
}

const char *fs_ext(const char *path) {
	for (size_t i = strlen(path) - 1; i > 0; -- i) {
		if (path[i] == '.')
			return path + i + 1;
	}
	return path;
}

#ifdef WIN32
uint64_t fs_file_time_to_unix(FILETIME ft) {
}
#endif

int fs_time(const char *path, int64_t *m, int64_t *a) {
#ifdef WIN32
#	define _UNIX_TIME_START  0x019DB1DED53E8000
#	define _TICKS_PER_SECOND 10000000

	HANDLE f = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (f == INVALID_HANDLE_VALUE)
		return -1;

	FILETIME create, access, modif;
	(void)create;
	if (!GetFileTime(f, &create, &access, &modif))
		return -1;

	LARGE_INTEGER li;

	if (m != NULL) {
		li.LowPart  = modif.dwLowDateTime;
		li.HighPart = modif.dwHighDateTime;
		*m = (int64_t)(li.QuadPart - _UNIX_TIME_START) / _TICKS_PER_SECOND;
	}

	if (a != NULL) {
		li.LowPart  = access.dwLowDateTime;
		li.HighPart = access.dwHighDateTime;
		*a = (int64_t)(li.QuadPart - _UNIX_TIME_START) / _TICKS_PER_SECOND;
	}

	return 0;
#else
	struct stat s;
	if (stat(path, &s) != 0)
		return -1;

	if (m != NULL)
		*m = (int64_t)s.st_mtime;
	if (a != NULL)
		*a = (int64_t)s.st_atime;
#endif

	return 0;
}

int fs_attr(const char *path) {
	int         attr = FS_REGULAR;
	const char *base = fs_basename(path);

#ifdef WIN32
	WORD attrs = GetFileAttributesA(path);

	if (attrs & FILE_ATTRIBUTE_HIDDEN || strcmp(base, ".") == 0 || strcmp(base, "..") == 0)
		attr |= FS_HIDDEN;
	if (attrs & FILE_ATTRIBUTE_DIRECTORY)
		attr |= FS_DIR;
	if (attrs & FILE_ATTRIBUTE_REPARSE_POINT)
		attr |= FS_LINK;
#else
	struct stat s;
	if (stat(path, &s) != 0)
		return FS_INVALID_ATTR;

	if (base[0] == '.')
		attr |= FS_HIDDEN;
	if (S_ISDIR(s.st_mode))
		attr |= FS_DIR;
	if (S_ISLNK(s.st_mode))
		attr |= FS_LINK;
#endif

	return attr;
}

char *fs_remove_ext(const char *path) {
	const char *ext = fs_ext(path);
	size_t len      = strlen(path) - strlen(ext) - 1;

	char *removed = (char*)malloc(len + 1);
	if (removed == NULL)
		return NULL;

	memcpy(removed, path, len);
	removed[len] = '\0';

	return removed;
}

char *fs_replace_ext(const char *path, const char *new_ext) {
	size_t len = strlen(path);
	for (size_t i = len - 1; i != (size_t)-1; -- i) {
		if (path[i] == '.') {
			len = i;
			break;
		}
	}

	size_t ext_len = strlen(new_ext);
	char  *str     = (char*)malloc(len + ext_len + 2);
	if (str == NULL)
		return NULL;

	memcpy(str, path, len);
	str[len]     = '.';
	str[len + 1] = '\0';
	strcat(str, new_ext);
	return str;
}

int fs_read_link(const char *path, char *buf, size_t size, size_t *written) {
#ifdef WIN32
	HANDLE handle = CreateFileA(path, FILE_READ_ATTRIBUTES,
	                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0,
	                            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS |
	                            FILE_ATTRIBUTE_REPARSE_POINT | FILE_FLAG_OPEN_REPARSE_POINT, 0);
	if (handle == INVALID_HANDLE_VALUE)
		return -1;

	DWORD n;
	if (!DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, NULL, 0, buf, size, &n, NULL))
		return -1;
#else
	ssize_t n = readlink(path, buf, size);
	if (n < 0)
		return -1;
#endif

	if (written != NULL)
		*written = (size_t)n;

	return 0;
}

int fs_create_link(const char *path, const char *target, bool is_dir) {
#ifdef WIN32
	return !CreateSymbolicLinkA(path, target, is_dir? SYMBOLIC_LINK_FLAG_DIRECTORY : 0)? -1 : 0;
#else
	(void)is_dir;
	return symlink(target, path);
#endif
}

int fs_create_dir(const char *path) {
#ifdef WIN32
	return !CreateDirectoryA(path, NULL)? -1 : 0;
#else
	return mkdir(path, 0777) != 0? -1 : 0;
#endif
}

int fs_remove_dir(const char *path) {
#ifdef WIN32
	return !RemoveDirectoryA(path)? -1 : 0;
#else
	return rmdir(path) != 0? -1 : 0;
#endif
}

int fs_remove_file(const char *path) {
#ifdef WIN32
	return !DeleteFileA(path)? -1 : 0;
#else
	return unlink(path) != 0? -1 : 0;
#endif
}

int fs_copy_file(const char *path, const char *new_) {
#ifdef WIN32
	return !CopyFileA(path, new_, false)? -1 : 0;
#else
	if (fs_exists(new_)) {
		if (fs_remove_file(new_) != 0)
			return -1;
	}

	/* https://stackoverflow.com/questions/2180079/how-can-i-copy-a-file-on-unix-using-c */
	int to, from;

	from = open(path, O_RDONLY);
	if (from < 0)
		return -1;

	to = open(new_, O_WRONLY | O_CREAT | O_EXCL, 0666);
	if (to < 0) {
		close(from);
		return -1;
	}

	char    buf[4096];
	ssize_t read_;
	while (read_ = read(from, buf, sizeof buf), read_ > 0) {
		char   *out_ptr = buf;
		ssize_t written;

		do {
			written = write(to, out_ptr, read_);
			if (written < 0) {
				close(from);
				close(to);
				return -1;
			}

			read_   -= written;
			out_ptr += written;
		} while (read_ > 0);
	}

	if (read_ != 0)
		return -1;

	if (close(from) != 0)
		return -1;
	if (close(to) != 0)
		return -1;

	struct stat s;
	if (stat(path, &s) != 0)
		return -1;

	chmod(new_, s.st_mode);

	return 0;
#endif
}

int fs_move_file(const char *path, const char *new_) {
#ifdef WIN32
	return !MoveFileA(path, new_)? -1 : 0;
#else
	return rename(path, new_) != 0? -1 : 0;
#endif
}

int fs_dir_open(fs_dir_t *d, const char *path) {
#ifdef WIN32
	char pattern[MAX_PATH] = {0};
	strcpy(pattern, path);
	strcat(pattern, "\\*");

	d->_handle = FindFirstFile(pattern, &d->_data);
	if (d->_handle == INVALID_HANDLE_VALUE)
		return -1;

	d->_first = true;
#else
	d->_d = opendir(path);
	if (d->_d == NULL)
		return -1;
#endif

	d->path = path;
	return 0;
}

int fs_dir_close(fs_dir_t *d) {
#ifdef WIN32
	if (!FindClose(d->_handle))
		return -1;
#else
	if (closedir(d->_d) != 0)
		return -1;
#endif

	memset(d, 0, sizeof(*d));
	return 0;
}

int fs_dir_next(fs_dir_t *d, fs_ent_t *e) {
#ifdef WIN32
	if (d->_first)
		d->_first = false;
	else {
		if (FindNextFile(d->_handle, &d->_data) == 0)
			return -1;
	}

	e->_data = d->_data;
	e->name  = e->_data.cFileName;
#else
	e->_e = readdir(d->_d);
	if (e->_e == NULL)
		return -1;

	e->name = e->_e->d_name;
#endif

	char path[PATH_MAX] = {0};
	strcpy(path, d->path);
	strcat(path, "/");
	strcat(path, e->name);

	e->attr = fs_attr(path);
	if (e->attr == FS_INVALID_ATTR)
		return -1;

	return 0;
}


#ifdef __cplusplus
}
#endif

#endif
