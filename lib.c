#include "membuf.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#if defined(__linux__) || defined(__ANDROID__)
#include <unistd.h>

#ifdef __GLIBC__
#define __USE_GNU
#endif

#include <sys/mman.h>
#include <sys/stat.h>
#else
#error "Unsupported platform"
#endif

// Align x to PAGE_SIZE
// This macro is inspired by the linux kernel's ALIGN and __ALIGN_MASK macros
// Here, we don't need typeof(x) because x is always a uint64_t
#define PAGE_SIZE_ALIGN(x) (((x) + (PAGE_SZ - 1)) & ~(PAGE_SZ - 1))

static uint32_t PAGE_SZ = 0;

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

	if (!PAGE_SZ) {
		PAGE_SZ = sysconf(_SC_PAGESIZE);
		// PAGE_SIZE is always a power of 2, so check for it
		if (PAGE_SZ & 1) // possibly buggy system, don't continue
			return NULL;
	}

	ret->name = name;
	ret->size = len;
	ret->offset = 0;
	ret->flags = 0;
	ret->capacity = (len == 0) ? PAGE_SZ : PAGE_SIZE_ALIGN(len);
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
		uint64_t status = fwrite(mem->buf, 1, mem->size, fptr);
		if (status != mem->size) {
			mem->flags |= MEMBUF_FILE_ACCESS_ERROR;
			return -1;
		}
	}

	fclose(fptr);
	return 0;
}

uint64_t mread(MemBuf* mem, uint64_t size, void* dest) {
	if (!mem) {
		mem->flags |= MEMBUF_NULL;
		return UINT64_MAX;
	}

	if (!size)
		return 0;

	if (!dest) {
		mem->flags |= MEMBUF_DEST_NULL;
		return UINT64_MAX;
	}

	if (mem->offset + size >= mem->size) {
		mem->flags |= MEMBUF_INVALID_READ;
		return UINT64_MAX;
	}

	memcpy(dest, ((uint8_t*)mem->buf) + mem->offset, size);
	mem->offset += size;
	return size;
}

uint64_t mset(MemBuf* mem, uint32_t n, uint32_t sz, const void* b) {
	for (uint32_t i = 0; i < n; i++) {
		uint64_t status = mwrite(mem, sz, b);
		if (status == UINT64_MAX)
			return status;
	}

	return (uint64_t)n * sz;
}

int mseek(MemBuf* mem, int whence, int64_t pos) {
	if (!mem) {
		mem->flags |= MEMBUF_NULL;
		return -1;
	}

	switch (whence) {
		case MEMBUF_CURRENT: {
			if (!pos)
				return 0;

			if (pos < 0) {
				// check for underflow
				if (mem->offset + pos > mem->offset) {
					mem->flags |= MEMBUF_INVALID_OFFSET;
					return -1;
				}

			}

			else {
				// check for overflow
				if (mem->offset + pos < mem->offset) {
					mem->flags |= MEMBUF_INVALID_OFFSET;
					return -1;
				}

				if (mem->offset + pos > mem->size) {
					mem->flags |= MEMBUF_INVALID_OFFSET;
					return -1;
				}

			}

			mem->offset += pos;
			break;
		}

		case MEMBUF_BEGIN: { // Only +ve pos/0  is allowed
			if (pos < 0) {
				mem->flags |= MEMBUF_INVALID_OFFSET;
				return -1;
			}


			// check for overflow
			if (mem->offset + pos < mem->offset) {
				mem->flags |= MEMBUF_INVALID_OFFSET;
				return -1;
			}


			if (mem->offset + pos > mem->size) {
				mem->flags |= MEMBUF_INVALID_OFFSET;
				return -1;
			}

			mem->offset = pos;
			break;	
		}

		case MEMBUF_END: { // Only -ve pos/0 is allowed
			if (pos > 0) {
				mem->flags |= MEMBUF_INVALID_OFFSET;
				return -1;
			}

			// check for underflow
                        if (mem->offset + pos > mem->offset) {
				mem->flags |= MEMBUF_INVALID_OFFSET;
                                return -1;
			}

			mem->offset = mem->size - 1 - pos; 
		}

		default: {
			mem->flags |= MEMBUF_INVALID_WHENCE;
			return -1;
		}
	};

	return 0;
}

uint64_t mwrite(MemBuf* mem, uint64_t size, const void* src) {
	if (!mem) {
                mem->flags |= MEMBUF_NULL;                         
		return UINT64_MAX;
        }

        if (!size)
		return 0;

        if (!src) {
                mem->flags |= MEMBUF_SRC_NULL;
                return UINT64_MAX;
        }

	if (mem->offset + size < mem->capacity) {
		memcpy(((uint8_t*)mem->buf) + ((mem->offset == 0) 
				? mem->offset : (mem->offset + 1)), src, size);
		mem->offset += (mem->offset == 0) ? size - 1 : size; // mem->offset is 0 indexed
		mem->size += size;       // mem->size is not 0 indexed
	}
	else {
		/* This is a little complicated so read this carefully 
		 * First copy off everything that fits into the current
		 * buffer.
		 *
		 * Then get more memory from the kernel using mremap().
		 * We use mremap() with MREMAP_MAYMOVE to reduce the
		 * chances that mremap() fails as there may be other
		 * mappings around mem->buf that we don't know about
		 *
		 * The kernel does the work of moving the contents 
		 * of the existing memory around so we need not worry
		 * about doing memmove() to the right location.
		 *
		 * However the address at mem->buf is invalidated by the
		 * call to mremap() so we assign the value returned
		 * by mremap back to mem->buf.
		 *
		 * Also since we use buffer relative offsets to access
		 * our contents this invalidation of memory has no
		 * effect on the working of this library
		 *
		 * If this library is modified at any point, do not 
		 * ever use absolute addresses anywhere in the code
		 * or everything breaks
		 */

		uint64_t writable = mem->capacity - mem->size;
		memcpy(((uint8_t*)mem->buf) + mem->offset, src, writable);
		mem->offset += writable;

		// Allocate 8 more pages so that we can reduce calls to mremap() 
		uint64_t ncapacity = PAGE_SIZE_ALIGN(mem->capacity + (size - writable)) + (8 * PAGE_SZ);
		void* new_ptr = mremap(mem->buf, mem->capacity, ncapacity, MREMAP_MAYMOVE);
		if (new_ptr == MAP_FAILED) {
			mem->flags |= MEMBUF_OUT_OF_MEMORY;
			return UINT64_MAX;
		}

		mem->buf = new_ptr;
		memcpy(((uint8_t*)mem->buf) + mem->offset, ((uint8_t*)src) + writable, size - writable);
		mem->size += size;
		mem->offset += (size - writable) - 1;
		mem->capacity = ncapacity;
	}

	return size;
}

static const char* e2s[] = {
	[0] = "Success\n",
	[MEMBUF_NULL] = "MemBuf* is NULL\n",
	[MEMBUF_UNMAP_FAILED] = "Could not unmap allocated memory\n",
	[MEMBUF_INVALID_READ] = "Cannot read more than the length of the stream\n",
	[MEMBUF_DEST_NULL] = "Destination to mread() is NULL\n",
	[MEMBUF_SRC_NULL] = "Source to mwrite() is NULL\n",
	[MEMBUF_OUT_OF_MEMORY] = "System has run out of memory\n",
	[MEMBUF_FILE_ACCESS_ERROR] = "Could not memdump() to file as write failed\n",
	[MEMBUF_INVALID_OFFSET] = "Cannot move file pointer to invalid offfset\n",
	[MEMBUF_INVALID_WHENCE] = "whence value given to mseek() is invalid()\n"
};

const char* merrToString(int code) {
	if (code >= MEMBUF_ERR_MAX)
		return "Unknown error";
	return e2s[code];
}
