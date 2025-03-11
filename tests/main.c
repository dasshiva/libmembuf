#include "membuf.h"
#include <stdio.h>
#include <stdlib.h>
#define EXIT_IF_NULL(s, err) \
	if (!s) { \
		printf("%s\n", err); \
		exit(1); \
	}

int main(int argc, const char** argv) {
	MemBuf* buf = mopen(NULL, 0, NULL);
	EXIT_IF_NULL(buf, "Could not open buffer");
	int ver = 1;

	mwrite(buf, 4, "\0asm");
	mwrite(buf, 4, &ver);
	memdump(buf, "artifact.wasm.test");
	mclose(buf);

	return 0;
}
