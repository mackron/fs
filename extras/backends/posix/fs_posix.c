#ifndef fs_posix_c
#define fs_posix_c

#include "fs_posix.h"

/* If it's not Windows, assume POSIX support. This can be adjusted later as other platforms come up. */
#if !defined(_WIN32)

/* TODO: Remove this when this file is amalgamated into the main file. */
#ifndef FS_MAX
#define FS_MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif
FS_API char* fs_strcpy(char* dst, const char* src);
FS_API int fs_strncpy_s(char* dst, size_t dstCap, const char* src, size_t count);


/* TODO: Move this into the main file. */
#define FS_STDIN  ":stdi:"
#define FS_STDOUT ":stdo:"
#define FS_STDERR ":stde:"


/* For 64-bit seeks. */
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include <string.h> /* memset() */
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

typedef struct fs_posix
{
    int _unused;
} fs_posix;

typedef struct fs_file_posix
{
    int fd;
    fs_bool32 isOpenedFromHandle;
} fs_file_posix;

static size_t fs_alloc_size_posix(const void* pBackendConfig)
{
    (void)pBackendConfig;
    return sizeof(fs_posix);
}

static fs_result fs_init_posix(fs* pFS, const void* pBackendConfig, fs_stream* pStream)
{
    (void)pFS;
    (void)pBackendConfig;
    (void)pStream;

    return FS_SUCCESS;
}

static void fs_uninit_posix(fs* pFS)
{
    (void)pFS;
}

static fs_result fs_ioctl_posix(fs* pFS, int op, void* pArg)
{
    (void)pFS;
    (void)op;
    (void)pArg;

    return FS_NOT_IMPLEMENTED;
}

static fs_result fs_remove_posix(fs* pFS, const char* pFilePath)
{
    int result = remove(pFilePath);
    if (result < 0) {
        return fs_result_from_errno(errno);
    }

    (void)pFS;
    return FS_SUCCESS;
}

static fs_result fs_rename_posix(fs* pFS, const char* pOldName, const char* pNewName)
{
    int result = rename(pOldName, pNewName);
    if (result < 0) {
        return fs_result_from_errno(errno);
    }

    (void)pFS;
    return FS_SUCCESS;
}

static fs_result fs_mkdir_posix(fs* pFS, const char* pPath)
{
    int result = mkdir(pPath, S_IRWXU);
    if (result < 0) {
        return fs_result_from_errno(errno);
    }

    (void)pFS;
    return FS_SUCCESS;
}


static fs_file_info fs_file_info_from_stat_posix(struct stat* pStat)
{
    fs_file_info info;

    memset(&info, 0, sizeof(info));
    info.size             = pStat->st_size;
    info.lastAccessTime   = pStat->st_atime;
    info.lastModifiedTime = pStat->st_mtime;
    info.directory        = S_ISDIR(pStat->st_mode) != 0;
    info.symlink          = S_ISLNK(pStat->st_mode) != 0;

    return info;
}

static fs_result fs_info_posix(fs* pFS, const char* pPath, int openMode, fs_file_info* pInfo)
{
    struct stat info;

    if (stat(pPath, &info) != 0) {
        return fs_result_from_errno(errno);
    }

    *pInfo = fs_file_info_from_stat_posix(&info);

    (void)pFS;
    (void)openMode;
    return FS_SUCCESS;
}

static size_t fs_file_alloc_size_posix(fs* pFS)
{
    (void)pFS;
    return sizeof(fs_file_posix);
}

static fs_result fs_file_open_posix(fs* pFS, fs_stream* pStream, const char* pFilePath, int openMode, fs_file* pFile)
{
    fs_file_posix* pFilePosix = (fs_file_posix*)fs_file_get_backend_data(pFile);
    int fd;
    int flags = 0;

    if ((openMode & FS_READ) != 0) {
        if ((openMode & FS_WRITE) != 0) {
            flags |= O_RDWR | O_CREAT;
        } else {
            flags |= O_RDONLY;
        }
    } else if ((openMode & FS_WRITE) != 0) {
        flags |= O_WRONLY | O_CREAT;
    }

    if ((openMode & FS_APPEND) != 0) {
        flags |= O_APPEND;
    } else {
        flags |= O_TRUNC;
    }

    /* TODO: Add support for the O_EXCL flag. */

    /* For ancient versions of Linux. */
    #if defined(O_LARGEFILE)
    flags |= O_LARGEFILE;
    #endif

    /*  */ if (strcmp(pFilePath, FS_STDIN ) == 0) {
        fd = STDIN_FILENO;
    } else if (strcmp(pFilePath, FS_STDOUT) == 0) {
        fd = STDOUT_FILENO;
    } else if (strcmp(pFilePath, FS_STDERR) == 0) {
        fd = STDERR_FILENO;
    } else {
        fd = open(pFilePath, flags);
    }

    if (fd < 0) {
        return fs_result_from_errno(errno);
    }

    pFilePosix->fd = fd;

    (void)pFS;
    (void)pStream;
    return FS_SUCCESS;
}

static void fs_file_close_posix(fs_file* pFile)
{
    fs_file_posix* pFilePosix = (fs_file_posix*)fs_file_get_backend_data(pFile);

    /* No need to do anything if the file was opened from stdin, stdout, or stderr. */
    if (pFilePosix->fd == STDIN_FILENO || pFilePosix->fd == STDOUT_FILENO || pFilePosix->fd == STDERR_FILENO) {
        return;
    }

    close(pFilePosix->fd);
}

static fs_result fs_file_read_posix(fs_file* pFile, void* pDst, size_t bytesToRead, size_t* pBytesRead)
{
    fs_file_posix* pFilePosix = (fs_file_posix*)fs_file_get_backend_data(pFile);
    ssize_t bytesRead;

    bytesRead = read(pFilePosix->fd, pDst, bytesToRead);
    if (bytesRead < 0) {
        return fs_result_from_errno(errno);
    }

    *pBytesRead = (size_t)bytesRead;

    if (*pBytesRead == 0) {
        return FS_AT_END;
    }

    return FS_SUCCESS;
}

static fs_result fs_file_write_posix(fs_file* pFile, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten)
{
    fs_file_posix* pFilePosix = (fs_file_posix*)fs_file_get_backend_data(pFile);
    ssize_t bytesWritten;

    bytesWritten = write(pFilePosix->fd, pSrc, bytesToWrite);
    if (bytesWritten < 0) {
        return fs_result_from_errno(errno);
    }

    *pBytesWritten = (size_t)bytesWritten;
    return FS_SUCCESS;
}

static fs_result fs_file_seek_posix(fs_file* pFile, fs_int64 offset, fs_seek_origin origin)
{
    fs_file_posix* pFilePosix = (fs_file_posix*)fs_file_get_backend_data(pFile);
    int whence;
    off_t result;

    if (origin == FS_SEEK_SET) {
        whence = SEEK_SET;
    } else if (origin == FS_SEEK_END) {
        whence = SEEK_END;
    } else {
        whence = SEEK_CUR;
    }

    #if defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64
    {
        result = lseek(pFilePosix->fd, (off_t)offset, whence);
    }
    #else
    {
        if (offset < -2147483648 || offset > 2147483647) {
            return FS_BAD_SEEK;    /* Offset is too large. */
        }

        result = lseek(pFilePosix->fd, (off_t)(int)offset, whence);
    }
    #endif

    if (result < 0) {
        return fs_result_from_errno(errno);
    }

    return FS_SUCCESS;
}

static fs_result fs_file_tell_posix(fs_file* pFile, fs_int64* pCursor)
{
    fs_file_posix* pFilePosix = (fs_file_posix*)fs_file_get_backend_data(pFile);
    fs_int64 cursor;

    cursor = (fs_int64)lseek(pFilePosix->fd, 0, SEEK_CUR);
    if (cursor < 0) {
        return fs_result_from_errno(errno);
    }

    *pCursor = cursor;
    return FS_SUCCESS;
}

static fs_result fs_file_flush_posix(fs_file* pFile)
{
    fs_file_posix* pFilePosix = (fs_file_posix*)fs_file_get_backend_data(pFile);
    int result;

    result = fsync(pFilePosix->fd);
    if (result < 0) {
        return fs_result_from_errno(errno);
    }

    return FS_SUCCESS;
}

static fs_result fs_file_info_posix(fs_file* pFile, fs_file_info* pInfo)
{
    fs_file_posix* pFilePosix = (fs_file_posix*)fs_file_get_backend_data(pFile);
    struct stat info;

    if (fstat(pFilePosix->fd, &info) < 0) {
        return fs_result_from_errno(errno);
    }

    *pInfo = fs_file_info_from_stat_posix(&info);

    return FS_SUCCESS;
}

static fs_result fs_file_duplicate_posix(fs_file* pFile, fs_file* pDuplicate)
{
    fs_file_posix* pFilePosix      = (fs_file_posix*)fs_file_get_backend_data(pFile);
    fs_file_posix* pDuplicatePosix = (fs_file_posix*)fs_file_get_backend_data(pDuplicate);

    pDuplicatePosix->fd = dup(pFilePosix->fd);
    if (pDuplicatePosix->fd < 0) {
        return fs_result_from_errno(errno);
    }

    return FS_SUCCESS;
}


#define FS_POSIX_MIN_ITERATOR_ALLOCATION_SIZE 1024

typedef struct fs_iterator_posix
{
    fs_iterator iterator;
    DIR* pDir;
    char* pFullFilePath;        /* Points to the end of the structure. */
    size_t directoryPathLen;    /* The length of the directory section. */
} fs_iterator_posix;

static void fs_free_iterator_posix(fs_iterator* pIterator);

static fs_iterator* fs_first_posix(fs* pFS, const char* pDirectoryPath, size_t directoryPathLen)
{
    fs_iterator_posix* pIteratorPosix;
    struct dirent* info;
    struct stat statInfo;
    size_t fileNameLen;

    /*
    Our input string isn't necessarily null terminated so we'll need to make a copy. This isn't
    the end of the world because we need to keep a copy of it anyway for when we need to stat
    the file for information like it's size.

    To do this we're going to allocate memory for our iterator which will include space for the
    directory path. Then we copy the directory path into the allocated memory and point the
    pFullFilePath member of the iterator to it. Then we call opendir(). Once that's done we
    can go to the first file and reallocate the iterator to make room for the file name portion,
    including the separating slash. Then we copy the file name portion over to the buffer.
    */

    if (directoryPathLen == 0 || pDirectoryPath[0] == '\0') {
        directoryPathLen = 1;
        pDirectoryPath = ".";
    }

    /* The first step is to calculate the length of the path if we need to. */
    if (directoryPathLen == (size_t)-1) {
        directoryPathLen = strlen(pDirectoryPath);
    }


    /*
    Now that we know the length of the directory we can allocate space for the iterator. The
    directory path will be placed at the end of the structure.
    */
    pIteratorPosix = (fs_iterator_posix*)fs_malloc(FS_MAX(sizeof(*pIteratorPosix) + directoryPathLen + 1, FS_POSIX_MIN_ITERATOR_ALLOCATION_SIZE), fs_get_allocation_callbacks(pFS));    /* +1 for null terminator. */
    if (pIteratorPosix == NULL) {
        return NULL;
    }

    /* Point pFullFilePath to the end of structure to where the path is located. */
    pIteratorPosix->pFullFilePath = (char*)pIteratorPosix + sizeof(*pIteratorPosix);
    pIteratorPosix->directoryPathLen = directoryPathLen;

    /* We can now copy over the directory path. This will null terminate the path which will allow us to call opendir(). */
    fs_strncpy_s(pIteratorPosix->pFullFilePath, directoryPathLen + 1, pDirectoryPath, directoryPathLen);

    /* We can now open the directory. */
    pIteratorPosix->pDir = opendir(pIteratorPosix->pFullFilePath);
    if (pIteratorPosix->pDir == NULL) {
        fs_free(pIteratorPosix, fs_get_allocation_callbacks(pFS));
        return NULL;
    }

    /* We now need to get information about the first file. */
    info = readdir(pIteratorPosix->pDir);
    if (info == NULL) {
        closedir(pIteratorPosix->pDir);
        fs_free(pIteratorPosix, fs_get_allocation_callbacks(pFS));
        return NULL;
    }

    fileNameLen = strlen(info->d_name);

    /*
    Now that we have the file name we need to append it to the full file path in the iterator. To do
    this we need to reallocate the iterator to account for the length of the file name, including the
    separating slash.
    */
    {
        fs_iterator_posix* pNewIteratorPosix= (fs_iterator_posix*)fs_realloc(pIteratorPosix, FS_MAX(sizeof(*pIteratorPosix) + directoryPathLen + 1 + fileNameLen + 1, FS_POSIX_MIN_ITERATOR_ALLOCATION_SIZE), fs_get_allocation_callbacks(pFS));    /* +1 for null terminator. */
        if (pNewIteratorPosix == NULL) {
            closedir(pIteratorPosix->pDir);
            fs_free(pIteratorPosix, fs_get_allocation_callbacks(pFS));
            return NULL;
        }

        pIteratorPosix = pNewIteratorPosix;
    }

    /* Memory has been allocated. Copy over the separating slash and file name. */
    pIteratorPosix->pFullFilePath = (char*)pIteratorPosix + sizeof(*pIteratorPosix);
    pIteratorPosix->pFullFilePath[directoryPathLen] = '/';
    fs_strcpy(pIteratorPosix->pFullFilePath + directoryPathLen + 1, info->d_name);

    /* The pFileName member of the base iterator needs to be set to the file name. */
    pIteratorPosix->iterator.pName   = pIteratorPosix->pFullFilePath + directoryPathLen + 1;
    pIteratorPosix->iterator.nameLen = fileNameLen;

    /* We can now get the file information. */
    if (stat(pIteratorPosix->pFullFilePath, &statInfo) != 0) {
        closedir(pIteratorPosix->pDir);
        fs_free(pIteratorPosix, fs_get_allocation_callbacks(pFS));
        return NULL;
    }

    pIteratorPosix->iterator.info = fs_file_info_from_stat_posix(&statInfo);

    return (fs_iterator*)pIteratorPosix;
}

static fs_iterator* fs_next_posix(fs_iterator* pIterator)
{
    fs_iterator_posix* pIteratorPosix = (fs_iterator_posix*)pIterator;
    struct dirent* info;
    struct stat statInfo;
    size_t fileNameLen;

    /* We need to get information about the next file. */
    info = readdir(pIteratorPosix->pDir);
    if (info == NULL) {
        fs_free_iterator_posix((fs_iterator*)pIteratorPosix);
        return NULL;    /* The end of the directory. */
    }

    fileNameLen = strlen(info->d_name);

    /* We need to reallocate the iterator to account for the new file name. */
    {
        fs_iterator_posix* pNewIteratorPosix = (fs_iterator_posix*)fs_realloc(pIteratorPosix, FS_MAX(sizeof(*pIteratorPosix) + pIteratorPosix->directoryPathLen + 1 + fileNameLen + 1, FS_POSIX_MIN_ITERATOR_ALLOCATION_SIZE), fs_get_allocation_callbacks(pIterator->pFS));    /* +1 for null terminator. */
        if (pNewIteratorPosix == NULL) {
            fs_free_iterator_posix((fs_iterator*)pIteratorPosix);
            return NULL;
        }

        pIteratorPosix = pNewIteratorPosix;
    }

    /* Memory has been allocated. Copy over the file name. */
    pIteratorPosix->pFullFilePath = (char*)pIteratorPosix + sizeof(*pIteratorPosix);
    fs_strcpy(pIteratorPosix->pFullFilePath + pIteratorPosix->directoryPathLen + 1, info->d_name);

    /* The pFileName member of the base iterator needs to be set to the file name. */
    pIteratorPosix->iterator.pName   = pIteratorPosix->pFullFilePath + pIteratorPosix->directoryPathLen + 1;
    pIteratorPosix->iterator.nameLen = fileNameLen;

    /* We can now get the file information. */
    if (stat(pIteratorPosix->pFullFilePath, &statInfo) != 0) {
        fs_free_iterator_posix((fs_iterator*)pIteratorPosix);
        return NULL;
    }

    pIteratorPosix->iterator.info = fs_file_info_from_stat_posix(&statInfo);

    return (fs_iterator*)pIteratorPosix;
}

static void fs_free_iterator_posix(fs_iterator* pIterator)
{
    fs_iterator_posix* pIteratorPosix = (fs_iterator_posix*)pIterator;

    closedir(pIteratorPosix->pDir);
    fs_free(pIteratorPosix, fs_get_allocation_callbacks(pIterator->pFS));
}

static fs_backend fs_posix_backend =
{
    fs_alloc_size_posix,
    fs_init_posix,
    fs_uninit_posix,
    fs_ioctl_posix,
    fs_remove_posix,
    fs_rename_posix,
    fs_mkdir_posix,
    fs_info_posix,
    fs_file_alloc_size_posix,
    fs_file_open_posix,
    NULL,
    fs_file_close_posix,
    fs_file_read_posix,
    fs_file_write_posix,
    fs_file_seek_posix,
    fs_file_tell_posix,
    fs_file_flush_posix,
    fs_file_info_posix,
    fs_file_duplicate_posix,
    fs_first_posix,
    fs_next_posix,
    fs_free_iterator_posix
};

const fs_backend* FS_POSIX = &fs_posix_backend;
#else
const fs_backend* FS_POSIX = NULL;
#endif

#endif  /* fs_posix_c */
