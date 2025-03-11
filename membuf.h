#ifndef __MEMBUF_H__
#define __MEMBUF_H__

#include <stdint.h>
// The aim is to be as near as possible to <stdio.h>
// So that one doesn't have to spend extra time looking at this header
// All our function are named just like <stdio.h> functions but with a
// "m" prefix.
// So instead of fread, you have mread and instead of fwrite you have
// mwrite. This makes it very easy to integrate this library into code
// For now, only mopenFromFile() and setFlags() defy this philosophy

struct MemBuf;
typedef struct MemBuf MemBuf;

// Since this is a memory stream many normal operations that one can do
// on file streams are unavailable such as fileno(), fmemopen() etc.
// Just enough functions are provided for one to be able to read/write
// to arbitrary locations in memory and move the pointer to arbitrary
// locations

// All functions returning 'int' will return -1 for failure 
// and 0 for success

// All functions returning pointers will return a valid pointer
// on success, and NULL on failure.

// All functions returning uint64_t will return UINT64_MAX on failure
// and any number greater than or equal to 0 otherwise

// To get a detailed error code use, merror()
// You can stringify the error code using merrToString(int)
// So, a typical way to handle failures would be:
//
// MemBuf* buf;
// .....
// int a = mXXXX(buf, arg);
// if (a == -1) {
// 	// deal with the problem here
// 	printf("Failed: %s", merrToString(merror(buf));
// }
// else {
// 	// No error
// }

// check if buffer has any errors
uint8_t merror(MemBuf*);

// convert error code to a string describing the error
// never fails
const char* merrToString(int code);

// check if buffer has anything more to be read
int meof(MemBuf*);

// close the stream and free all underlying memory
// after calling this buf is no longer usable
int mclose(MemBuf* buf);

// Create a new memory stream, initialising it with 'init' contents
// name can be null in which case this is a anonymous stream
// init must not be null if len != 0
MemBuf* mopen(const char* name, uint64_t len, void* init);

// Loads a file to memory and initialises a MemBuf with its contents
// This is equivalent to mopen(filename, fileContents.length(), fileContents)
// file must not be NULL
MemBuf* mopenFromFile(const char* file);

// Set flags associated with the stream
void msetFlags(MemBuf*, int);

// Flush contents to stream (currently is a no-op)
int mflush(MemBuf*);

// get next byte from memory stream
int mgetc(MemBuf*);

// put a byte into stream and advance pointer
int mputc(MemBuf*, int byte);

// query position of pointer in memory stream
uint64_t mtell(MemBuf*);

// write formatted string to mem based on parameters
int mprintf(MemBuf* mem, const char* fmt, ...);

// set position of pointer to pos with respect to 'whence'
int mseek(MemBuf* mem, int whence, int64_t pos);

// read 'len' bytes from pointer and store into ptr, advancing
// pointer by size
uint64_t mread(MemBuf* mem, uint64_t size, void* ptr);

// write 'len' bytes from ptr to stream and advance pointer by 'size'
uint64_t mwrite(MemBuf* mem, uint64_t size, const void* ptr);

// dump contents of stream to 'file'
// creates 'file' if it does not exist and overwrites 'file' if it exists
int memdump(MemBuf* mem, const char* file);

// mgets()/mputs() is not provided as we don't know if the contents 
// in memory is from a text file or not and therefore there is 
// no guarantee that there are newlines to terminate the call 
// to mgets()/mputs()
//
// mscanf() is not provided due to the dangerous nature of the scanf
// family of functions to cause bugs

// values returned by merror
enum {
	MEMBUF_NULL = 1,
	MEMBUF_UNMAP_FAILED,
	MEMBUF_FILE_ACCESS_ERROR,
	MEMBUF_DEST_NULL,
	MEMBUF_INVALID_READ,
	MEMBUF_SRC_NULL,
	MEMBUF_OUT_OF_MEMORY
};
#endif
