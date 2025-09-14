# FS API Documentation

---

# fs_result_description

```c
const char* fs_result_description(
    fs_result result
);
```

---

# fs_malloc

```c
void* fs_malloc(
    size_t                         sz,
    const fs_allocation_callbacks* pAllocationCallbacks
);
```

---

# fs_calloc

```c
void* fs_calloc(
    size_t                         sz,
    const fs_allocation_callbacks* pAllocationCallbacks
);
```

---

# fs_realloc

```c
void* fs_realloc(
    void*                          p,
    size_t                         sz,
    const fs_allocation_callbacks* pAllocationCallbacks
);
```

---

# fs_free

```c
void fs_free(
    void*                          p,
    const fs_allocation_callbacks* pAllocationCallbacks
);
```

---

# fs_stream_init

```c
fs_result fs_stream_init(
    const fs_stream_vtable* pVTable,
    fs_stream*              pStream
);
```

---

# fs_stream_read

```c
fs_result fs_stream_read(
    fs_stream* pStream,
    void*      pDst,
    size_t     bytesToRead,
    size_t*    pBytesRead
);
```

---

# fs_stream_write

```c
fs_result fs_stream_write(
    fs_stream*  pStream,
    const void* pSrc,
    size_t      bytesToWrite,
    size_t*     pBytesWritten
);
```

---

# fs_stream_seek

```c
fs_result fs_stream_seek(
    fs_stream*     pStream,
    fs_int64       offset,
    fs_seek_origin origin
);
```

---

# fs_stream_tell

```c
fs_result fs_stream_tell(
    fs_stream* pStream,
    fs_int64*  pCursor
);
```

---

# fs_stream_writef

```c
fs_result fs_stream_writef(
    fs_stream*  pStream,
    const char* fmt,
    ...         
);
```

---

# fs_stream_writef_ex

```c
fs_result fs_stream_writef_ex(
    fs_stream*                     pStream,
    const fs_allocation_callbacks* pAllocationCallbacks,
    const char*                    fmt,
    ...                            
);
```

---

# fs_stream_writefv

```c
fs_result fs_stream_writefv(
    fs_stream*  pStream,
    const char* fmt,
    va_list     args
);
```

---

# fs_stream_writefv_ex

```c
fs_result fs_stream_writefv_ex(
    fs_stream*                     pStream,
    const fs_allocation_callbacks* pAllocationCallbacks,
    const char*                    fmt,
    va_list                        args
);
```

---

# fs_stream_duplicate

```c
fs_result fs_stream_duplicate(
    fs_stream*                     pStream,
    const fs_allocation_callbacks* pAllocationCallbacks,
    fs_stream**                    ppDuplicatedStream
);
```

Duplicates a stream.

This will allocate the new stream on the heap. The caller is responsible for freeing the stream
with `fs_stream_delete_duplicate()` when it's no longer needed.

---

# fs_stream_delete_duplicate

```c
void fs_stream_delete_duplicate(
    fs_stream*                     pDuplicatedStream,
    const fs_allocation_callbacks* pAllocationCallbacks
);
```

Deletes a duplicated stream.

Do not use this for a stream that was not duplicated with `fs_stream_duplicate()`.

---

# fs_stream_read_to_end

```c
fs_result fs_stream_read_to_end(
    fs_stream*                     pStream,
    fs_format                      format,
    const fs_allocation_callbacks* pAllocationCallbacks,
    void**                         ppData,
    size_t*                        pDataSize
);
```

---

# fs_sysdir

```c
size_t fs_sysdir(
    [in]            fs_sysdir_type type,
    [out, optional] char*          pDst,
    [in]            size_t         dstCap
);
```

Get the path of a known system directory.

The returned path will be null-terminated. If the output buffer is too small, the required size
will be returned, not including the null terminator.

## Parameters

[in] **type**  
The type of system directory to query. See `fs_sysdir_type` for recognized values.

[out, optional] **pDst**  
A pointer to a buffer that will receive the path. If NULL, the function will return the
required length of the buffer, not including the null terminator.

[in] **dstCap**  
The capacity of the output buffer, in bytes. This is ignored if `pDst` is NULL.

## Return Value

Returns the length of the string, not including the null terminator. Returns 0 on failure. If the
return value is >= to `dstCap` it means the output buffer was too small. Use the returned value to
know how big to make the buffer.

## Example 1 - Querying the Home Directory

```c
size_t len = fs_sysdir(FS_SYSDIR_HOME, NULL, 0);
if (len == 0) {
    // Failed to query the length of the home directory path.
}

char* pPath = (char*)malloc(len + 1);  // +1 for null terminator.
if (pPath == NULL) {
    // Out of memory.
}

len = fs_sysdir(FS_SYSDIR_HOME, pPath, len + 1);
if (len == 0) {
    // Failed to get the home directory path.
}

printf("Home directory: %s\n", pPath);
free(pPath);
```

## See Also

[fs_sysdir_type](#fs_sysdir_type)  
[fs_mktmp()](#fs_mktmp)  

---

# fs_mktmp

```c
fs_result fs_mktmp(
    [in]  const char* pPrefix,
    [out] char*       pTmpPath,
    [in]  size_t      tmpPathCap,
    [in]  int         options
);
```

Create a temporary file or directory.

This function creates a temporary file or directory with a unique name based on the provided
prefix. The full path to the created file or directory is returned in `pTmpPath`.

Use the option flag `FS_MKTMP_FILE` to create a temporary file, or `FS_MKTMP_DIR` to create a
temporary directory.

## Parameters

[in] **pPrefix**  
A prefix for the temporary file or directory name. This should not include the system's base
temp directory path. Do not include paths like "/tmp" in the prefix. The output path will
include the system's base temp directory and the prefix.

The prefix can include subdirectories, such as "myapp/subdir". In this case the library will
create the directory hierarchy for you, unless you pass in `FS_NO_CREATE_DIRS`. Note that not
all platforms treat the name portion of the prefix the same. In particular, Windows will only
use up to the first 3 characters of the name portion of the prefix.

[out] **pTmpPath**  
A pointer to a buffer that will receive the full path of the created temporary file or
directory. This will be null-terminated.

[in] **tmpPathCap**  
The capacity of the output buffer, in bytes.

[in] **options**  
Options for creating the temporary file or directory. Can be a combination of the following:

| Option | Description |
|:-------|:------------|
| `FS_MKTMP_FILE` | Creates a temporary file. Cannot be used with FS_MKTMP_DIR. |
| `FS_MKTMP_DIR` | Creates a temporary directory. Cannot be used with FS_MKTMP_FILE. |
| `FS_NO_CREATE_DIRS` | Do not create parent directories if they do not exist. If this flag is not set, parent directories will be created as needed. |


## Return Value

Returns `FS_SUCCESS` on success; any other error code on failure. Will return `FS_PATH_TOO_LONG` if
the output buffer is too small.

---

# fs_archive_type_init

```c
fs_archive_type fs_archive_type_init(
    const fs_backend* pBackend,
    const char*       pExtension
);
```

---

# fs_config_init_default

```c
fs_config fs_config_init_default(
);
```

---

# fs_config_init

```c
fs_config fs_config_init(
    const fs_backend* pBackend,
    const void*       pBackendConfig,
    fs_stream*        pStream
);
```

---

# fs_init

```c
fs_result fs_init(
    [in, optional] const fs_config* pConfig,
    [out]          fs**             ppFS
);
```

Initializes a file system object.

This is the main object that you will use to open files. There are different types of file system
backends, such as the standard file system, ZIP archives, etc. which you can configure via the
config.

The config is used to select which backend to use and to register archive types against known
file extensions. If you just want to use the regular file system and don't care about archives,
you can just pass in NULL for the config.

By registering archive types, you'll be able to open files from within them straight from a file
path without without needing to do any manual management. For example, if you register ZIP archives
to the ".zip" extension, you can open a file from a path like this:

    somefolder/archive.zip/somefile.txt

These can also be handled transparently, so the above path can be opened with this:

    somefolder/somefile.txt

Note that the `archive.zip` part is not needed. If you want this functionality, you must register
the archive types with the config.

Most of the time you will use a `fs` object that represents the normal file system, which is the
default backend if you don't pass in a config, but sometimes you may want to have a `fs` object
that represents an archive, such as a ZIP archive. To do this, you need to provide a stream that
reads the actual data of the archive. Most of the time you will just use the stream provided by
a `fs_file` object you opened earlier from the regular file system, but if you would rather source
your data from elsewhere, like a memory buffer, you can pass in your own stream. You also need to
specify the backend to use, such as `FS_ZIP` in the case of ZIP archives. See examples below for
more information.

If you want to use custom allocation callbacks, you can do so by passing in a pointer to a
`fs_allocation_callbacks` struct into the config. If you pass in NULL, the default allocation
callbacks which use malloc/realloc/free will be used. If you pass in non-NULL, this function will
make a copy of the struct, so you can free or modify the struct after this function returns.

## Parameters

[in, optional] **pConfig**  
A pointer to a configuration struct. Can be NULL, in which case the regular file system will be
used, and archives will not be supported unless explicitly mounted later with `fs_mount_fs()`.

[out] **ppFS**  
A pointer to a pointer which will receive the initialized file system object. The object must
be uninitialized with `fs_uninit()` when no longer needed.

## Return Value

Returns FS_SUCCESS on success; any other result code otherwise.

## Example 1 - Basic Usage

The example below shows how to initialize a `fs` object which uses the regular file system and does
not support archives. This is the most basic usage of the `fs` object.

```c
#include "fs.h"

...

fs* pFS;
fs_result result = fs_init(NULL, &pFS);
if (result != FS_SUCCESS) {
    // Handle error.
}

...

fs_uninit(pFS);
```

## Example 2 - Supporting Archives

The example below shows how to initialize a `fs` object which uses the regular file system and
supports ZIP archives. Error checking has been omitted for clarity.

```c
#include "fs.h"
#include "extras/backends/zip/fs_zip.h"  // For FS_ZIP backend.

...

fs* pFS;
fs_config fsConfig;

// Archive types are supported by mapping a backend (`FS_ZIP` in this case) to a file extension.
fs_archive_type pArchiveTypes[] =
{
    {FS_ZIP, "zip"}
};

// The archive types are registered via the config.
fsConfig = fs_config_init_default();
fsConfig.pArchiveTypes    = pArchiveTypes;
fsConfig.archiveTypeCount = sizeof(pArchiveTypes) / sizeof(pArchiveTypes[0]);

// Once the config is ready, initialize the fs object.
fs_init(&fsConfig, &pFS);

// Now you can open files from within ZIP archives from a file path.
fs_file* pFileInArchive;
fs_file_open(pFS, "somefolder/archive.zip/somefile.txt", FS_READ, &pFileInArchive);
```

## Example 3 - Archive Backends

This example shows how you can open an archive file directly, and then create a new `fs` object
which uses the archive as its backend. This is useful if, for example, you want to use a ZIP file
as a virtual file system.

```c
#include "fs.h"
#include "extras/backends/zip/fs_zip.h"  // For FS_ZIP backend.

...

fs* pFS;                // Main file system object.
fs* pArchiveFS;         // File system object for the archive.
fs_config archiveConfig;
fs_file* pArchiveFile;  // The actual archive file.

// Open the archive file itself first, usually from the regular file system.
fs_file_open(pFS, "somefolder/archive.zip", FS_READ, &pArchiveFile);

...

// Setup the config for the archive `fs` object such that it uses the ZIP backend (FS_ZIP), and
// reads from the stream of the actual archive file (pArchiveFile) which was opened earlier.
archiveConfig = fs_config_init(FS_ZIP, NULL, fs_file_get_stream(pArchiveFile));

// With the config ready we can now initialize the `fs` object for the archive.
fs_init(&archiveConfig, &pArchiveFS);

...

// Now that we have a `fs` object representing the archive, we can open files from within it like
// normal.
fs_file* pFileInArchive;
fs_file_open(pArchiveFS, "fileinsidearchive.txt", FS_READ, &pFileInArchive);
```

## See Also

[fs_uninit()](#fs_uninit)  

---

# fs_uninit

```c
void fs_uninit(
    [in] fs* pFS
);
```

Uninitializes a file system object.

This does not do any file closing for you. You must close any opened files yourself before calling
this function.

## Parameters

[in] **pFS**  
A pointer to the file system object to uninitialize. Must not be NULL.

## See Also

[fs_init()](#fs_init)  

---

# fs_ioctl

```c
fs_result fs_ioctl(
    [in]           fs*   pFS,
    [in]           int   op,
    [in, optional] void* pArg
);
```

Performs a control operation on the file system.

This is backend-specific. Check the documentation for the backend you are using to see what
operations are supported.

## Parameters

[in] **pFS**  
A pointer to the file system object. Must not be NULL.

[in] **op**  
The operation to perform. This is backend-specific.

[in, optional] **pArg**  
An optional pointer to an argument struct. This is backend-specific. Can be NULL if the
operation does not require any arguments.

## Return Value

Returns FS_SUCCESS on success; any other result code otherwise. May return FS_NOT_IMPLEMENTED if
the operation is not supported by the backend.

---

# fs_remove

```c
fs_result fs_remove(
    [in, optional] fs*         pFS,
    [in]           const char* pFilePath,
    [in]           int         options
);
```

Removes a file or empty directory.

This function will delete a file or an empty directory from the file system. It will consider write
mount points unless the FS_IGNORE_MOUNTS flag is specified in the options parameter in which case
the path will be treated as a real path.

See fs_file_open() for information about the options flags.

## Parameters

[in, optional] **pFS**  
A pointer to the file system object. Can be NULL to use the native file system.

[in] **pFilePath**  
The path to the file or directory to remove. Must not be NULL.

[in] **options**  
Options for the operation. Can be 0 or a combination of the following flags:

| Option |
|:-------|
| `FS_IGNORE_MOUNTS` |
| `FS_NO_SPECIAL_DIRS` |
| `FS_NO_ABOVE_ROOT_NAVIGATION` |


## Return Value

Returns FS_SUCCESS on success; any other result code otherwise. Returns FS_DOES_NOT_EXIST if the
file or directory does not exist. Returns FS_DIRECTORY_NOT_EMPTY if attempting to remove a
non-empty directory.

## See Also

[fs_file_open()](#fs_file_open)  

---

# fs_rename

```c
fs_result fs_rename(
    [in, optional] fs*         pFS,
    [in]           const char* pOldPath,
    [in]           const char* pNewPath,
    [in]           int         options
);
```

Renames or moves a file or directory.

This function will rename or move a file or directory from one location to another. It will
consider write mount points unless the FS_IGNORE_MOUNTS flag is specified in the options parameter
in which case the paths will be treated as real paths.

This will fail with FS_DIFFERENT_DEVICE if the source and destination are on different devices.

See fs_file_open() for information about the options flags.

## Parameters

[in, optional] **pFS**  
A pointer to the file system object. Can be NULL to use the native file system.

[in] **pOldPath**  
The current path of the file or directory to rename/move. Must not be NULL.

[in] **pNewPath**  
The new path for the file or directory. Must not be NULL.

[in] **options**  
Options for the operation. Can be 0 or a combination of the following flags:

| Option |
|:-------|
| `FS_IGNORE_MOUNTS` |
| `FS_NO_SPECIAL_DIRS` |
| `FS_NO_ABOVE_ROOT_NAVIGATION` |


## Return Value

Returns FS_SUCCESS on success; any other result code otherwise. Returns FS_DOES_NOT_EXIST if the
source file or directory does not exist. Returns FS_ALREADY_EXISTS if the destination path already
exists.

## See Also

[fs_file_open()](#fs_file_open)  

---

# fs_mkdir

```c
fs_result fs_mkdir(
    [in, optional] fs*         pFS,
    [in]           const char* pPath,
    [in]           int         options
);
```

Creates a directory.

This function creates a directory at the specified path. By default, it will create the entire
directory hierarchy if parent directories do not exist. It will consider write mount points unless
the FS_IGNORE_MOUNTS flag is specified in the options parameter in which case the path will be
treated as a real path.

See fs_file_open() for information about the options flags.

## Parameters

[in, optional] **pFS**  
A pointer to the file system object. Can be NULL to use the native file system.

[in] **pPath**  
The path of the directory to create. Must not be NULL.

[in] **options**  
Options for the operation. Can be 0 or a combination of the following flags:

| Option |
|:-------|
| `FS_IGNORE_MOUNTS` |
| `FS_NO_CREATE_DIRS` |


## Return Value

Returns FS_SUCCESS on success; any other result code otherwise. Returns FS_ALREADY_EXISTS if the
directory already exists. Returns FS_DOES_NOT_EXIST if FS_NO_CREATE_DIRS is specified and a
parent directory does not exist.

## See Also

[fs_file_open()](#fs_file_open)  

---

# fs_info

```c
fs_result fs_info(
    [in, optional] fs*           pFS,
    [in]           const char*   pPath,
    [in]           int           openMode,
    [out]          fs_file_info* pInfo
);
```

Retrieves information about a file or directory without opening it.

This function gets information about a file or directory such as its size, modification time,
and whether it is a directory or symbolic link. The openMode parameter accepts the same flags as
fs_file_open() but FS_READ, FS_WRITE, FS_TRUNCATE, FS_APPEND, and FS_EXCLUSIVE are ignored.

## Parameters

[in, optional] **pFS**  
A pointer to the file system object. Can be NULL to use the native file system.

[in] **pPath**  
The path to the file or directory to get information about. Must not be NULL.

[in] **openMode**  
Open mode flags that may affect how the file is accessed. See fs_file_open() for details.

[out] **pInfo**  
A pointer to a fs_file_info structure that will receive the file information. Must not be NULL.

## Return Value

Returns FS_SUCCESS on success; any other result code otherwise. Returns FS_DOES_NOT_EXIST if the
file or directory does not exist.

## See Also

[fs_file_get_info()](#fs_file_get_info)  
[fs_file_open()](#fs_file_open)  

---

# fs_get_stream

```c
fs_stream* fs_get_stream(
    [in] fs* pFS
);
```

Retrieves a pointer to the stream used by the file system object.

This is only relevant if the file system will initialized with a stream (such as when opening an
archive). If the file system was not initialized with a stream, this will return NULL.

## Parameters

[in] **pFS**  
A pointer to the file system object. Must not be NULL.

## Return Value

Returns a pointer to the stream used by the file system object, or NULL if no stream was provided
at initialization time.

---

# fs_get_allocation_callbacks

```c
const fs_allocation_callbacks* fs_get_allocation_callbacks(
    [in] fs* pFS
);
```

Retrieves a pointer to the allocation callbacks used by the file system object.

Note that this will *not* return the same pointer that was specified in the config at initialization
time. This function returns a pointer to the internal copy of the struct.

## Parameters

[in] **pFS**  
A pointer to the file system object. Must not be NULL.

## Return Value

Returns a pointer to the allocation callbacks used by the file system object. If `pFS` is NULL, this
will return NULL.

---

# fs_get_backend_data

```c
void* fs_get_backend_data(
    [in] fs* pFS
);
```

For use only by backend implementations. Retrieves a pointer to the backend-specific data
associated with the file system object.

You should never call this function unless you are implementing a custom backend. The size of the
data can be retrieved with `fs_get_backend_data_size()`.

## Parameters

[in] **pFS**  
A pointer to the file system object. Must not be NULL.

## Return Value

Returns a pointer to the backend-specific data associated with the file system object, or NULL if no
backend data is available.

## See Also

[fs_get_backend_data_size()](#fs_get_backend_data_size)  

---

# fs_get_backend_data_size

```c
size_t fs_get_backend_data_size(
    [in] fs* pFS
);
```

For use only by backend implementations. Retrieves the size of the backend-specific data
associated with the file system object.

You should never call this function unless you are implementing a custom backend. The data can be
accessed with `fs_get_backend_data()`.

## Parameters

[in] **pFS**  
A pointer to the file system object. Must not be NULL.

## Return Value

Returns the size of the backend-specific data associated with the file system object, or 0 if no
backend data is available.

## See Also

[fs_get_backend_data()](#fs_get_backend_data)  

---

# fs_ref

```c
fs* fs_ref(
    [in] fs* pFS
);
```

Increments the reference count of the file system object.

This function would be used to prevent garbage collection of opened archives. It should be rare to
ever need to call this function directly.

## Parameters

[in] **pFS**  
A pointer to the file system object. Must not be NULL.

## Return Value

Returns `pFS`.

## See Also

[fs_unref()](#fs_unref)  
[fs_refcount()](#fs_refcount)  

---

# fs_unref

```c
fs_uint32 fs_unref(
    [in] fs* pFS
);
```

Decrements the reference count of the file system object.

This does not uninitialize the object once the reference count hits zero.

## Parameters

[in] **pFS**  
A pointer to the file system object. Must not be NULL.

## Return Value

Returns the new reference count.

## See Also

[fs_ref()](#fs_ref)  
[fs_refcount()](#fs_refcount)  

---

# fs_refcount

```c
fs_uint32 fs_refcount(
    [in] fs* pFS
);
```

Retrieves the current reference count of the file system object.

## Parameters

[in] **pFS**  
A pointer to the file system object. Must not be NULL.

## Return Value

Returns the current reference count of the file system object.

## See Also

[fs_ref()](#fs_ref)  
[fs_unref()](#fs_unref)  

---

# fs_file_open

```c
fs_result fs_file_open(
    [in, optional] fs*         pFS,
    [in]           const char* pFilePath,
    [in]           int         openMode,
    [out]          fs_file**   ppFile
);
```

Opens a file.

If the file path is prefixed with the virtual path of a mount point, this function will first try
opening the file from that mount. If that fails, it will fall back to the native file system and
treat the path as a real path. If the FS_ONLY_MOUNTS flag is specified in the openMode parameter,
the last step of falling back to the native file system will be skipped.

By default, opening a file will transparently look inside archives of known types (registered at
initialization time of the `fs` object). This can slow, and if you would rather not have this
behavior, consider using the `FS_OPAQUE` option (see below).

This function opens a file for reading and/or writing. The openMode parameter specifies how the
file should be opened. It can be a combination of the following flags:


| Option | Description |
|:-------|:------------|
| `FS_READ` | Open the file for reading. If used with `FS_WRITE`, the file will be opened in read/write mode. When opening in read-only mode, the file must exist. |
| `FS_WRITE` | Open the file for writing. If used with `FS_READ`, the file will be opened in read/write mode. When opening in write-only mode, the file will be created if it does not exist. By default, the file will be opened in overwrite mode. To change this, combine this with either one of the `FS_TRUNCATE` or `FS_APPEND` flags. |
| `FS_TRUNCATE` | Only valid when used with `FS_WRITE`. If the file already exists, it will be truncated to zero length when opened. If the file does not exist, it will be created. Not compatible with `FS_APPEND`. |
| `FS_APPEND` | Only valid when used with `FS_WRITE`. All writes will occur at the end of the file, regardless of the current cursor position. If the file does not exist, it will be created. Not compatible with `FS_TRUNCATE`. |
| `FS_EXCLUSIVE` | Only valid when used with `FS_WRITE`. The file will be created, but if it already exists, the open will fail with FS_ALREADY_EXISTS. |
| `FS_TRANSPARENT` | This is the default behavior. When used, files inside archives can be opened transparently. For example, "somefolder/archive.zip/file.txt" can be opened with "somefolder/file.txt" (the "archive.zip" part need not be specified). This assumes the `fs` object has been initialized with support for the relevant archive types.<br><br>Transparent mode is the slowest mode since it requires searching through the file system for archives, and then open those archives, and then searching through the archive for the file. If this is prohibitive, consider using `FS_OPAQUE` (fastest) or `FS_VERBOSE` modes instead.<br><br>Furthermore, you can consider having a rule in your application that instead of opening files inside archives from a transparent path, that you instead mount the archive, and then open all files with `FS_OPAQUE`, but with a virtual path that points to the archive. For example: <br>    <br>    fs_mount(pFS, "somefolder/archive.zip", "assets", FS_READ);<br>    fs_file_open(pFS, "assets/file.txt", FS_READ \| FS_OPAQUE, &pFile);<br><br>Here the archive is mounted to the virtual path "assets". Because the path "assets/file.txt" is prefixed with "assets", the file system knows to look inside the mounted archive without having to search for it. |
| `FS_OPAQUE` | When used, archives will be treated as opaque, meaning attempting to open a file from an unmounted archive will fail. For example, "somefolder/archive.zip/file.txt" will fail because it is inside an archive. This is the fastest mode, but you will not be able to open files from inside archives unless it is sourced from a mount. |
| `FS_VERBOSE` | When used, files inside archives can be opened, but the name of the archive must be specified explicitly in the path, such as "somefolder/archive.zip/file.txt". This is faster than `FS_TRANSPARENT` mode since it does not require searching for archives. |
| `FS_NO_CREATE_DIRS` | When opening a file in write mode, the default behavior is to create the directory structure automatically if required. When this options is used, directories will *not* be created automatically. If the necessary parent directories do not exist, the open will fail with FS_DOES_NOT_EXIST. |
| `FS_IGNORE_MOUNTS` | When used, mounted directories and archives will be ignored when opening files. The path will be treated as a real path. |
| `FS_ONLY_MOUNTS` | When used, only mounted directories and archives will be considered when opening files. When a file is opened, it will first search through mounts, and if the file is not found in any of those it will fall back to the native file system and try treating the path as a real path. When this flag is set, that last step of falling back to the native file system is skipped. |
| `FS_NO_SPECIAL_DIRS` | When used, the presence of special directories like "." and ".." in the path will result in an error. When using this option, you need not specify FS_NO_ABOVE_ROOT_NAVIGATION since it is implied. |
| `FS_NO_ABOVE_ROOT_NAVIGATION` | When used, navigating above the mount point with leading ".." segments will result in an error. This option is implied when using FS_NO_SPECIAL_DIRS. Close the file with `fs_file_close()` when no longer needed. The file will not be closed automatically when the `fs` object is uninitialized. |


## Parameters

[in, optional] **pFS**  
A pointer to the file system object. Can be NULL to use the native file system. Note that when
this is NULL, archives and mounts will not be supported.

[in] **pFilePath**  
The path to the file to open. Must not be NULL.

[in] **openMode**  
The mode to open the file with. A combination of the flags described above.

[out] **ppFile**  
A pointer to a pointer which will receive the opened file object. Must not be NULL.

## Return Value

Returns FS_SUCCESS on success; any other result code otherwise. Returns FS_DOES_NOT_EXIST if the
file does not exist when opening for reading. Returns FS_ALREADY_EXISTS if the file already exists
when opening with FS_EXCLUSIVE. Returns FS_IS_DIRECTORY if the path refers to a directory.

## Example 1 - Basic Usage

The example below shows how to open a file for reading from the regular file system.

```c
fs_result result;
fs_file* pFile;

result = fs_file_open(pFS, "somefolder/somefile.txt", FS_READ, &pFile);
if (result != FS_SUCCESS) {
    // Handle error.
}

// Use the file...

// Close the file when no longer needed.
fs_file_close(pFile);
```

## Example 2 - Opening from a Mount

The example below shows how to mount a directory and then open a file from it. Error checking has
been omitted for clarity.

```c
// "assets" is the virtual path. FS_READ indicates this is a mount for use when opening a file in
// read-only mode (write mounts would use FS_WRITE).
fs_mount(pFS, "some_actual_path", "assets", FS_READ);

...

// The file path is prefixed with the virtual path "assets" so the file system will look inside the
// mounted directory first. Since the "assets" virtual path points to "some_actual_path", the file
// that will actually be opened is "some_actual_path/file.txt".
fs_file_open(pFS, "assets/file.txt", FS_READ, &pFile);
```

## Example 3 - Opening from an Archive

The example below shows how to open a file from within a ZIP archive. This assumes the `fs` object
was initialized with support for ZIP archives (see fs_init() documentation for more information).

```c
// Opening the file directly from the archive by specifying the archive name in the path.
fs_file_open(pFS, "somefolder/archive.zip/somefile.txt", FS_READ, &pFile);

// Same as above. The "archive.zip" part is not needed because transparent mode is used by default.
fs_file_open(pFS, "somefolder/somefile.txt", FS_READ, &pFile);

// Opening a file in verbose mode. The archive name must be specified in the path.
fs_file_open(pFS, "somefolder/archive.zip/somefile.txt", FS_READ | FS_VERBOSE, &pFile); // This is a valid path in verbose mode.

// This will fail because the archive name is not specified in the path.
fs_file_open(pFS, "somefolder/somefile.txt", FS_READ | FS_VERBOSE, &pFile); // This will fail because verbose mode requires explicit archive names.

// This will fail because opaque mode treats archives as opaque.
fs_file_open(pFS, "somefolder/archive.zip/somefile.txt", FS_READ | FS_OPAQUE, &pFile);
```

## Example 4 - Opening from a Mounted Archive

It is OK to use opaque mode when opening files from a mounted archive. This is the only way to open
files from an archive when using opaque mode.

```c
// Mount the archive to the virtual path "assets".
fs_mount(pFS, "somefolder/archive.zip", "assets", FS_READ);

// Now you can open files from the archive in opaque mode. Note how the path is prefixed with the
// virtual path "assets" which is how the mapping back to "somefolder/archive.zip" is made.
fs_file_open(pFS, "assets/somefile.txt", FS_READ | FS_OPAQUE, &pFile);
```

## See Also

[fs_file_close()](#fs_file_close)  
[fs_file_read()](#fs_file_read)  
[fs_file_write()](#fs_file_write)  
[fs_file_seek()](#fs_file_seek)  
[fs_file_tell()](#fs_file_tell)  
[fs_file_flush()](#fs_file_flush)  
[fs_file_truncate()](#fs_file_truncate)  
[fs_file_get_info()](#fs_file_get_info)  
[fs_file_duplicate()](#fs_file_duplicate)  

---

# fs_file_close

```c
void fs_file_close(
    [in] fs_file* pFile
);
```

Closes a file.

You must close any opened files with this function when they are no longer needed. The owner `fs`
object will not close files automatically when it is uninitialized with `fs_uninit()`.

## Parameters

[in] **pFile**  
A pointer to the file to close. Must not be NULL.

## See Also

[fs_file_open()](#fs_file_open)  

---

# fs_file_read

```c
fs_result fs_file_read(
    [in]            fs_file* pFile,
    [out]           void*    pDst,
    [in]            size_t   bytesToRead,
    [out, optional] size_t*  pBytesRead
);
```

Reads data from a file.

This function reads up to `bytesToRead` bytes from the file into the buffer pointed to by `pDst`.
The number of bytes actually read will be stored in the variable pointed to by `pBytesRead`.

If the end of the file is reached before any bytes are read, this function will return `FS_AT_END`
and `*pBytesRead` will be set to 0. `FS_AT_END` will only be returned if `*pBytesRead` is 0.

## Parameters

[in] **pFile**  
A pointer to the file to read from. Must not be NULL.

[out] **pDst**  
A pointer to the buffer that will receive the read data. Must not be NULL.

[in] **bytesToRead**  
The maximum number of bytes to read from the file.

[out, optional] **pBytesRead**  
A pointer to a variable that will receive the number of bytes actually read. Can be NULL if you
do not care about this information. If NULL, the function will return an error if not all
requested bytes could be read.

## Return Value

Returns `FS_SUCCESS` on success, `FS_AT_END` on end of file, or an error code otherwise. Will only
return `FS_AT_END` if `*pBytesRead` is 0.

If `pBytesRead` is NULL, the function will return an error if not all requested bytes could be
read. Otherwise, if `pBytesRead` is not NULL, the function will return `FS_SUCCESS` even if fewer
than `bytesToRead` bytes were read.

## See Also

[fs_file_open()](#fs_file_open)  
[fs_file_write()](#fs_file_write)  
[fs_file_seek()](#fs_file_seek)  
[fs_file_tell()](#fs_file_tell)  

---

# fs_file_write

```c
fs_result fs_file_write(
    [in]            fs_file*    pFile,
    [in]            const void* pSrc,
    [in]            size_t      bytesToWrite,
    [out, optional] size_t*     pBytesWritten
);
```

Writes data to a file.

This function writes up to `bytesToWrite` bytes from the buffer pointed to by `pSrc` to the file.
The number of bytes actually written will be stored in the variable pointed to by `pBytesWritten`.

## Parameters

[in] **pFile**  
A pointer to the file to write to. Must not be NULL.

[in] **pSrc**  
A pointer to the buffer containing the data to write. Must not be NULL.

[in] **bytesToWrite**  
The number of bytes to write to the file.

[out, optional] **pBytesWritten**  
A pointer to a variable that will receive the number of bytes actually written. Can be NULL if
you do not care about this information. If NULL, the function will return an error if not all
requested bytes could be written.

## Return Value

Returns `FS_SUCCESS` on success, or an error code otherwise.

If `pBytesWritten` is NULL, the function will return an error if not all requested bytes could be
written. Otherwise, if `pBytesWritten` is not NULL, the function will return `FS_SUCCESS` even if
fewer than `bytesToWrite` bytes were written.

## See Also

[fs_file_open()](#fs_file_open)  
[fs_file_read()](#fs_file_read)  
[fs_file_seek()](#fs_file_seek)  
[fs_file_tell()](#fs_file_tell)  
[fs_file_flush()](#fs_file_flush)  
[fs_file_truncate()](#fs_file_truncate)  

---

# fs_file_writef

```c
fs_result fs_file_writef(
    [in] fs_file*    pFile,
    [in] const char* fmt,
    [in] ...         
);
```

A helper for writing formatted data to a file.

## Parameters

[in] **pFile**  
A pointer to the file to write to. Must not be NULL.

[in] **fmt**  
A printf-style format string. Must not be NULL.

[in] **...**  
Additional arguments as required by the format string.

## Return Value

Same as `fs_file_write()`.

## See Also

[fs_file_write()](#fs_file_write)  
[fs_file_writefv()](#fs_file_writefv)  

---

# fs_file_writefv

```c
fs_result fs_file_writefv(
    [in] fs_file*    pFile,
    [in] const char* fmt,
    [in] va_list     args
);
```

A helper for writing formatted data to a file.

## Parameters

[in] **pFile**  
A pointer to the file to write to. Must not be NULL.

[in] **fmt**  
A printf-style format string. Must not be NULL.

[in] **args**  
Additional arguments as required by the format string.

## Return Value

Same as `fs_file_write()`.

## See Also

[fs_file_write()](#fs_file_write)  
[fs_file_writef()](#fs_file_writef)  

---

# fs_file_seek

```c
fs_result fs_file_seek(
    [in] fs_file*       pFile,
    [in] fs_int64       offset,
    [in] fs_seek_origin origin
);
```

Moves the read/write cursor of a file.

You can seek relative to the start of the file, the current cursor position, or the end of the file.
A negative offset seeks backwards.

It is not an error to seek beyond the end of the file. If you seek beyond the end of the file and
then write, the exact behavior depends on the backend. On POSIX systems, it will most likely result
in a sparse file. In read mode, attempting to read beyond the end of the file will simply result
in zero bytes being read, and `FS_AT_END` being returned by `fs_file_read()`.

It is an error to try seeking to before the start of the file.

## Parameters

[in] **pFile**  
A pointer to the file to seek. Must not be NULL.

[in] **offset**  
The offset to seek to, relative to the position specified by `origin`. A negative value seeks
backwards.

[in] **origin**  
The origin from which to seek. One of the following values:

| Option | Description |
|:-------|:------------|
| `FS_SEEK_SET` | Seek from the start of the file. |
| `FS_SEEK_CUR` | Seek from the current cursor position. |
| `FS_SEEK_END` | Seek from the end of the file. |


## Return Value

Returns FS_SUCCESS on success; any other result code otherwise.

## See Also

[fs_file_tell()](#fs_file_tell)  
[fs_file_read()](#fs_file_read)  
[fs_file_write()](#fs_file_write)  

---

# fs_file_tell

```c
fs_result fs_file_tell(
    [in]  fs_file*  pFile,
    [out] fs_int64* pCursor
);
```

Retrieves the current position of the read/write cursor in a file.

## Parameters

[in] **pFile**  
A pointer to the file to query. Must not be NULL.

[out] **pCursor**  
A pointer to a variable that will receive the current cursor position. Must not be NULL.

## Return Value

Returns FS_SUCCESS on success; any other result code otherwise.

## See Also

[fs_file_seek()](#fs_file_seek)  
[fs_file_read()](#fs_file_read)  
[fs_file_write()](#fs_file_write)  

---

# fs_file_flush

```c
fs_result fs_file_flush(
    [in] fs_file* pFile
);
```

Flushes any buffered data to disk.

## Parameters

[in] **pFile**  
A pointer to the file to flush. Must not be NULL.

## Return Value

Returns FS_SUCCESS on success; any other result code otherwise.

---

# fs_file_truncate

```c
fs_result fs_file_truncate(
    [in] fs_file* pFile
);
```

Truncates a file to the current cursor position.

It is possible for a backend to not support truncation, in which case this function will return
`FS_NOT_IMPLEMENTED`.

## Parameters

[in] **pFile**  
A pointer to the file to truncate. Must not be NULL.

## Return Value

Returns FS_SUCCESS on success; any other result code otherwise. Will return `FS_NOT_IMPLEMENTED` if
the backend does not support truncation.

---

# fs_file_get_info

```c
fs_result fs_file_get_info(
    [in]  fs_file*      pFile,
    [out] fs_file_info* pInfo
);
```

Retrieves information about an opened file.

## Parameters

[in] **pFile**  
A pointer to the file to query. Must not be NULL.

[out] **pInfo**  
A pointer to a fs_file_info structure that will receive the file information. Must not be NULL.

## Return Value

Returns FS_SUCCESS on success; any other result code otherwise.

---

# fs_file_duplicate

```c
fs_result fs_file_duplicate(
    [in]  fs_file*  pFile,
    [out] fs_file** ppDuplicate
);
```

Duplicates a file handle.

This creates a new file handle that refers to the same underlying file as the original. The new
file handle will have its own independent cursor position. The initial position of the new file's
cursor will be undefined. You should call `fs_file_seek()` to set it to a known position before
using it.

Note that this does not duplicate the actual file on the file system itself. It just creates a
new `fs_file` object that refers to the same file.

## Parameters

[in] **pFile**  
A pointer to the file to duplicate. Must not be NULL.

[out] **ppDuplicate**  
A pointer to a pointer which will receive the duplicated file handle. Must not be NULL.

## Return Value

Returns FS_SUCCESS on success; any other result code otherwise.

---

# fs_file_get_backend_data

```c
void* fs_file_get_backend_data(
    [in] fs_file* pFile
);
```

Retrieves the backend-specific data associated with a file.

You should never call this function unless you are implementing a custom backend. The size of the
data can be retrieved with `fs_file_get_backend_data_size()`.

## Parameters

[in] **pFile**  
A pointer to the file to query. Must not be NULL.

## Return Value

Returns a pointer to the backend-specific data associated with the file, or NULL if there is no
such data.

## See Also

[fs_file_get_backend_data_size()](#fs_file_get_backend_data_size)  

---

# fs_file_get_backend_data_size

```c
size_t fs_file_get_backend_data_size(
    [in] fs_file* pFile
);
```

Retrieves the size of the backend-specific data associated with a file.

You should never call this function unless you are implementing a custom backend. The data can be
accessed with `fs_file_get_backend_data()`.

## Parameters

[in] **pFile**  
A pointer to the file to query. Must not be NULL.

## Return Value

Returns the size of the backend-specific data associated with the file, or 0 if there is no such
data.

## See Also

[fs_file_get_backend_data()](#fs_file_get_backend_data)  

---

# fs_file_get_stream

```c
fs_stream* fs_file_get_stream(
    [in] fs_file* pFile
);
```

Files are streams. This function returns a pointer to the `fs_stream` interface of the file.

## Parameters

[in] **pFile**  
A pointer to the file whose stream pointer is being retrieved. Must not be NULL.

## Return Value

Returns a pointer to the `fs_stream` interface of the file, or NULL if `pFile` is NULL.

## See Also

[fs_file_get_fs()](#fs_file_get_fs)  

---

# fs_file_get_fs

```c
fs* fs_file_get_fs(
    [in] fs_file* pFile
);
```

Retrieves the file system that owns a file.

## Parameters

[in] **pFile**  
A pointer to the file whose file system pointer is being retrieved. Must not be NULL.

## Return Value

Returns a pointer to the `fs` interface of the file's file system, or NULL if `pFile` is NULL.

## See Also

[fs_file_get_stream()](#fs_file_get_stream)  

---

# fs_first_ex

```c
fs_iterator* fs_first_ex(
    [in, optional] fs*         pFS,
    [in]           const char* pDirectoryPath,
    [in]           size_t      directoryPathLen,
    [in]           int         mode
);
```

The same as `fs_first()`, but with the length of the directory path specified explicitly.

## Parameters

[in, optional] **pFS**  
A pointer to the file system object. This can be NULL in which case the native file system will
be used.

[in] **pDirectoryPath**  
The path to the directory to iterate. Must not be NULL.

[in] **directoryPathLen**  
The length of the directory path. Can be set to `FS_NULL_TERMINATED` if the path is
null-terminated.

[in] **mode**  
Options for the iterator. See `fs_file_open()` for a description of the available flags.

## Return Value

Same as `fs_first()`.

---

# fs_first

```c
fs_iterator* fs_first(
    fs*         pFS,
    const char* pDirectoryPath,
    int         mode
);
```

Creates an iterator for the first entry in a directory.

This function creates an iterator that can be used to iterate over the entries in a directory. This
will be the first function called when iterating over the files inside a directory.

To get the next entry in the directory, call `fs_next()`. When `fs_next()` returns NULL, there are
no more entries in the directory. If you want to end iteration early, use `fs_free_iterator()` to
free the iterator.

See `fs_file_open()` for a description of the available flags that can be used in the `mode`
parameter. When `FS_WRITE` is specified, it will look at write mounts. Otherwise, it will look at
read mounts.


Parameter
---------
pFS : (in, optional)
    A pointer to the file system object. This can be NULL in which case the native file system will
    be used.

pDirectoryPath : (in)
    The path to the directory to iterate. Must not be NULL.

mode : (in)
    Options for the iterator. See `fs_file_open()` for a description of the available flags.

## Return Value

Returns a pointer to an iterator object on success; NULL on failure or if the directory is empty.

## See Also

[fs_next()](#fs_next)  
[fs_free_iterator()](#fs_free_iterator)  
[fs_first_ex()](#fs_first_ex)  

---

# fs_next

```c
fs_iterator* fs_next(
    [in] fs_iterator* pIterator
);
```

Gets the next entry in a directory iteration.

This function is used to get the next entry in a directory iteration. It should be called after
`fs_first()` or `fs_first_ex()` to retrieve the first entry, and then subsequently called to
retrieve each following entry.

If there are no more entries in the directory, this function will return NULL, and an explicit call
to `fs_free_iterator()` is not needed.

## Parameters

[in] **pIterator**  
A pointer to the iterator object. Must not be NULL.

## Return Value

Returns a pointer to the next iterator object on success; NULL if there are no more entries. If
NULL is returned, you need not call `fs_free_iterator()`. If you want to terminate iteration early,
you must call `fs_free_iterator()` to free the iterator.

You cannot assume that the returned pointer is the same as the input pointer. It may need to be
reallocated internally to hold the data of the next entry.

## See Also

[fs_first()](#fs_first)  
[fs_first_ex()](#fs_first_ex)  
[fs_free_iterator()](#fs_free_iterator)  

---

# fs_free_iterator

```c
void fs_free_iterator(
    [in, optional] fs_iterator* pIterator
);
```

Frees an iterator object.

This function frees an iterator object that was created by `fs_first()` or `fs_first_ex()`. You
need not call this if `fs_next()` returned NULL from an earlier iteration. However, if you want to
terminate iteration early, you must call this function to free the iterator.

It is safe to call this function with a NULL pointer, in which case it will do nothing.

## Parameters

[in, optional] **pIterator**  
A pointer to the iterator object. Can be NULL.

## See Also

[fs_first()](#fs_first)  
[fs_first_ex()](#fs_first_ex)  
[fs_next()](#fs_next)  

---

# fs_open_archive_ex

```c
fs_result fs_open_archive_ex(
    [in]           fs*               pFS,
    [in]           const fs_backend* pBackend,
    [in, optional] const void*       pBackendConfig,
    [in]           const char*       pArchivePath,
    [in]           size_t            archivePathLen,
    [in]           int               openMode,
    [out]          fs**              ppArchive
);
```

The same as `fs_open_archive()`, but with the ability to explicitly specify the backend to use.

## Parameters

[in] **pFS**  
A pointer to the file system object. Must not be NULL.

[in] **pBackend**  
A pointer to the backend to use for opening the archive. Must not be NULL.

[in, optional] **pBackendConfig**  
A pointer to backend-specific configuration data. Can be NULL if the backend does not require
any configuration.

[in] **pArchivePath**  
The path to the archive to open. Must not be NULL.

[in] **archivePathLen**  
The length of the archive path. Can be set to `FS_NULL_TERMINATED` if the path is null-terminated.

[in] **openMode**  
The mode to open the archive with.

[out] **ppArchive**  
A pointer to a pointer which will receive the opened archive file system object. Must not be
NULL.

## Return Value

Returns FS_SUCCESS on success; any other result code otherwise.

## See Also

[fs_open_archive()](#fs_open_archive)  
[fs_close_archive()](#fs_close_archive)  

---

# fs_open_archive

```c
fs_result fs_open_archive(
    [in]  fs*         pFS,
    [in]  const char* pArchivePath,
    [in]  int         openMode,
    [out] fs**        ppArchive
);
```

Helper function for initializing a file system object for an archive, such as a ZIP file.

To uninitialize the archive, you must use `fs_close_archive()`. Do not use `fs_uninit()` to
uninitialize an archive. The reason for this is that archives opened in this way are garbage
collected, and there are reference counting implications.

Note that opening the archive in write mode (`FS_WRITE`) does not automatically mean you will be
able to write to it. None of the stock backends support writing to archives at this time.

## Parameters

[in] **pFS**  
A pointer to the file system object. Must not be NULL.

[in] **pArchivePath**  
The path to the archive to open. Must not be NULL.

[in] **openMode**  
The open mode flags to open the archive with. See `fs_file_open()` for a description of the
available flags.

[out] **ppArchive**  
A pointer to a pointer which will receive the opened archive file system object. Must not be
NULL.

## Return Value

Returns FS_SUCCESS on success; any other result code otherwise.

## See Also

[fs_close_archive()](#fs_close_archive)  
[fs_open_archive_ex()](#fs_open_archive_ex)  

---

# fs_close_archive

```c
void fs_close_archive(
    [in] fs* pArchive
);
```

Closes an archive that was previously opened with `fs_open_archive()`.

You must use this function to close an archive opened with `fs_open_archive()`. Do not use
`fs_uninit()` to uninitialize an archive.

Note that when an archive is closed, it does not necessarily mean that the underlying file is
closed immediately. This is because archives are reference counted and garbage collected. You can
force garbage collection of unused archives with `fs_gc_archives()`.

## Parameters

[in] **pArchive**  
A pointer to the archive file system object to close. Must not be NULL.

## See Also

[fs_open_archive()](#fs_open_archive)  
[fs_open_archive_ex()](#fs_open_archive_ex)  

---

# fs_gc_archives

```c
void fs_gc_archives(
    [in] fs* pFS,
    [in] int policy
);
```

Garbage collects unused archives.

This function will close any opened archives that are no longer in use depending on the specified
policy.

You should rarely need to call this function directly. Archives will automatically be garbage collected
when the `fs` object is uninitialized with `fs_uninit()`.

## Parameters

[in] **pFS**  
A pointer to the file system object. Must not be NULL.

[in] **policy**  
The garbage collection policy to use. Set this to FS_GC_POLICY_THRESHOLD to only collect archives
if the number of opened archives exceeds the threshold set with `fs_set_archive_gc_threshold()`
which defaults to 10. Set this to FS_GC_POLICY_ALL to collect all unused archives regardless of the
threshold.

## See Also

[fs_open_archive()](#fs_open_archive)  
[fs_close_archive()](#fs_close_archive)  
[fs_set_archive_gc_threshold()](#fs_set_archive_gc_threshold)  

---

# fs_set_archive_gc_threshold

```c
void fs_set_archive_gc_threshold(
    [in] fs*    pFS,
    [in] size_t threshold
);
```

Sets the threshold for garbage collecting unused archives.

When an archive is no longer in use (its reference count drops to zero), it will not be closed
immediately. Instead, it will be kept open in case it is needed again soon. The threshold is what
determines how many unused archives will be kept open before they are garbage collected. The
default threshold is 10.

## Parameters

[in] **pFS**  
A pointer to the file system object. Must not be NULL.

[in] **threshold**  
The threshold for garbage collecting unused archives.

## See Also

[fs_gc_archives()](#fs_gc_archives)  

---

# fs_get_archive_gc_threshold

```c
size_t fs_get_archive_gc_threshold(
    [in] fs* pFS
);
```

Retrieves the threshold for garbage collecting unused archives.

## Parameters

[in] **pFS**  
A pointer to the file system object. Must not be NULL.

## Return Value

Returns the threshold for garbage collecting unused archives.

---

# fs_path_looks_like_archive

```c
fs_bool32 fs_path_looks_like_archive(
    [in] fs*         pFS,
    [in] const char* pPath,
    [in] size_t      pathLen
);
```

A helper function for checking if a path looks like it could be an archive.

This only checks the path string itself. It does not actually attempt to open and validate the
archive itself.

## Parameters

[in] **pFS**  
A pointer to the file system object. Must not be NULL.

[in] **pPath**  
The path to check. Must not be NULL.

[in] **pathLen**  
The length of the path string. Can be set to `FS_NULL_TERMINATED` if the path is null-terminated.

## Return Value

Returns FS_TRUE if the path looks like an archive, FS_FALSE otherwise.

---

# fs_mount

```c
fs_result fs_mount(
    [in]           fs*         pFS,
    [in]           const char* pActualPath,
    [in, optional] const char* pVirtualPath,
    [in]           int         options
);
```

Mounts a real directory or archive to a virtual path.

You must specify the actual path to the directory or archive on the file system referred to by
`pFS`. The virtual path can be NULL, in which case it will be treated as an empty string.

The virtual path is the path prefix that will be used when opening files. For example, if you mount
the actual path "somefolder" to the virtual path "assets", then when you open a file with the path
"assets/somefile.txt", it will actually open "somefolder/somefile.txt".

There are two groups of mounts - read-only and write. Read-only mounts are used when opening a file
in read-only mode (i.e. without the `FS_WRITE` flag). Write mounts are used when opening a file in
write mode (i.e. with the `FS_WRITE` flag). To control this, set the appropriate flag in the
`options` parameter.

The following flags are supported in the `options` parameter:


| Option | Description |
|:-------|:------------|
| `FS_READ` | This is a read-only mount. It will be used when opening files without the `FS_WRITE` flag. |
| `FS_WRITE` | This is a write mount. It will be used when opening files with the `FS_WRITE |
| `FS_LOWEST_PRIORITY` | By default, mounts are searched in the reverse order that they were added. This means that the most recently added mount has the highest priority. When this flag is specified, the mount will have the lowest priority instead. For a read-only mount, you can have multiple mounts with the same virtual path in which case they will be searched in order or priority when opening a file. For write mounts, you can have multiple mounts with the same virtual path, but when opening a file for writing, only the first matching mount will be used. You can have multiple write mounts where the virtual path is a sub-path of another write mount. For example, you could have one write mount with the virtual path "assets" and another with the virtual path "assets/images". When opening a file for writing, if the path starts with "assets/images", that mount will be used because it is a more specific match. Otherwise, if the path starts with "assets" but not "assets/images", the other mount will be used. You can specify both `FS_READ` and `FS_WRITE` in the `options` parameter to create one read-only mount, and one write mount in a single call. This is equivalent to calling `fs_mount()` twice - once with `FS_READ`, and again with `FS_WRITE`. Unmounting a directory or archive is done with `fs_unmount()`. You must specify the actual path when unmounting. |


## Parameters

[in] **pFS**  
A pointer to the file system object. Must not be NULL.

[in] **pActualPath**  
The actual path to the directory or archive to mount. Must not be NULL.

[in, optional] **pVirtualPath**  
The virtual path to mount the directory or archive to. Can be NULL in which case it will be
treated as an empty string.

[in] **options**  
Options for the mount. A combination of the flags described above.

## Return Value

Returns `FS_SUCCESS` on success; any other result code otherwise. If an identical mount already exists,
`FS_SUCCESS` will be returned.

## Example 1 - Basic Usage

```c
// Mount two directories to the same virtual path.
fs_mount(pFS, "some/actual/path", "assets", FS_READ);   // Lowest priority.
fs_mount(pFS, "some/other/path",  "assets", FS_READ);

// Mount a directory for writing.
fs_mount(pFS, "some/write/path",      "assets",        FS_WRITE);
fs_mount(pFS, "some/more/write/path", "assets/images", FS_WRITE); // More specific write mount.

// Mount a read-only mount, and a write mount in a single call.
fs_mount(pFS, "some/actual/path", "assets", FS_READ | FS_WRITE);
```

## Example 2 - Mounting an Archive

```c
// Mount a ZIP archive to the virtual path "assets".
fs_mount(pFS, "some/actual/path/archive.zip", "assets", FS_READ);
```

## See Also

[fs_unmount()](#fs_unmount)  
[fs_mount_sysdir()](#fs_mount_sysdir)  
[fs_mount_fs()](#fs_mount_fs)  

---

# fs_unmount

```c
fs_result fs_unmount(
    [in] fs*         pFS,
    [in] const char* pActualPath,
    [in] int         options
);
```

Unmounts a directory or archive that was previously mounted with `fs_mount()`.

You must specify the actual path to the directory or archive that was used when mounting. The
virtual path is not needed.

The only options that matter here are `FS_READ` and `FS_WRITE`. If you want to unmount a read-only
mount, you must specify `FS_READ`. If you want to unmount a write mount, you must specify
`FS_WRITE`. If you want to unmount both a read-only mount, and a write mount in a single call, you
can specify both flags. Using both flags is the same as calling `fs_unmount()` twice - once for the
read-only mount, and once for the write mount.

## Parameters

[in] **pFS**  
A pointer to the file system object. Must not be NULL.

[in] **pActualPath**  
The actual path to the directory or archive to unmount. Must not be NULL.

[in] **options**  
Either `FS_READ`, `FS_WRITE`, or both to unmount the corresponding mounts.

## Return Value

Returns `FS_SUCCESS` on success; any other result code otherwise. If no matching mount could be
found, `FS_SUCCESS` will be returned (it will just be a no-op).

---

# fs_mount_sysdir

```c
fs_result fs_mount_sysdir(
    [in]           fs*            pFS,
    [in]           fs_sysdir_type type,
    [in]           const char*    pSubDir,
    [in, optional] const char*    pVirtualPath,
    [in]           int            options
);
```

A helper function for mounting a standard system directory to a virtual path.

When calling this function you specify the type of system directory you want to mount. The actual
path of the system directory will often be generic, like "/home/yourname/" which is not useful for
a real program. For this reason, this function forces you to specify a sub-directory that will be
used with the system directory. This would often be something like the name of your application,
such as "myapp". It can also include sub-directories, such as "mycompany/myapp".

Otherwise, this function behaves exactly like `fs_mount()`.

Unmount the directory with `fs_unmount_sysdir()`. You must specify the same type and sub-directory
that was used when mounting.

## Parameters

[in] **pFS**  
A pointer to the file system object. Must not be NULL.

[in] **type**  
The type of system directory to mount.

[in] **pSubDir**  
The sub-directory to use with the system directory. Must not be NULL nor an empty string.

[in, optional] **pVirtualPath**  
The virtual path to mount the system directory to. Can be NULL in which case it will be treated
as an empty string.

[in] **options**  
Options for the mount. A combination of the flags described in `fs_mount()`.

## Return Value

Returns `FS_SUCCESS` on success; any other result code otherwise. If an identical mount already
exists, `FS_SUCCESS` will be returned.

## See Also

[fs_mount()](#fs_mount)  
[fs_unmount_sysdir()](#fs_unmount_sysdir)  

---

# fs_unmount_sysdir

```c
fs_result fs_unmount_sysdir(
    [in] fs*            pFS,
    [in] fs_sysdir_type type,
    [in] const char*    pSubDir,
    [in] int            options
);
```

Unmounts a system directory that was previously mounted with `fs_mount_sysdir()`.

This is the same as `fs_unmount()`, but follows the "type" and sub-directory semantics of
`fs_mount_sysdir()`.

## Parameters

[in] **pFS**  
A pointer to the file system object. Must not be NULL.

[in] **type**  
The type of system directory to unmount.

[in] **pSubDir**  
The sub-directory that was used with the system directory when mounting. Must not be NULL nor
an empty string.

[in] **options**  
Either `FS_READ`, `FS_WRITE`, or both to unmount the corresponding mounts.

## Return Value

Returns `FS_SUCCESS` on success; any other result code otherwise. If no matching mount could be
found, `FS_SUCCESS` will be returned (it will just be a no-op).

---

# fs_mount_fs

```c
fs_result fs_mount_fs(
    [in]           fs*         pFS,
    [in]           fs*         pOtherFS,
    [in, optional] const char* pVirtualPath,
    [in]           int         options
);
```

Mounts another `fs` object to a virtual path.

This is the same as `fs_mount()`, but instead of specifying an actual path to a directory or
archive, you specify another `fs` object.

Use `fs_unmount_fs()` to unmount the file system.

## Parameters

[in] **pFS**  
A pointer to the file system object. Must not be NULL.

[in] **pOtherFS**  
A pointer to the other file system object to mount. Must not be NULL.

[in, optional] **pVirtualPath**  
The virtual path to mount the other file system to. Can be NULL in which case it will be treated
as an empty string.

[in] **options**  
Options for the mount. A combination of the flags described in `fs_mount()`.

## Return Value

Returns `FS_SUCCESS` on success; any other result code otherwise. If an identical mount already
exists, `FS_SUCCESS` will be returned.

## See Also

[fs_mount()](#fs_mount)  
[fs_unmount_fs()](#fs_unmount_fs)  

---

# fs_unmount_fs

```c
fs_result fs_unmount_fs(
    [in] fs* pFS,
    [in] fs* pOtherFS,
    [in] int options
);
```

Unmounts a file system that was previously mounted with `fs_mount_fs()`.

## Parameters

[in] **pFS**  
A pointer to the file system object. Must not be NULL.

[in] **pOtherFS**  
A pointer to the other file system object to unmount. Must not be NULL.

[in] **options**  
Options for the unmount. A combination of the flags described in `fs_unmount()`.

## Return Value

Returns `FS_SUCCESS` on success; any other result code otherwise. If no matching mount could be
found, `FS_SUCCESS` will be returned (it will just be a no-op).

## See Also

[fs_unmount()](#fs_unmount)  
[fs_mount_fs()](#fs_mount_fs)  

---

# fs_file_read_to_end

```c
fs_result fs_file_read_to_end(
    fs_file*  pFile,
    fs_format format,
    void**    ppData,
    size_t*   pDataSize
);
```

Helper functions for reading the entire contents of a file, starting from the current cursor position. Free
the returned pointer with fs_free(), using the same allocation callbacks as the fs object. You can use
fs_get_allocation_callbacks() if necessary, like so:

    fs_free(pFileData, fs_get_allocation_callbacks(pFS));

The format (FS_FORMAT_TEXT or FS_FORMAT_BINARY) is used to determine whether or not a null terminator should be
appended to the end of the data.

For flexiblity in case the backend does not support cursor retrieval or positioning, the data will be read
in fixed sized chunks.

---

# fs_file_open_and_read

```c
fs_result fs_file_open_and_read(
    fs*         pFS,
    const char* pFilePath,
    fs_format   format,
    void**      ppData,
    size_t*     pDataSize
);
```

---

# fs_file_open_and_write

```c
fs_result fs_file_open_and_write(
    fs*         pFS,
    const char* pFilePath,
    const void* pData,
    size_t      dataSize
);
```

---

# fs_result_from_errno

```c
fs_result fs_result_from_errno(
    int error
);
```

---

# fs_path_first

```c
fs_result fs_path_first(
    const char*       pPath,
    size_t            pathLen,
    fs_path_iterator* pIterator
);
```

---

# fs_path_last

```c
fs_result fs_path_last(
    const char*       pPath,
    size_t            pathLen,
    fs_path_iterator* pIterator
);
```

---

# fs_path_next

```c
fs_result fs_path_next(
    fs_path_iterator* pIterator
);
```

---

# fs_path_prev

```c
fs_result fs_path_prev(
    fs_path_iterator* pIterator
);
```

---

# fs_path_is_first

```c
fs_bool32 fs_path_is_first(
    const fs_path_iterator* pIterator
);
```

---

# fs_path_is_last

```c
fs_bool32 fs_path_is_last(
    const fs_path_iterator* pIterator
);
```

---

# fs_path_iterators_compare

```c
int fs_path_iterators_compare(
    const fs_path_iterator* pIteratorA,
    const fs_path_iterator* pIteratorB
);
```

---

# fs_path_compare

```c
int fs_path_compare(
    const char* pPathA,
    size_t      pathALen,
    const char* pPathB,
    size_t      pathBLen
);
```

---

# fs_path_file_name

```c
const char* fs_path_file_name(
    const char* pPath,
    size_t      pathLen
);
```

---

# fs_path_directory

```c
int fs_path_directory(
    char*       pDst,
    size_t      dstCap,
    const char* pPath,
    size_t      pathLen
);
```

Does *not* include the null terminator. Returns an offset of pPath. Will only be null terminated if pPath is. Returns null if the path ends with a slash.

---

# fs_path_extension

```c
const char* fs_path_extension(
    const char* pPath,
    size_t      pathLen
);
```

Returns the length, or < 0 on error. pDst can be null in which case the required length will be returned. Will not include a trailing slash.

---

# fs_path_extension_equal

```c
fs_bool32 fs_path_extension_equal(
    const char* pPath,
    size_t      pathLen,
    const char* pExtension,
    size_t      extensionLen
);
```

Does *not* include the null terminator. Returns an offset of pPath. Will only be null terminated if pPath is. Returns null if the extension cannot be found.

---

# fs_path_trim_base

```c
const char* fs_path_trim_base(
    const char* pPath,
    size_t      pathLen,
    const char* pBasePath,
    size_t      basePathLen
);
```

Returns true if the extension is equal to the given extension. Case insensitive.

---

# fs_path_begins_with

```c
fs_bool32 fs_path_begins_with(
    const char* pPath,
    size_t      pathLen,
    const char* pBasePath,
    size_t      basePathLen
);
```

---

# fs_path_append

```c
int fs_path_append(
    char*       pDst,
    size_t      dstCap,
    const char* pBasePath,
    size_t      basePathLen,
    const char* pPathToAppend,
    size_t      pathToAppendLen
);
```

---

# fs_path_normalize

```c
int fs_path_normalize(
    char*        pDst,
    size_t       dstCap,
    const char*  pPath,
    size_t       pathLen,
    unsigned int options
);
```

pDst can be equal to pBasePath in which case it will be appended in-place. pDst can be null in which case the function will return the required length.

---

# fs_memory_stream_init_write

```c
fs_result fs_memory_stream_init_write(
    const fs_allocation_callbacks* pAllocationCallbacks,
    fs_memory_stream*              pStream
);
```

---

# fs_memory_stream_init_readonly

```c
fs_result fs_memory_stream_init_readonly(
    const void*       pData,
    size_t            dataSize,
    fs_memory_stream* pStream
);
```

---

# fs_memory_stream_uninit

```c
void fs_memory_stream_uninit(
    fs_memory_stream* pStream
);
```

---

# fs_memory_stream_read

```c
fs_result fs_memory_stream_read(
    fs_memory_stream* pStream,
    void*             pDst,
    size_t            bytesToRead,
    size_t*           pBytesRead
);
```

Only needed for write mode. This will free the internal pointer so make sure you've done what you need to do with it.

---

# fs_memory_stream_write

```c
fs_result fs_memory_stream_write(
    fs_memory_stream* pStream,
    const void*       pSrc,
    size_t            bytesToWrite,
    size_t*           pBytesWritten
);
```

---

# fs_memory_stream_seek

```c
fs_result fs_memory_stream_seek(
    fs_memory_stream* pStream,
    fs_int64          offset,
    int               origin
);
```

---

# fs_memory_stream_tell

```c
fs_result fs_memory_stream_tell(
    fs_memory_stream* pStream,
    size_t*           pCursor
);
```

---

# fs_memory_stream_remove

```c
fs_result fs_memory_stream_remove(
    fs_memory_stream* pStream,
    size_t            offset,
    size_t            size
);
```

---

# fs_memory_stream_truncate

```c
fs_result fs_memory_stream_truncate(
    fs_memory_stream* pStream
);
```

---

# fs_memory_stream_take_ownership

```c
void* fs_memory_stream_take_ownership(
    fs_memory_stream* pStream,
    size_t*           pSize
);
```

---

# fs_sort

```c
void fs_sort(
    void*                                               pBase,
    size_t                                              count,
    size_t                                              stride,
    int (*compareProc)(void*, const void*, const void*) compareProc,
    void*                                               pUserData
);
```

---

# fs_binary_search

```c
void* fs_binary_search(
    const void*                                         pKey,
    const void*                                         pList,
    size_t                                              count,
    size_t                                              stride,
    int (*compareProc)(void*, const void*, const void*) compareProc,
    void*                                               pUserData
);
```

---

# fs_linear_search

```c
void* fs_linear_search(
    const void*                                         pKey,
    const void*                                         pList,
    size_t                                              count,
    size_t                                              stride,
    int (*compareProc)(void*, const void*, const void*) compareProc,
    void*                                               pUserData
);
```

---

# fs_sorted_search

```c
void* fs_sorted_search(
    const void*                                         pKey,
    const void*                                         pList,
    size_t                                              count,
    size_t                                              stride,
    int (*compareProc)(void*, const void*, const void*) compareProc,
    void*                                               pUserData
);
```

---

# fs_strncmp

```c
int fs_strncmp(
    const char* str1,
    const char* str2,
    size_t      maxLen
);
```

---

# fs_strnicmp

```c
int fs_strnicmp(
    const char* str1,
    const char* str2,
    size_t      count
);
```

---

# fs_vsprintf

```c
int fs_vsprintf(
    char*       buf,
    char const* fmt,
    va_list     va
);
```

---

# fs_vsnprintf

```c
int fs_vsnprintf(
    char*       buf,
    size_t      count,
    char const* fmt,
    va_list     va
);
```

---

# fs_sprintf

```c
int fs_sprintf(
    char*       buf,
    char const* fmt,
    ...         
);
```

---

# fs_snprintf

```c
int fs_snprintf(
    char*       buf,
    size_t      count,
    char const* fmt,
    ...         
);
```

---

# Enums

## fs_result

| Name | Value |
|------|-------|
| `FS_SUCCESS` | `0` |
| `FS_ERROR` | `-1` |
| `FS_INVALID_ARGS` | `-2` |
| `FS_INVALID_OPERATION` | `-3` |
| `FS_OUT_OF_MEMORY` | `-4` |
| `FS_OUT_OF_RANGE` | `-5` |
| `FS_ACCESS_DENIED` | `-6` |
| `FS_DOES_NOT_EXIST` | `-7` |
| `FS_ALREADY_EXISTS` | `-8` |
| `FS_TOO_MANY_OPEN_FILES` | `-9` |
| `FS_INVALID_FILE` | `-10` |
| `FS_TOO_BIG` | `-11` |
| `FS_PATH_TOO_LONG` | `-12` |
| `FS_NAME_TOO_LONG` | `-13` |
| `FS_NOT_DIRECTORY` | `-14` |
| `FS_IS_DIRECTORY` | `-15` |
| `FS_DIRECTORY_NOT_EMPTY` | `-16` |
| `FS_AT_END` | `-17` |
| `FS_NO_SPACE` | `-18` |
| `FS_BUSY` | `-19` |
| `FS_IO_ERROR` | `-20` |
| `FS_INTERRUPT` | `-21` |
| `FS_UNAVAILABLE` | `-22` |
| `FS_ALREADY_IN_USE` | `-23` |
| `FS_BAD_ADDRESS` | `-24` |
| `FS_BAD_SEEK` | `-25` |
| `FS_BAD_PIPE` | `-26` |
| `FS_DEADLOCK` | `-27` |
| `FS_TOO_MANY_LINKS` | `-28` |
| `FS_NOT_IMPLEMENTED` | `-29` |
| `FS_NO_MESSAGE` | `-30` |
| `FS_BAD_MESSAGE` | `-31` |
| `FS_NO_DATA_AVAILABLE` | `-32` |
| `FS_INVALID_DATA` | `-33` |
| `FS_TIMEOUT` | `-34` |
| `FS_NO_NETWORK` | `-35` |
| `FS_NOT_UNIQUE` | `-36` |
| `FS_NOT_SOCKET` | `-37` |
| `FS_NO_ADDRESS` | `-38` |
| `FS_BAD_PROTOCOL` | `-39` |
| `FS_PROTOCOL_UNAVAILABLE` | `-40` |
| `FS_PROTOCOL_NOT_SUPPORTED` | `-41` |
| `FS_PROTOCOL_FAMILY_NOT_SUPPORTED` | `-42` |
| `FS_ADDRESS_FAMILY_NOT_SUPPORTED` | `-43` |
| `FS_SOCKET_NOT_SUPPORTED` | `-44` |
| `FS_CONNECTION_RESET` | `-45` |
| `FS_ALREADY_CONNECTED` | `-46` |
| `FS_NOT_CONNECTED` | `-47` |
| `FS_CONNECTION_REFUSED` | `-48` |
| `FS_NO_HOST` | `-49` |
| `FS_IN_PROGRESS` | `-50` |
| `FS_CANCELLED` | `-51` |
| `FS_MEMORY_ALREADY_MAPPED` | `-52` |
| `FS_DIFFERENT_DEVICE` | `-53` |
| `FS_CHECKSUM_MISMATCH` | `-100` |
| `FS_NO_BACKEND` | `-101` |
| `FS_NEEDS_MORE_INPUT` | `100` |
| `FS_HAS_MORE_OUTPUT` | `102` |

---

## fs_seek_origin

| Name | Value |
|------|-------|
| `FS_SEEK_SET` | `0` |
| `FS_SEEK_CUR` | `1` |
| `FS_SEEK_END` | `2` |

---

## fs_format

| Name |
|------|
| `FS_FORMAT_TEXT` |
| `FS_FORMAT_BINARY` |

---

## fs_sysdir_type

| Name |
|------|
| `FS_SYSDIR_HOME` |
| `FS_SYSDIR_TEMP` |
| `FS_SYSDIR_CONFIG` |
| `FS_SYSDIR_DATA` |
| `FS_SYSDIR_CACHE` |

---

