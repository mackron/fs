/*
Zip file support.

This only supports STORE and DEFLATE. It does not support DEFLATE64.

To use this, you'll first need to a fs_stream containing a Zip archive file. You can get this
easily from a fs object.

    fs_file* pZipArchiveFile;
    fs_file_open(pFS, "archive.zip", FS_READ, &pZipArchiveFile); // Assumes pFS was initialized earlier.

    ...

    fs* pZip;
    fs_init(FS_ZIP, NULL, fs_file_get_stream(pZipArchiveFile), NULL, &pZip);

    ...

    fs_file* pFileInsideZip;
    fs_file_open(pZip, "file.txt", FS_READ, &pFileInsideZip);

    ... now just read from pFileInsideZip like any other file ...

A Zip archive is its own file system and is therefore implemented as an fs backend. In order to
actually use the backend, it needs to have access to the raw data of the entire Zip file. This is
supplied via a fs_stream object. The fs_file object is a stream and can be used for this purpose.
The code above just opens the Zip file from an earlier created fs object.

Once you have the fs_stream object for the Zip file, you can initialize a fs object, telling it to
use the Zip backend which you do by passing in FS_ZIP, which is declared in this file. If all goes
well, you'll get a pointer to a new fs object representing the Zip archive and you can use it to
open files from within it just like any other file.

You can pass in NULL for the backend config in fs_init().
*/
#ifndef fs_zip_h
#define fs_zip_h

#if defined(__cplusplus)
extern "C" {
#endif

/* BEG fs_zip.h */
extern const fs_backend* FS_ZIP;
/* END fs_zip.h */

#if defined(__cplusplus)
}
#endif
#endif  /* fs_zip_h */
