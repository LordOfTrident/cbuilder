#include "print_lines.h"

void print_lines(const char **lines, size_t count) {
	for (size_t i = 0; i < count; ++ i)
		printf("%s\n", lines[i]);
}
