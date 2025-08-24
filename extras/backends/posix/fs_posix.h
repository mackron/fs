/*
POSIX file system support.

The handle type for fs_file_open_from_handle() is an `int`:

    fs_file_open_from_handle(pFS, (void*)STDOUT_FILENO, &file);
*/
#ifndef fs_posix_h
#define fs_posix_h

#include "../../../fs.h"

#if defined(__cplusplus)
extern "C" {
#endif

/* BEG fs_posix.h */
extern const fs_backend* FS_POSIX;
/* END fs_posix.h */

#if defined(__cplusplus)
}
#endif
#endif  /* fs_posix_h */
