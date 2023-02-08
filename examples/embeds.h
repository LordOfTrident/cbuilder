#ifndef EMBEDS_H_HEADER_GUARD
#define EMBEDS_H_HEADER_GUARD

/* Embed paths macros */
#define EMBED_HELLO_TXT "embed/hello.txt.h"

/* Create the embeds */
#define EMBED() \
	do { \
		embed(RES"/hello.txt", SRC"/"EMBED_HELLO_TXT, STRING_ARRAY); \
	} while (0)

#endif
