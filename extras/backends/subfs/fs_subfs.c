#ifndef fs_subfs_c
#define fs_subfs_c

#include "../../../fs.h"
#include "fs_subfs.h"

#include <assert.h>
#include <string.h>

#define FS_SUBFS_UNUSED(x) (void)x

static void fs_subfs_zero_memory_default(void* p, size_t sz)
{
    if (sz > 0) {
        memset(p, 0, sz);
    }
}

#ifndef FS_SUBFS_ZERO_MEMORY
#define FS_SUBFS_ZERO_MEMORY(p, sz) fs_subfs_zero_memory_default((p), (sz))
#endif
#define FS_SUBFS_ZERO_OBJECT(p)     FS_SUBFS_ZERO_MEMORY((p), sizeof(*(p)))

#ifndef FS_SUBFS_ASSERT
#define FS_SUBFS_ASSERT(x) assert(x)
#endif


/* Bit of a naughty hack. This is defined with FS_API in fs.c, but is not forward declared in fs.h. We can just declare it here. */
FS_API char* fs_strcpy(char* dst, const char* src);


/* BEG fs_subfs.c */
typedef struct fs_subfs
{
    fs* pOwnerFS;
    char* pRootDir;   /* Points to the end of the structure. */
    size_t rootDirLen;
} fs_subfs;

typedef struct fs_file_subfs
{
    fs_file* pActualFile;
} fs_file_subfs;


typedef struct fs_subfs_path
{
    char  pFullPathStack[1024];
    char* pFullPathHeap;
    char* pFullPath;
    int fullPathLen;
} fs_subfs_path;

static fs_result fs_subfs_path_init(fs* pFS, const char* pPath, size_t pathLen, fs_subfs_path* pSubFSPath)
{
    fs_subfs* pSubFS;
    char  pPathCleanStack[1024];
    char* pPathCleanHeap = NULL;
    char* pPathClean;
    size_t pathCleanLen;

    FS_SUBFS_ASSERT(pFS        != NULL);
    FS_SUBFS_ASSERT(pPath      != NULL);
    FS_SUBFS_ASSERT(pSubFSPath != NULL);

    FS_SUBFS_ZERO_OBJECT(pSubFSPath);   /* Safety. */

    /* We first have to clean the path, with a strict requirement that we fail if attempting to navigate above the root. */
    pathCleanLen = fs_path_normalize(pPathCleanStack, sizeof(pPathCleanStack), pPath, pathLen, FS_NO_ABOVE_ROOT_NAVIGATION);
    if (pathCleanLen <= 0) {
        return FS_DOES_NOT_EXIST;   /* Almost certainly because we're trying to navigate above the root directory. */
    }

    if (pathCleanLen >= sizeof(pPathCleanStack)) {
        pPathCleanHeap = (char*)fs_malloc(pathCleanLen + 1, fs_get_allocation_callbacks(pFS));
        if (pPathCleanHeap == NULL) {
            return FS_OUT_OF_MEMORY;
        }

        fs_path_normalize(pPathCleanHeap, pathCleanLen + 1, pPath, pathLen, FS_NO_ABOVE_ROOT_NAVIGATION);    /* This will never fail. */
        pPathClean = pPathCleanHeap;
    } else {
        pPathClean = pPathCleanStack;
    }

    /* Now that the input path has been cleaned we need only append it to the base path. */
    pSubFS = (fs_subfs*)fs_get_backend_data(pFS);
    FS_SUBFS_ASSERT(pSubFS != NULL);

    pSubFSPath->fullPathLen = fs_path_append(pSubFSPath->pFullPathStack, sizeof(pSubFSPath->pFullPathStack), pSubFS->pRootDir, pSubFS->rootDirLen, pPathClean, pathCleanLen);
    if (pSubFSPath->fullPathLen < 0) {
        fs_free(pPathCleanHeap, fs_get_allocation_callbacks(pFS));
        return FS_ERROR;    /* Should never hit this, but leaving here for safety. */
    }

    if (pSubFSPath->fullPathLen >= (int)sizeof(pSubFSPath->pFullPathStack)) {
        pSubFSPath->pFullPathHeap = (char*)fs_malloc(pSubFSPath->fullPathLen + 1, fs_get_allocation_callbacks(pFS));
        if (pSubFSPath->pFullPathHeap == NULL) {
            fs_free(pPathCleanHeap, fs_get_allocation_callbacks(pFS));
            return FS_OUT_OF_MEMORY;
        }

        fs_path_append(pSubFSPath->pFullPathHeap, pSubFSPath->fullPathLen + 1, pSubFS->pRootDir, pSubFS->rootDirLen, pPathClean, pathCleanLen);    /* This will never fail. */
        pSubFSPath->pFullPath = pSubFSPath->pFullPathHeap;
    } else {
        pSubFSPath->pFullPath = pSubFSPath->pFullPathStack;
    }

    return FS_SUCCESS;
}

static void fs_subfs_path_uninit(fs_subfs_path* pSubFSPath)
{
    if (pSubFSPath->pFullPathHeap != NULL) {
        fs_free(pSubFSPath->pFullPathHeap, fs_get_allocation_callbacks(NULL));
    }

    FS_SUBFS_ZERO_OBJECT(pSubFSPath);
}


static size_t fs_alloc_size_subfs(const void* pBackendConfig)
{
    fs_subfs_config* pSubFSConfig = (fs_subfs_config*)pBackendConfig;

    if (pSubFSConfig == NULL) {
        return 0;   /* The subfs config must be specified. */
    }

    /* We include a copy of the path with the main allocation. */
    return sizeof(fs_subfs) + strlen(pSubFSConfig->pRootDir) + 1 + 1;   /* +1 for trailing slash and +1 for null terminator. */
}

static fs_result fs_init_subfs(fs* pFS, const void* pBackendConfig, fs_stream* pStream)
{
    fs_subfs_config* pSubFSConfig = (fs_subfs_config*)pBackendConfig;
    fs_subfs* pSubFS;

    FS_SUBFS_ASSERT(pFS != NULL);
    FS_SUBFS_UNUSED(pStream);

    if (pSubFSConfig == NULL) {
        return FS_INVALID_ARGS; /* Must have a config. */
    }

    if (pSubFSConfig->pOwnerFS == NULL) {
        return FS_INVALID_ARGS; /* Must have an owner FS. */
    }

    if (pSubFSConfig->pRootDir == NULL) {
        return FS_INVALID_ARGS; /* Must have a root directory. */
    }

    pSubFS = (fs_subfs*)fs_get_backend_data(pFS);
    FS_SUBFS_ASSERT(pFS != NULL);

    pSubFS->pOwnerFS   = pSubFSConfig->pOwnerFS;
    pSubFS->pRootDir   = (char*)(pSubFS + 1);
    pSubFS->rootDirLen = strlen(pSubFSConfig->pRootDir);

    fs_strcpy(pSubFS->pRootDir, pSubFSConfig->pRootDir);

    /* Append a trailing slash if necessary. */
    if (pSubFS->pRootDir[pSubFS->rootDirLen - 1] != '/') {
        pSubFS->pRootDir[pSubFS->rootDirLen] = '/';
        pSubFS->pRootDir[pSubFS->rootDirLen + 1] = '\0';
        pSubFS->rootDirLen += 1;
    }

    return FS_SUCCESS;
}

static void fs_uninit_subfs(fs* pFS)
{
    /* Nothing to do here. */
    FS_SUBFS_UNUSED(pFS);
}

static fs_result fs_ioctl_subfs(fs* pFS, int op, void* pArgs)
{
    fs_subfs* pSubFS;

    pSubFS = (fs_subfs*)fs_get_backend_data(pFS);
    FS_SUBFS_ASSERT(pSubFS != NULL);

    return fs_ioctl(pSubFS->pOwnerFS, op, pArgs);
}

static fs_result fs_remove_subfs(fs* pFS, const char* pFilePath)
{
    fs_result result;
    fs_subfs* pSubFS;
    fs_subfs_path subfsPath;

    pSubFS = (fs_subfs*)fs_get_backend_data(pFS);
    FS_SUBFS_ASSERT(pSubFS != NULL);

    result = fs_subfs_path_init(pFS, pFilePath, FS_NULL_TERMINATED, &subfsPath);
    if (result != FS_SUCCESS) {
        return result;
    }

    result = fs_remove(pSubFS->pOwnerFS, subfsPath.pFullPath);
    fs_subfs_path_uninit(&subfsPath);

    return result;
}

static fs_result fs_rename_subfs(fs* pFS, const char* pOldName, const char* pNewName)
{
    fs_result result;
    fs_subfs* pSubFS;
    fs_subfs_path subfsPathOld;
    fs_subfs_path subfsPathNew;

    pSubFS = (fs_subfs*)fs_get_backend_data(pFS);
    FS_SUBFS_ASSERT(pSubFS != NULL);

    result = fs_subfs_path_init(pFS, pOldName, FS_NULL_TERMINATED, &subfsPathOld);
    if (result != FS_SUCCESS) {
        return result;
    }

    result = fs_subfs_path_init(pFS, pNewName, FS_NULL_TERMINATED, &subfsPathNew);
    if (result != FS_SUCCESS) {
        fs_subfs_path_uninit(&subfsPathOld);
        return result;
    }

    result = fs_rename(pSubFS->pOwnerFS, subfsPathOld.pFullPath, subfsPathNew.pFullPath);

    fs_subfs_path_uninit(&subfsPathOld);
    fs_subfs_path_uninit(&subfsPathNew);

    return result;
}

static fs_result fs_mkdir_subfs(fs* pFS, const char* pPath)
{
    fs_result result;
    fs_subfs* pSubFS;
    fs_subfs_path subfsPath;

    pSubFS = (fs_subfs*)fs_get_backend_data(pFS);
    FS_SUBFS_ASSERT(pSubFS != NULL);

    result = fs_subfs_path_init(pFS, pPath, FS_NULL_TERMINATED, &subfsPath);
    if (result != FS_SUCCESS) {
        return result;
    }

    result = fs_mkdir(pSubFS->pOwnerFS, subfsPath.pFullPath, FS_MKDIR_IGNORE_MOUNTS);
    fs_subfs_path_uninit(&subfsPath);

    return result;
}

static fs_result fs_info_subfs(fs* pFS, const char* pPath, int openMode, fs_file_info* pInfo)
{
    fs_result result;
    fs_subfs* pSubFS;
    fs_subfs_path subfsPath;

    pSubFS = (fs_subfs*)fs_get_backend_data(pFS);
    FS_SUBFS_ASSERT(pSubFS != NULL);

    result = fs_subfs_path_init(pFS, pPath, FS_NULL_TERMINATED, &subfsPath);
    if (result != FS_SUCCESS) {
        return result;
    }

    result = fs_info(pSubFS->pOwnerFS, subfsPath.pFullPath, openMode, pInfo);
    fs_subfs_path_uninit(&subfsPath);

    return result;
}

static size_t fs_file_alloc_size_subfs(fs* pFS)
{
    FS_SUBFS_UNUSED(pFS);
    return sizeof(fs_file_subfs);
}

static fs_result fs_file_open_subfs(fs* pFS, fs_stream* pStream, const char* pFilePath, int openMode, fs_file* pFile)
{
    fs_result result;
    fs_subfs_path subfsPath;
    fs_subfs* pSubFS;
    fs_file_subfs* pSubFSFile;

    FS_SUBFS_UNUSED(pStream);

    result = fs_subfs_path_init(pFS, pFilePath, FS_NULL_TERMINATED, &subfsPath);
    if (result != FS_SUCCESS) {
        return result;
    }

    pSubFS = (fs_subfs*)fs_get_backend_data(pFS);
    FS_SUBFS_ASSERT(pSubFS != NULL);

    pSubFSFile = (fs_file_subfs*)fs_file_get_backend_data(pFile);
    FS_SUBFS_ASSERT(pSubFSFile != NULL);

    result = fs_file_open(pSubFS->pOwnerFS, subfsPath.pFullPath, openMode, &pSubFSFile->pActualFile);
    fs_subfs_path_uninit(&subfsPath);

    return result;
}

static fs_result fs_file_open_handle_subfs(fs* pFS, void* hBackendFile, fs_file* pFile)
{
    fs_subfs* pSubFS;
    fs_file_subfs* pSubFSFile;

    pSubFS = (fs_subfs*)fs_get_backend_data(pFS);
    FS_SUBFS_ASSERT(pSubFS != NULL);

    pSubFSFile = (fs_file_subfs*)fs_file_get_backend_data(pFile);
    FS_SUBFS_ASSERT(pSubFSFile != NULL);

    return fs_file_open_from_handle(pSubFS->pOwnerFS, hBackendFile, &pSubFSFile->pActualFile);
}

static void fs_file_close_subfs(fs_file* pFile)
{
    fs_file_subfs* pSubFSFile = (fs_file_subfs*)fs_file_get_backend_data(pFile);
    FS_SUBFS_ASSERT(pSubFSFile != NULL);

    fs_file_close(pSubFSFile->pActualFile);
}

static fs_result fs_file_read_subfs(fs_file* pFile, void* pDst, size_t bytesToRead, size_t* pBytesRead)
{
    fs_file_subfs* pSubFSFile = (fs_file_subfs*)fs_file_get_backend_data(pFile);
    FS_SUBFS_ASSERT(pSubFSFile != NULL);

    return fs_file_read(pSubFSFile->pActualFile, pDst, bytesToRead, pBytesRead);
}

static fs_result fs_file_write_subfs(fs_file* pFile, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten)
{
    fs_file_subfs* pSubFSFile = (fs_file_subfs*)fs_file_get_backend_data(pFile);
    FS_SUBFS_ASSERT(pSubFSFile != NULL);

    return fs_file_write(pSubFSFile->pActualFile, pSrc, bytesToWrite, pBytesWritten);
}

static fs_result fs_file_seek_subfs(fs_file* pFile, fs_int64 offset, fs_seek_origin origin)
{
    fs_file_subfs* pSubFSFile = (fs_file_subfs*)fs_file_get_backend_data(pFile);
    FS_SUBFS_ASSERT(pSubFSFile != NULL);
    
    return fs_file_seek(pSubFSFile->pActualFile, offset, origin);
}

static fs_result fs_file_tell_subfs(fs_file* pFile, fs_int64* pCursor)
{
    fs_file_subfs* pSubFSFile = (fs_file_subfs*)fs_file_get_backend_data(pFile);
    FS_SUBFS_ASSERT(pSubFSFile != NULL);
    
    return fs_file_tell(pSubFSFile->pActualFile, pCursor);
}

static fs_result fs_file_flush_subfs(fs_file* pFile)
{
    fs_file_subfs* pSubFSFile = (fs_file_subfs*)fs_file_get_backend_data(pFile);
    FS_SUBFS_ASSERT(pSubFSFile != NULL);
    
    return fs_file_flush(pSubFSFile->pActualFile);
}

static fs_result fs_file_info_subfs(fs_file* pFile, fs_file_info* pInfo)
{
    fs_file_subfs* pSubFSFile = (fs_file_subfs*)fs_file_get_backend_data(pFile);
    FS_SUBFS_ASSERT(pSubFSFile != NULL);
    
    return fs_file_get_info(pSubFSFile->pActualFile, pInfo);
}

static fs_result fs_file_duplicate_subfs(fs_file* pFile, fs_file* pDuplicatedFile)
{
    fs_file_subfs* pSubFSFile;
    fs_file_subfs* pSubFSFileDuplicated;

    pSubFSFile = (fs_file_subfs*)fs_file_get_backend_data(pFile);
    FS_SUBFS_ASSERT(pSubFSFile != NULL);

    pSubFSFileDuplicated = (fs_file_subfs*)fs_file_get_backend_data(pDuplicatedFile);
    FS_SUBFS_ASSERT(pSubFSFileDuplicated != NULL);
    
    return fs_file_duplicate(pSubFSFile->pActualFile, &pSubFSFileDuplicated->pActualFile);
}

static fs_iterator* fs_first_subfs(fs* pFS, const char* pDirectoryPath, size_t directoryPathLen)
{
    fs_result result;
    fs_subfs* pSubFS;
    fs_subfs_path subfsPath;
    fs_iterator* pIterator;

    pSubFS = (fs_subfs*)fs_get_backend_data(pFS);
    FS_SUBFS_ASSERT(pSubFS != NULL);

    result = fs_subfs_path_init(pFS, pDirectoryPath, directoryPathLen, &subfsPath);
    if (result != FS_SUCCESS) {
        return NULL;
    }

    pIterator = fs_first(pSubFS->pOwnerFS, subfsPath.pFullPath, subfsPath.fullPathLen);
    fs_subfs_path_uninit(&subfsPath);

    return pIterator;
}

static fs_iterator* fs_next_subfs(fs_iterator* pIterator)
{
    return fs_next(pIterator);
}

static void fs_free_iterator_subfs(fs_iterator* pIterator)
{
    fs_free_iterator(pIterator);
}

fs_backend fs_subfs_backend =
{
    fs_alloc_size_subfs,
    fs_init_subfs,
    fs_uninit_subfs,
    fs_ioctl_subfs,
    fs_remove_subfs,
    fs_rename_subfs,
    fs_mkdir_subfs,
    fs_info_subfs,
    fs_file_alloc_size_subfs,
    fs_file_open_subfs,
    fs_file_open_handle_subfs,
    fs_file_close_subfs,
    fs_file_read_subfs,
    fs_file_write_subfs,
    fs_file_seek_subfs,
    fs_file_tell_subfs,
    fs_file_flush_subfs,
    fs_file_info_subfs,
    fs_file_duplicate_subfs,
    fs_first_subfs,
    fs_next_subfs,
    fs_free_iterator_subfs
};
const fs_backend* FS_SUBFS = &fs_subfs_backend;
/* END fs_subfs.c */

#endif  /* fs_subfs_c */
