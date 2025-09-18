/*
In-memory file system backend.

This backend stores all files and directories in memory. This is useful for temporary file
operations, testing, or when you need a virtual file system that doesn't persist to disk.

This supports both reading and writing.
*/
#ifndef fs_mem_h
#define fs_mem_h

#if defined(__cplusplus)
extern "C" {
#endif

/* BEG fs_mem.h */
extern const fs_backend* FS_MEM;
/* END fs_mem.h */

#if defined(__cplusplus)
}
#endif
#endif  /* fs_mem_h */