#ifndef fs_sub_c
#define fs_sub_c

#include "../../../fs.h"
#include "fs_sub.h"

#include <assert.h>
#include <string.h>

#define FS_SUB_UNUSED(x) (void)x

static void fs_sub_zero_memory_default(void* p, size_t sz)
{
    if (sz > 0) {
        memset(p, 0, sz);
    }
}

#ifndef FS_SUB_ZERO_MEMORY
#define FS_SUB_ZERO_MEMORY(p, sz) fs_sub_zero_memory_default((p), (sz))
#endif
#define FS_SUB_ZERO_OBJECT(p)     FS_SUB_ZERO_MEMORY((p), sizeof(*(p)))

#ifndef FS_SUB_ASSERT
#define FS_SUB_ASSERT(x) assert(x)
#endif


/* Bit of a naughty hack. This is defined with FS_API in fs.c, but is not forward declared in fs.h. We can just declare it here. */
FS_API char* fs_strcpy(char* dst, const char* src);


/* BEG fs_sub.c */
typedef struct fs_sub
{
    fs* pOwnerFS;
    char* pRootDir;   /* Points to the end of the structure. */
    size_t rootDirLen;
} fs_sub;

typedef struct fs_file_sub
{
    fs_file* pActualFile;
} fs_file_sub;


typedef struct fs_sub_path
{
    char  pFullPathStack[1024];
    char* pFullPathHeap;
    char* pFullPath;
    int fullPathLen;
} fs_sub_path;

static fs_result fs_sub_path_init(fs* pFS, const char* pPath, size_t pathLen, fs_sub_path* pSubFSPath)
{
    fs_sub* pSubFS;
    char  pPathCleanStack[1024];
    char* pPathCleanHeap = NULL;
    char* pPathClean;
    size_t pathCleanLen;

    FS_SUB_ASSERT(pFS        != NULL);
    FS_SUB_ASSERT(pPath      != NULL);
    FS_SUB_ASSERT(pSubFSPath != NULL);

    FS_SUB_ZERO_OBJECT(pSubFSPath);   /* Safety. */

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
    pSubFS = (fs_sub*)fs_get_backend_data(pFS);
    FS_SUB_ASSERT(pSubFS != NULL);

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

static void fs_sub_path_uninit(fs_sub_path* pSubFSPath)
{
    if (pSubFSPath->pFullPathHeap != NULL) {
        fs_free(pSubFSPath->pFullPathHeap, fs_get_allocation_callbacks(NULL));
    }

    FS_SUB_ZERO_OBJECT(pSubFSPath);
}


static size_t fs_alloc_size_sub(const void* pBackendConfig)
{
    fs_sub_config* pSubFSConfig = (fs_sub_config*)pBackendConfig;

    if (pSubFSConfig == NULL) {
        return 0;   /* The sub config must be specified. */
    }

    /* We include a copy of the path with the main allocation. */
    return sizeof(fs_sub) + strlen(pSubFSConfig->pRootDir) + 1 + 1;   /* +1 for trailing slash and +1 for null terminator. */
}

static fs_result fs_init_sub(fs* pFS, const void* pBackendConfig, fs_stream* pStream)
{
    fs_sub_config* pSubFSConfig = (fs_sub_config*)pBackendConfig;
    fs_sub* pSubFS;

    FS_SUB_ASSERT(pFS != NULL);
    FS_SUB_UNUSED(pStream);

    if (pSubFSConfig == NULL) {
        return FS_INVALID_ARGS; /* Must have a config. */
    }

    if (pSubFSConfig->pOwnerFS == NULL) {
        return FS_INVALID_ARGS; /* Must have an owner FS. */
    }

    if (pSubFSConfig->pRootDir == NULL) {
        return FS_INVALID_ARGS; /* Must have a root directory. */
    }

    pSubFS = (fs_sub*)fs_get_backend_data(pFS);
    FS_SUB_ASSERT(pFS != NULL);

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

static void fs_uninit_sub(fs* pFS)
{
    /* Nothing to do here. */
    FS_SUB_UNUSED(pFS);
}

static fs_result fs_ioctl_sub(fs* pFS, int op, void* pArgs)
{
    fs_sub* pSubFS;

    pSubFS = (fs_sub*)fs_get_backend_data(pFS);
    FS_SUB_ASSERT(pSubFS != NULL);

    return fs_ioctl(pSubFS->pOwnerFS, op, pArgs);
}

static fs_result fs_remove_sub(fs* pFS, const char* pFilePath)
{
    fs_result result;
    fs_sub* pSubFS;
    fs_sub_path subPath;

    pSubFS = (fs_sub*)fs_get_backend_data(pFS);
    FS_SUB_ASSERT(pSubFS != NULL);

    result = fs_sub_path_init(pFS, pFilePath, FS_NULL_TERMINATED, &subPath);
    if (result != FS_SUCCESS) {
        return result;
    }

    result = fs_remove(pSubFS->pOwnerFS, subPath.pFullPath);
    fs_sub_path_uninit(&subPath);

    return result;
}

static fs_result fs_rename_sub(fs* pFS, const char* pOldName, const char* pNewName)
{
    fs_result result;
    fs_sub* pSubFS;
    fs_sub_path subPathOld;
    fs_sub_path subPathNew;

    pSubFS = (fs_sub*)fs_get_backend_data(pFS);
    FS_SUB_ASSERT(pSubFS != NULL);

    result = fs_sub_path_init(pFS, pOldName, FS_NULL_TERMINATED, &subPathOld);
    if (result != FS_SUCCESS) {
        return result;
    }

    result = fs_sub_path_init(pFS, pNewName, FS_NULL_TERMINATED, &subPathNew);
    if (result != FS_SUCCESS) {
        fs_sub_path_uninit(&subPathOld);
        return result;
    }

    result = fs_rename(pSubFS->pOwnerFS, subPathOld.pFullPath, subPathNew.pFullPath);

    fs_sub_path_uninit(&subPathOld);
    fs_sub_path_uninit(&subPathNew);

    return result;
}

static fs_result fs_mkdir_sub(fs* pFS, const char* pPath)
{
    fs_result result;
    fs_sub* pSubFS;
    fs_sub_path subPath;

    pSubFS = (fs_sub*)fs_get_backend_data(pFS);
    FS_SUB_ASSERT(pSubFS != NULL);

    result = fs_sub_path_init(pFS, pPath, FS_NULL_TERMINATED, &subPath);
    if (result != FS_SUCCESS) {
        return result;
    }

    result = fs_mkdir(pSubFS->pOwnerFS, subPath.pFullPath, FS_IGNORE_MOUNTS);
    fs_sub_path_uninit(&subPath);

    return result;
}

static fs_result fs_info_sub(fs* pFS, const char* pPath, int openMode, fs_file_info* pInfo)
{
    fs_result result;
    fs_sub* pSubFS;
    fs_sub_path subPath;

    pSubFS = (fs_sub*)fs_get_backend_data(pFS);
    FS_SUB_ASSERT(pSubFS != NULL);

    result = fs_sub_path_init(pFS, pPath, FS_NULL_TERMINATED, &subPath);
    if (result != FS_SUCCESS) {
        return result;
    }

    result = fs_info(pSubFS->pOwnerFS, subPath.pFullPath, openMode, pInfo);
    fs_sub_path_uninit(&subPath);

    return result;
}

static size_t fs_file_alloc_size_sub(fs* pFS)
{
    FS_SUB_UNUSED(pFS);
    return sizeof(fs_file_sub);
}

static fs_result fs_file_open_sub(fs* pFS, fs_stream* pStream, const char* pFilePath, int openMode, fs_file* pFile)
{
    fs_result result;
    fs_sub_path subPath;
    fs_sub* pSubFS;
    fs_file_sub* pSubFSFile;

    FS_SUB_UNUSED(pStream);

    result = fs_sub_path_init(pFS, pFilePath, FS_NULL_TERMINATED, &subPath);
    if (result != FS_SUCCESS) {
        return result;
    }

    pSubFS = (fs_sub*)fs_get_backend_data(pFS);
    FS_SUB_ASSERT(pSubFS != NULL);

    pSubFSFile = (fs_file_sub*)fs_file_get_backend_data(pFile);
    FS_SUB_ASSERT(pSubFSFile != NULL);

    result = fs_file_open(pSubFS->pOwnerFS, subPath.pFullPath, openMode, &pSubFSFile->pActualFile);
    fs_sub_path_uninit(&subPath);

    return result;
}

static void fs_file_close_sub(fs_file* pFile)
{
    fs_file_sub* pSubFSFile = (fs_file_sub*)fs_file_get_backend_data(pFile);
    FS_SUB_ASSERT(pSubFSFile != NULL);

    fs_file_close(pSubFSFile->pActualFile);
}

static fs_result fs_file_read_sub(fs_file* pFile, void* pDst, size_t bytesToRead, size_t* pBytesRead)
{
    fs_file_sub* pSubFSFile = (fs_file_sub*)fs_file_get_backend_data(pFile);
    FS_SUB_ASSERT(pSubFSFile != NULL);

    return fs_file_read(pSubFSFile->pActualFile, pDst, bytesToRead, pBytesRead);
}

static fs_result fs_file_write_sub(fs_file* pFile, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten)
{
    fs_file_sub* pSubFSFile = (fs_file_sub*)fs_file_get_backend_data(pFile);
    FS_SUB_ASSERT(pSubFSFile != NULL);

    return fs_file_write(pSubFSFile->pActualFile, pSrc, bytesToWrite, pBytesWritten);
}

static fs_result fs_file_seek_sub(fs_file* pFile, fs_int64 offset, fs_seek_origin origin)
{
    fs_file_sub* pSubFSFile = (fs_file_sub*)fs_file_get_backend_data(pFile);
    FS_SUB_ASSERT(pSubFSFile != NULL);
    
    return fs_file_seek(pSubFSFile->pActualFile, offset, origin);
}

static fs_result fs_file_tell_sub(fs_file* pFile, fs_int64* pCursor)
{
    fs_file_sub* pSubFSFile = (fs_file_sub*)fs_file_get_backend_data(pFile);
    FS_SUB_ASSERT(pSubFSFile != NULL);
    
    return fs_file_tell(pSubFSFile->pActualFile, pCursor);
}

static fs_result fs_file_flush_sub(fs_file* pFile)
{
    fs_file_sub* pSubFSFile = (fs_file_sub*)fs_file_get_backend_data(pFile);
    FS_SUB_ASSERT(pSubFSFile != NULL);
    
    return fs_file_flush(pSubFSFile->pActualFile);
}

static fs_result fs_file_info_sub(fs_file* pFile, fs_file_info* pInfo)
{
    fs_file_sub* pSubFSFile = (fs_file_sub*)fs_file_get_backend_data(pFile);
    FS_SUB_ASSERT(pSubFSFile != NULL);
    
    return fs_file_get_info(pSubFSFile->pActualFile, pInfo);
}

static fs_result fs_file_duplicate_sub(fs_file* pFile, fs_file* pDuplicatedFile)
{
    fs_file_sub* pSubFSFile;
    fs_file_sub* pSubFSFileDuplicated;

    pSubFSFile = (fs_file_sub*)fs_file_get_backend_data(pFile);
    FS_SUB_ASSERT(pSubFSFile != NULL);

    pSubFSFileDuplicated = (fs_file_sub*)fs_file_get_backend_data(pDuplicatedFile);
    FS_SUB_ASSERT(pSubFSFileDuplicated != NULL);
    
    return fs_file_duplicate(pSubFSFile->pActualFile, &pSubFSFileDuplicated->pActualFile);
}

static fs_iterator* fs_first_sub(fs* pFS, const char* pDirectoryPath, size_t directoryPathLen)
{
    fs_result result;
    fs_sub* pSubFS;
    fs_sub_path subPath;
    fs_iterator* pIterator;

    pSubFS = (fs_sub*)fs_get_backend_data(pFS);
    FS_SUB_ASSERT(pSubFS != NULL);

    result = fs_sub_path_init(pFS, pDirectoryPath, directoryPathLen, &subPath);
    if (result != FS_SUCCESS) {
        return NULL;
    }

    pIterator = fs_first(pSubFS->pOwnerFS, subPath.pFullPath, subPath.fullPathLen);
    fs_sub_path_uninit(&subPath);

    return pIterator;
}

static fs_iterator* fs_next_sub(fs_iterator* pIterator)
{
    return fs_next(pIterator);
}

static void fs_free_iterator_sub(fs_iterator* pIterator)
{
    fs_free_iterator(pIterator);
}

fs_backend fs_sub_backend =
{
    fs_alloc_size_sub,
    fs_init_sub,
    fs_uninit_sub,
    fs_ioctl_sub,
    fs_remove_sub,
    fs_rename_sub,
    fs_mkdir_sub,
    fs_info_sub,
    fs_file_alloc_size_sub,
    fs_file_open_sub,
    fs_file_close_sub,
    fs_file_read_sub,
    fs_file_write_sub,
    fs_file_seek_sub,
    fs_file_tell_sub,
    fs_file_flush_sub,
    fs_file_info_sub,
    fs_file_duplicate_sub,
    fs_first_sub,
    fs_next_sub,
    fs_free_iterator_sub
};
const fs_backend* FS_SUB = &fs_sub_backend;
/* END fs_sub.c */

#endif  /* fs_sub_c */
