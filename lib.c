#include "membuf.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__) || defined(__ANDROID__)
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>
#else
#error "Unsupported platform"
#endif

// Align x to a bytes
// This macro is inspired by the linux kernel's ALIGN and __ALIGN_MASK macros
// Here, we don't need typeof(x) because x is always a uint64_t
#define PAGE_SIZE_ALIGN(x) (((x) + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1))

static uint32_t PAGE_SIZE = 0;

struct MemBuf {
	uint64_t    size;
	uint64_t    offset;
	uint64_t    capacity;
	void*       buf;
	uint64_t    flags;
	const char* name;
};


MemBuf* mopen(const char* name, uint64_t len, void* init) {
	MemBuf* ret = malloc(sizeof(MemBuf));
	if (!ret)
		return NULL;

	if (!PAGE_SIZE) {
		PAGE_SIZE = sysconf(_SC_PAGESIZE);
		// PAGE_SIZE is always a power of 2, so check for it
		if (PAGE_SIZE & 1) // possibly buggy system, don't continue
			return NULL;
	}

	ret->name = name;
	ret->size = len;
	ret->offset = 0;
	ret->flags = 0;
	ret->capacity = (len == 0) ? PAGE_SIZE : PAGE_SIZE_ALIGN(len);
	ret->buf = mmap(NULL, ret->capacity, PROT_READ | PROT_WRITE, 
			MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

	if (ret->buf == MAP_FAILED) {
		free(ret);
		return NULL;
	}


	if (len) {
		// if len != 0, init can't be NULL
		if (!init) {
			free(ret);
			return NULL;
		}

		else 
			memcpy(ret->buf, init, ret->size);
	}

	return ret;

}

int mclose(MemBuf* buf) {
	if (!buf) {
		buf->flags |= MEMBUF_NULL;
		return -1;
	}

	// buf->buf is always initialised
	if(munmap(buf->buf, buf->capacity) == -1) {
		buf->flags |= MEMBUF_UNMAP_FAILED;
		return -1;
	}

	free(buf);
	return 0;
}

uint8_t merror(MemBuf* m) {
	// errors are always stored in the low 8 bits of m->flags (if any)
	return (uint8_t)(m->flags & 0xFF);
}

MemBuf* mopenFromFile(const char* file) {
	if (!file)
		return NULL;

	struct stat st;
	if (stat(file, &st) == -1) 
		return NULL;

	FILE* fptr = fopen(file, "r");
	if (!fptr)
		return NULL;

	void* init = NULL;
	if (st.st_size) {
		init = malloc(sizeof(uint8_t) * st.st_size);
		if (!init) 
			return NULL;

		if (fread(init, sizeof(uint8_t), st.st_size, fptr) != st.st_size) {
			free(init);
			return NULL;
		}
	}

	fclose(fptr);
	return mopen(file, st.st_size, init);
}


int mflush(MemBuf* m) {
	// No-op for the moment
	return 0;
}

uint64_t mtell(MemBuf* m) {
	return m->offset;
}

int memdump(MemBuf* mem, const char* file) {
	if (!mem) {
		mem->flags |= MEMBUF_NULL;
		return -1;
	}

	FILE* fptr = fopen(file, "w");
	if (!fptr) {
		mem->flags |= MEMBUF_FILE_ACCESS_ERROR;
		return -1;
	}

	// Write only if there is something to be written
	if (mem->size) {
		if (fwrite(mem->buf, mem->size, 1, fptr) != mem->size) {
			mem->flags |= MEMBUF_FILE_ACCESS_ERROR;
			return -1;
		}
	}

	fclose(fptr);
	return 0;
}
