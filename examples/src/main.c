#include "print_lines.h"

/* defines embed paths macros (EMBED_HELLO_TXT) */
#include "../embeds.h"

/* embed file EMBED_HELLO_TXT into variable embed_hello_txt */
#define  EMBED_NAME embed_hello_txt
#include EMBED_HELLO_TXT

#define ARRAY_SIZE(ARRAY) (sizeof(ARRAY) / sizeof(ARRAY[0]))

int main(void) {
	print_lines(embed_hello_txt, ARRAY_SIZE(embed_hello_txt));
	return 0;
}
