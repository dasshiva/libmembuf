#include "membuf.h"
#include <stdio.h>
#include <stdlib.h>
#define EXIT_IF_NULL(s, err) \
	if (!s) { \
		printf("%s\n", err); \
		exit(1); \
	}

#define PRINT_IF_ERROR_UINT64_MAX(s, buf) \
	if (s == UINT64_MAX) { \
		printf("%s", merrToString(merror(buf))); \
                exit(2); \
	}

#define PRINT_IF_ERROR_NEGATIVE(s, buf) \
	if (s == -1) { \
		printf("%s", merrToString(merror(buf))); \
		exit(2); \
	}

int main(int argc, const char** argv) {
	MemBuf* buf = mopen(NULL, 0, NULL);
	EXIT_IF_NULL(buf, "Could not open buffer");
	int ver = 1;

	uint64_t us = writeU32(buf, "\0asm");
	PRINT_IF_ERROR_UINT64_MAX(us, buf);

	us = writeU32(buf, &ver);
	PRINT_IF_ERROR_UINT64_MAX(us, buf);

	int s = memdump(buf, "test-files/artifact.wasm.test");
	PRINT_IF_ERROR_NEGATIVE(s, buf);

	s = mclose(buf);
	PRINT_IF_ERROR_NEGATIVE(s, buf);

	return 0;
}
