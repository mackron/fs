#ifndef fs_win32_c
#define fs_win32_c

#include "fs_win32.h"

#include <string.h>

#if defined(_WIN32)
#include <windows.h>

/* TODO: Remove this when this file is amalgamated into the main file. */
#ifndef FS_COUNTOF
#define FS_COUNTOF(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif
#ifndef FS_ZERO_OBJECT
#define FS_ZERO_OBJECT(p) memset((p), 0, sizeof(*(p)))
#endif
#ifndef FS_MAX
#define FS_MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif
#ifndef FS_MIN
#define FS_MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif
FS_API char* fs_strcpy(char* dst, const char* src);
FS_API int fs_strncpy_s(char* dst, size_t dstCap, const char* src, size_t count);

FS_API fs_result fs_result_from_GetLastError();
FS_API fs_uint64 fs_FILETIME_to_unix(const FILETIME* pFT);


#if defined(UNICODE) || defined(_UNICODE)
#define fs_win32_char wchar_t
#else
#define fs_win32_char char
#endif

typedef struct
{
    size_t len;
    fs_win32_char* path;
    fs_win32_char  pathStack[256];
    fs_win32_char* pathHeap;
} fs_win32_path;

static void fs_win32_path_init_internal(fs_win32_path* pPath)
{
    pPath->len = 0;
    pPath->path = pPath->pathStack;
    pPath->pathStack[0] = '\0';
    pPath->pathHeap = NULL;
}

static fs_result fs_win32_path_init(fs_win32_path* pPath, const char* pPathUTF8, size_t pathUTF8Len, const fs_allocation_callbacks* pAllocationCallbacks)
{
    size_t i;

    fs_win32_path_init_internal(pPath);

    #if defined(UNICODE) || defined(_UNICODE)
    {
        int wideCharLen;
        int cbMultiByte;

        if (pathUTF8Len == (size_t)-1) {
            cbMultiByte = (int)-1;
        } else {
            cbMultiByte = (int)pathUTF8Len + 1;
        }

        wideCharLen = MultiByteToWideChar(CP_UTF8, 0, pPathUTF8, cbMultiByte, NULL, 0);
        if (wideCharLen == 0) {
            return FS_ERROR;
        }

        /* Use the stack if possible. If not, allocate on the heap. */
        if (wideCharLen <= (int)FS_COUNTOF(pPath->pathStack)) {
            pPath->path = pPath->pathStack;
        } else {
            pPath->pathHeap = (fs_win32_char*)fs_malloc(sizeof(fs_win32_char) * wideCharLen, pAllocationCallbacks);
            if (pPath->pathHeap == NULL) {
                return FS_OUT_OF_MEMORY;
            }

            pPath->path = pPath->pathHeap;
        }

        MultiByteToWideChar(CP_UTF8, 0, pPathUTF8, cbMultiByte, pPath->path, wideCharLen);
        pPath->len = wideCharLen - 1;  /* The count returned by MultiByteToWideChar() includes the null terminator, so subtract 1 to compensate. */

        /* Convert forward slashes to back slashes for compatibility. */
        for (i = 0; i < pPath->len; i += 1) {
            if (pPath->path[i] == '/') {
                pPath->path[i] = '\\';
            }
        }

        return FS_SUCCESS;
    }
    #else
    {
        /*
        Not doing any conversion here. Just assuming the path is an ANSI path. We need to copy over the string
        and convert slashes to backslashes.
        */
        if (pathUTF8Len == (size_t)-1) {
            pPath->len = strlen(pPathUTF8);
        } else {
            pPath->len = pathUTF8Len;
        }

        if (pPath->len >= sizeof(pPath->pathStack)) {
            pPath->pathHeap = (fs_win32_char*)fs_malloc(sizeof(fs_win32_char) * (pPath->len + 1), pAllocationCallbacks);
            if (pPath->pathHeap == NULL) {
                return FS_OUT_OF_MEMORY;
            }

            pPath->path = pPath->pathHeap;
        }

        fs_strcpy(pPath->path, pPathUTF8);
        for (i = 0; i < pPath->len; i += 1) {
            if (pPath->path[i] == '/') {
                pPath->path[i] = '\\';
            }
        }

        return FS_SUCCESS;
    }
    #endif
}



static fs_result fs_win32_path_append(fs_win32_path* pPath, const char* pAppendUTF8, const fs_allocation_callbacks* pAllocationCallbacks)
{
    fs_result result;
    fs_win32_path append;
    size_t newLen;

    result = fs_win32_path_init(&append, pAppendUTF8, (size_t)-1, pAllocationCallbacks);
    if (result != FS_SUCCESS) {
        return result;
    }

    newLen = pPath->len + append.len;

    if (pPath->path == pPath->pathHeap) {
        /* It's on the heap. Just realloc. */
        fs_win32_char* pNewHeap = (fs_win32_char*)fs_realloc(pPath->pathHeap, sizeof(fs_win32_char) * (newLen + 1), pAllocationCallbacks);
        if (pNewHeap == NULL) {
            return FS_OUT_OF_MEMORY;
        }

        pPath->pathHeap = pNewHeap;
        pPath->path     = pNewHeap;
    } else {
        /* Getting here means it's on the stack. We may need to transfer to the heap. */
        if (newLen >= FS_COUNTOF(pPath->pathStack)) {
            /* There's not enough room on the stack. We need to move the string from the stack to the heap. */
            pPath->pathHeap = (fs_win32_char*)fs_malloc(sizeof(fs_win32_char) * (newLen + 1), pAllocationCallbacks);
            if (pPath->pathHeap == NULL) {
                return FS_OUT_OF_MEMORY;
            }

            memcpy(pPath->pathHeap, pPath->pathStack, sizeof(fs_win32_char) * (pPath->len + 1));
            pPath->path = pPath->pathHeap;
        } else {
            /* There's enough room on the stack. No modifications needed. */
        }
    }

    /* Now we can append. */
    memcpy(pPath->path + pPath->len, append.path, sizeof(fs_win32_char) * (append.len + 1));  /* Null terminator copied in-place. */
    pPath->len = newLen;

    return FS_SUCCESS;
}



static void fs_win32_path_uninit(fs_win32_path* pPath, const fs_allocation_callbacks* pAllocationCallbacks)
{
    if (pPath->pathHeap) {
        fs_free(pPath->pathHeap, pAllocationCallbacks);
        pPath->pathHeap = NULL;
    }
}


static fs_file_info fs_file_info_from_WIN32_FIND_DATA(const WIN32_FIND_DATA* pFD)
{
    fs_file_info info;

    FS_ZERO_OBJECT(&info);
    info.size             = ((fs_uint64)pFD->nFileSizeHigh << 32) | (fs_uint64)pFD->nFileSizeLow;
    info.lastModifiedTime = fs_FILETIME_to_unix(&pFD->ftLastWriteTime);
    info.lastAccessTime   = fs_FILETIME_to_unix(&pFD->ftLastAccessTime);
    info.directory        = (pFD->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)     != 0;
    info.symlink          = (pFD->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;

    return info;
}

static fs_file_info fs_file_info_from_HANDLE_FILE_INFORMATION(const BY_HANDLE_FILE_INFORMATION* pFileInfo)
{
    fs_file_info info;

    FS_ZERO_OBJECT(&info);
    info.size             = ((fs_uint64)pFileInfo->nFileSizeHigh << 32) | (fs_uint64)pFileInfo->nFileSizeLow;
    info.lastModifiedTime = fs_FILETIME_to_unix(&pFileInfo->ftLastWriteTime);
    info.lastAccessTime   = fs_FILETIME_to_unix(&pFileInfo->ftLastAccessTime);
    info.directory        = (pFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

    return info;
}


static size_t fs_alloc_size_win32(const void* pBackendConfig)
{
    (void)pBackendConfig;
    return 0;
}

static fs_result fs_init_win32(fs* pFS, const void* pBackendConfig, fs_stream* pStream)
{
    (void)pFS;
    (void)pBackendConfig;
    (void)pStream;

    return FS_SUCCESS;
}

static void fs_uninit_win32(fs* pFS)
{
    (void)pFS;
}

static fs_result fs_ioctl_win32(fs* pFS, int op, void* pArg)
{
    (void)pFS;
    (void)op;
    (void)pArg;

    return FS_NOT_IMPLEMENTED;
}

static fs_result fs_remove_win32(fs* pFS, const char* pFilePath)
{
    BOOL resultWin32;
    fs_result result;
    fs_win32_path path;

    result = fs_win32_path_init(&path, pFilePath, (size_t)-1, fs_get_allocation_callbacks(pFS));
    if (result != FS_SUCCESS) {
        return result;
    }

    resultWin32 = DeleteFile(path.path);
    if (resultWin32 == FS_FALSE) {
        /* It may have been a directory. */
        DWORD error = GetLastError();
        if (error == ERROR_ACCESS_DENIED || error == ERROR_FILE_NOT_FOUND) {
            DWORD attributes = GetFileAttributes(path.path);
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
                resultWin32 = RemoveDirectory(path.path);
                if (resultWin32 == FS_FALSE) {
                    result = fs_result_from_GetLastError();
                    goto done;
                } else {
                    return FS_SUCCESS;
                }
            } else {
                result = fs_result_from_GetLastError();
                goto done;
            }
        } else {
            result = fs_result_from_GetLastError();
            goto done;
        }

        result = fs_result_from_GetLastError();
        goto done;
    } else {
        result = FS_SUCCESS;
    }

done:
    fs_win32_path_uninit(&path, fs_get_allocation_callbacks(pFS));

    (void)pFS;
    return result;
}

static fs_result fs_rename_win32(fs* pFS, const char* pOldName, const char* pNewName)
{
    BOOL resultWin32;
    fs_result result;
    fs_win32_path pathOld;
    fs_win32_path pathNew;

    result = fs_win32_path_init(&pathOld, pOldName, (size_t)-1, fs_get_allocation_callbacks(pFS));
    if (result != FS_SUCCESS) {
        return result;
    }

    result = fs_win32_path_init(&pathNew, pNewName, (size_t)-1, fs_get_allocation_callbacks(pFS));
    if (result != FS_SUCCESS) {
        fs_win32_path_uninit(&pathOld, fs_get_allocation_callbacks(pFS));
        return result;
    }

    resultWin32 = MoveFile(pathOld.path, pathNew.path);
    if (resultWin32 == FS_FALSE) {
        result = fs_result_from_GetLastError();
    }

    fs_win32_path_uninit(&pathOld, fs_get_allocation_callbacks(pFS));
    fs_win32_path_uninit(&pathNew, fs_get_allocation_callbacks(pFS));

    (void)pFS;
    return result;
}

static fs_result fs_mkdir_win32(fs* pFS, const char* pPath)
{
    BOOL resultWin32;
    fs_result result;
    fs_win32_path path;

    /* If it's a drive letter segment just pretend it's successful. */
    if ((pPath[0] >= 'a' && pPath[0] <= 'z') || (pPath[0] >= 'A' && pPath[0] <= 'Z')) {
        if (pPath[1] == ':' && pPath[2] == '\0') {
            return FS_SUCCESS;
        }
    }

    result = fs_win32_path_init(&path, pPath, (size_t)-1, fs_get_allocation_callbacks(pFS));
    if (result != FS_SUCCESS) {
        return result;
    }

    resultWin32 = CreateDirectory(path.path, NULL);
    if (resultWin32 == FS_FALSE) {
        result = fs_result_from_GetLastError();
        goto done;
    }

done:
    fs_win32_path_uninit(&path, fs_get_allocation_callbacks(pFS));

    (void)pFS;
    return FS_SUCCESS;
}

static fs_result fs_info_from_stdio_win32(HANDLE hFile, fs_file_info* pInfo)
{
    BY_HANDLE_FILE_INFORMATION fileInfo;

    if (GetFileInformationByHandle(hFile, &fileInfo) == FS_FALSE) {
        return fs_result_from_GetLastError();
    }

    *pInfo = fs_file_info_from_HANDLE_FILE_INFORMATION(&fileInfo);

    return FS_SUCCESS;
}

static fs_result fs_info_win32(fs* pFS, const char* pPath, int openMode, fs_file_info* pInfo)
{
    HANDLE hFind;
    WIN32_FIND_DATA fd;
    fs_result result;
    fs_win32_path path;

    /* Special case for standard IO files. */
    /*  */ if (pPath == FS_STDIN ) {
        return fs_info_from_stdio_win32(GetStdHandle(STD_INPUT_HANDLE ), pInfo);
    } else if (pPath == FS_STDOUT) {
        return fs_info_from_stdio_win32(GetStdHandle(STD_OUTPUT_HANDLE), pInfo);
    } else if (pPath == FS_STDERR) {
        return fs_info_from_stdio_win32(GetStdHandle(STD_ERROR_HANDLE ), pInfo);
    }

    result = fs_win32_path_init(&path, pPath, (size_t)-1, fs_get_allocation_callbacks(pFS));
    if (result != FS_SUCCESS) {
        return result;
    }

    hFind = FindFirstFile(path.path, &fd);

    fs_win32_path_uninit(&path, fs_get_allocation_callbacks(pFS));

    if (hFind == INVALID_HANDLE_VALUE) {
        result = fs_result_from_GetLastError();
        goto done;
    } else {
        result = FS_SUCCESS;
    }

    FindClose(hFind);
    hFind = NULL;

    *pInfo = fs_file_info_from_WIN32_FIND_DATA(&fd);

done:
    (void)openMode;
    (void)pFS;
    return result;
}


typedef struct fs_file_win32
{
    HANDLE hFile;
    fs_bool32 isStandardHandle;
    int openMode;       /* The original open mode for duplication purposes. */
    char* pFilePath;
    char  pFilePathStack[256];
    char* pFilePathHeap;
} fs_file_win32;

static size_t fs_file_alloc_size_win32(fs* pFS)
{
    (void)pFS;
    return sizeof(fs_file_win32);
}

static fs_result fs_file_open_win32(fs* pFS, fs_stream* pStream, const char* pFilePath, int openMode, fs_file* pFile)
{
    fs_file_win32* pFileWin32 = (fs_file_win32*)fs_file_get_backend_data(pFile);
    fs_result result;
    fs_win32_path path;
    HANDLE hFile;
    DWORD dwDesiredAccess = 0;
    DWORD dwShareMode = 0;
    DWORD dwCreationDisposition = OPEN_EXISTING;

    /*  */ if (pFilePath == FS_STDIN ) {
        hFile = GetStdHandle(STD_INPUT_HANDLE);
        pFileWin32->isStandardHandle = FS_TRUE;
    } else if (pFilePath == FS_STDOUT) {
        hFile = GetStdHandle(STD_OUTPUT_HANDLE);
        pFileWin32->isStandardHandle = FS_TRUE;
    } else if (pFilePath == FS_STDERR) {
        hFile = GetStdHandle(STD_ERROR_HANDLE);
        pFileWin32->isStandardHandle = FS_TRUE;
    } else {
        pFileWin32->isStandardHandle = FS_FALSE;
    }

    if (pFileWin32->isStandardHandle) {
        pFileWin32->hFile = hFile;
        return FS_SUCCESS;
    }


    if ((openMode & FS_READ) != 0) {
        dwDesiredAccess      |= GENERIC_READ;
        dwShareMode          |= FILE_SHARE_READ;
        dwCreationDisposition = OPEN_EXISTING;  /* In read mode, our default is to open an existing file, and fail if it doesn't exist. This can be overwritten in the write case below. */
    }

    if ((openMode & FS_WRITE) != 0) {
        dwShareMode |= FILE_SHARE_WRITE;

        if ((openMode & FS_EXCLUSIVE) != 0) {
            dwDesiredAccess      |= GENERIC_WRITE;
            dwCreationDisposition = CREATE_NEW;
        } else if ((openMode & FS_APPEND) != 0) {
            dwDesiredAccess      |= FILE_APPEND_DATA;
            dwCreationDisposition = OPEN_ALWAYS;
        } else if ((openMode & FS_TRUNCATE) != 0) {
            dwDesiredAccess      |= GENERIC_WRITE;
            dwCreationDisposition = CREATE_ALWAYS;
        } else {
            dwDesiredAccess      |= GENERIC_WRITE;
            dwCreationDisposition = OPEN_ALWAYS;
        }
    }

    /* As an added safety check, make sure one or both of read and write was specified. */
    if (dwDesiredAccess == 0) {
        return FS_INVALID_ARGS;
    }

    result = fs_win32_path_init(&path, pFilePath, (size_t)-1, fs_get_allocation_callbacks(pFS));
    if (result != FS_SUCCESS) {
        return result;
    }

    hFile = CreateFile(path.path, dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        result = fs_result_from_GetLastError();
    } else {
        result = FS_SUCCESS;
        pFileWin32->hFile = hFile;
    }

    if (result != FS_SUCCESS) {
        return result;
    }

    /* We need to keep track of the open mode for duplication purposes. */
    pFileWin32->openMode = openMode;

    /* We need to make a copy of the path for duplication purposes. */
    if (path.len < FS_COUNTOF(pFileWin32->pFilePathStack)) {
        pFileWin32->pFilePath = pFileWin32->pFilePathStack;
    } else {
        pFileWin32->pFilePathHeap = (char*)fs_malloc(path.len + 1, fs_get_allocation_callbacks(pFS));
        if (pFileWin32->pFilePathHeap == NULL) {
            result = FS_OUT_OF_MEMORY;
            if (pFileWin32->isStandardHandle == FS_FALSE) {
                CloseHandle(pFileWin32->hFile);
            }

            fs_win32_path_uninit(&path, fs_get_allocation_callbacks(pFS));
            return result;
        }

        pFileWin32->pFilePath = pFileWin32->pFilePathHeap;
    }

    fs_strcpy(pFileWin32->pFilePath, pFilePath);


    /* All done. */
    fs_win32_path_uninit(&path, fs_get_allocation_callbacks(pFS));

    (void)pFS;
    (void)pStream;
    return FS_SUCCESS;
}

static void fs_file_close_win32(fs_file* pFile)
{
    fs_file_win32* pFileWin32 = (fs_file_win32*)fs_file_get_backend_data(pFile);

    if (pFileWin32->isStandardHandle == FS_FALSE) {
        CloseHandle(pFileWin32->hFile);
        fs_free(pFileWin32->pFilePathHeap, fs_get_allocation_callbacks(fs_file_get_fs(pFile)));
    }
}

static fs_result fs_file_read_win32(fs_file* pFile, void* pDst, size_t bytesToRead, size_t* pBytesRead)
{
    fs_file_win32* pFileWin32 = (fs_file_win32*)fs_file_get_backend_data(pFile);
    BOOL resultWin32;
    size_t bytesRemaining = bytesToRead;
    char* pRunningDst = (char*)pDst;

    /*
    ReadFile() expects a DWORD for the number of bytes to read which means we'll need to run it in a loop in case
    our bytesToRead argument is larger than 4GB.
    */
    while (bytesRemaining > 0) {
        DWORD bytesToReadNow = (DWORD)FS_MIN(bytesRemaining, (size_t)0xFFFFFFFF);
        DWORD bytesReadNow;

        resultWin32 = ReadFile(pFileWin32->hFile, pRunningDst, bytesToReadNow, &bytesReadNow, NULL);
        if (resultWin32 == FS_FALSE) {
            return fs_result_from_GetLastError();
        }

        bytesRemaining -= bytesReadNow;
        pRunningDst    += bytesReadNow;
    }

    *pBytesRead = bytesToRead - bytesRemaining;
    
    if (*pBytesRead == 0) {
        return FS_AT_END;
    }

    return FS_SUCCESS;
}

static fs_result fs_file_write_win32(fs_file* pFile, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten)
{
    fs_file_win32* pFileWin32 = (fs_file_win32*)fs_file_get_backend_data(pFile);
    BOOL resultWin32;
    size_t bytesRemaining = bytesToWrite;
    const char* pRunningSrc = (const char*)pSrc;

    /*
    WriteFile() expects a DWORD for the number of bytes to write which means we'll need to run it in a loop in case
    our bytesToWrite argument is larger than 4GB.
    */
    while (bytesRemaining > 0) {
        DWORD bytesToWriteNow = (DWORD)FS_MIN(bytesRemaining, (size_t)0xFFFFFFFF);
        DWORD bytesWrittenNow;

        resultWin32 = WriteFile(pFileWin32->hFile, pRunningSrc, bytesToWriteNow, &bytesWrittenNow, NULL);
        if (resultWin32 == FS_FALSE) {
            return fs_result_from_GetLastError();
        }

        bytesRemaining -= bytesWrittenNow;
        pRunningSrc    += bytesWrittenNow;
    }

    *pBytesWritten = bytesToWrite - bytesRemaining;

    return FS_SUCCESS;
}

static fs_result fs_file_seek_win32(fs_file* pFile, fs_int64 offset, fs_seek_origin origin)
{
    fs_file_win32* pFileWin32 = (fs_file_win32*)fs_file_get_backend_data(pFile);
    LARGE_INTEGER liDistanceToMove;
    DWORD dwMoveMethod;

    switch (origin) {
    case FS_SEEK_SET:
        dwMoveMethod = FILE_BEGIN;
        liDistanceToMove.QuadPart = offset;
        break;
    case FS_SEEK_CUR:
        dwMoveMethod = FILE_CURRENT;
        liDistanceToMove.QuadPart = offset;
        break;
    case FS_SEEK_END:
        dwMoveMethod = FILE_END;
        liDistanceToMove.QuadPart = offset;
        break;
    default:
        return FS_INVALID_ARGS;
    }

    /*
    Use SetFilePointer() instead of SetFilePointerEx() for compatibility with old Windows.

    Note from MSDN:

        If you do not need the high order 32-bits, this pointer must be set to NULL.
    */
    if (SetFilePointer(pFileWin32->hFile, liDistanceToMove.LowPart, (liDistanceToMove.HighPart == 0 ? NULL : &liDistanceToMove.HighPart), dwMoveMethod) == INVALID_SET_FILE_POINTER) {
        return fs_result_from_GetLastError();
    }

    return FS_SUCCESS;
}

static fs_result fs_file_tell_win32(fs_file* pFile, fs_int64* pCursor)
{
    fs_file_win32* pFileWin32 = (fs_file_win32*)fs_file_get_backend_data(pFile);
    LARGE_INTEGER liCursor;

    liCursor.HighPart = 0;
    liCursor.LowPart  = SetFilePointer(pFileWin32->hFile, 0, &liCursor.HighPart, FILE_CURRENT);

    if (liCursor.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
        return fs_result_from_GetLastError();
    }

    *pCursor = liCursor.QuadPart;

    return FS_SUCCESS;
}

static fs_result fs_file_flush_win32(fs_file* pFile)
{
    fs_file_win32* pFileWin32 = (fs_file_win32*)fs_file_get_backend_data(pFile);

    if (FlushFileBuffers(pFileWin32->hFile) == FS_FALSE) {
        return fs_result_from_GetLastError();
    }

    return FS_SUCCESS;
}

static fs_result fs_file_truncate_win32(fs_file* pFile)
{
    fs_file_win32* pFileWin32 = (fs_file_win32*)fs_file_get_backend_data(pFile);

    if (SetEndOfFile(pFileWin32->hFile) == FS_FALSE) {
        return fs_result_from_GetLastError();
    }

    return FS_SUCCESS;
}

static fs_result fs_file_info_win32(fs_file* pFile, fs_file_info* pInfo)
{
    fs_file_win32* pFileWin32 = (fs_file_win32*)fs_file_get_backend_data(pFile);
    BY_HANDLE_FILE_INFORMATION fileInfo;

    if (GetFileInformationByHandle(pFileWin32->hFile, &fileInfo) == FS_FALSE) {
        return fs_result_from_GetLastError();
    }

    *pInfo = fs_file_info_from_HANDLE_FILE_INFORMATION(&fileInfo);

    return FS_SUCCESS;
}

static fs_result fs_file_duplicate_win32(fs_file* pFile, fs_file* pDuplicate)
{
    fs_file_win32* pFileWin32      = (fs_file_win32*)fs_file_get_backend_data(pFile);
    fs_file_win32* pDuplicateWin32 = (fs_file_win32*)fs_file_get_backend_data(pDuplicate);
    fs_result result;
    BY_HANDLE_FILE_INFORMATION info1, info2;

    if (pFileWin32->isStandardHandle) {
        pDuplicateWin32->hFile = pFileWin32->hFile;
        pDuplicateWin32->isStandardHandle = FS_TRUE;
        
        return FS_SUCCESS;
    }

    /*
    We cannot duplicate the handle because that will result in a shared read/write pointer. We need to
    open the file again with the same path and flags. We're not going to allow duplication of files
    that were opened in write mode.
    */
    if ((pFileWin32->openMode & FS_WRITE) != 0) {
        return FS_INVALID_OPERATION;
    }

    result = fs_file_open_win32(fs_file_get_fs(pFile), NULL, pFileWin32->pFilePath, pFileWin32->openMode, pDuplicate);
    if (result != FS_SUCCESS) {
        return result;
    }

    /* Now check the file information in case it got replaced with a different file from under us. */
    if (GetFileInformationByHandle(pFileWin32->hFile, &info1) == FS_FALSE) {
        fs_file_close_win32(pDuplicate);
        return fs_result_from_GetLastError();
    }
    if (GetFileInformationByHandle(pDuplicateWin32->hFile, &info2) == FS_FALSE) {
        fs_file_close_win32(pDuplicate);
        return fs_result_from_GetLastError();
    }

    if ((info1.dwVolumeSerialNumber != info2.dwVolumeSerialNumber) || (info1.nFileIndexLow != info2.nFileIndexLow) || (info1.nFileIndexHigh != info2.nFileIndexHigh)) {
        fs_file_close_win32(pDuplicate);
        return FS_INVALID_OPERATION;
    }

    return FS_SUCCESS;
}


#define FS_WIN32_MIN_ITERATOR_ALLOCATION_SIZE 1024

typedef struct fs_iterator_win32
{
    fs_iterator iterator;
    HANDLE hFind;
    WIN32_FIND_DATAA findData;
    char* pFullFilePath;        /* Points to the end of the structure. */
    size_t directoryPathLen;    /* The length of the directory section. */
} fs_iterator_win32;

static void fs_free_iterator_win32(fs_iterator* pIterator);

static fs_iterator* fs_iterator_win32_resolve(fs_iterator_win32* pIteratorWin32, fs* pFS, HANDLE hFind, const WIN32_FIND_DATA* pFD)
{
    fs_iterator_win32* pNewIteratorWin32;
    size_t allocSize;
    int nameLenIncludingNullTerminator;

    /*
    The name is stored at the end of the struct. In order to know how much memory to allocate we'll
    need to calculate the length of the name.
    */
    #if defined(UNICODE) || defined(_UNICODE)
    {
        nameLenIncludingNullTerminator = WideCharToMultiByte(CP_UTF8, 0, pFD->cFileName, -1, NULL, 0, NULL, NULL);
        if (nameLenIncludingNullTerminator == 0) {
            fs_free_iterator_win32((fs_iterator*)pIteratorWin32);
            return NULL;
        }
    }
    #else
    {
        nameLenIncludingNullTerminator = (int)strlen(pFD->cFileName) + 1;  /* +1 for the null terminator. */
    }
    #endif

    allocSize = FS_MAX(sizeof(fs_iterator_win32) + nameLenIncludingNullTerminator, FS_WIN32_MIN_ITERATOR_ALLOCATION_SIZE);    /* 1KB just to try to avoid excessive internal reallocations inside realloc(). */

    pNewIteratorWin32 = (fs_iterator_win32*)fs_realloc(pIteratorWin32, allocSize, fs_get_allocation_callbacks(pFS));
    if (pNewIteratorWin32 == NULL) {
        fs_free_iterator_win32((fs_iterator*)pIteratorWin32);
        return NULL;
    }

    pNewIteratorWin32->iterator.pFS = pFS;
    pNewIteratorWin32->hFind        = hFind;

    /* Name. */
    pNewIteratorWin32->iterator.pName   = (char*)pNewIteratorWin32 + sizeof(fs_iterator_win32);
    pNewIteratorWin32->iterator.nameLen = (size_t)nameLenIncludingNullTerminator - 1;

    #if defined(UNICODE) || defined(_UNICODE)
    {
        WideCharToMultiByte(CP_UTF8, 0, pFD->cFileName, -1, (char*)pNewIteratorWin32->iterator.pName, nameLenIncludingNullTerminator, NULL, NULL);  /* const-cast is safe here. */
    }
    #else
    {
        fs_strcpy((char*)pNewIteratorWin32->iterator.pName, pFD->cFileName);  /* const-cast is safe here. */
    }
    #endif

    /* Info. */
    pNewIteratorWin32->iterator.info = fs_file_info_from_WIN32_FIND_DATA(pFD);

    return (fs_iterator*)pNewIteratorWin32;
}

static fs_iterator* fs_first_win32(fs* pFS, const char* pDirectoryPath, size_t directoryPathLen)
{
    HANDLE hFind;
    WIN32_FIND_DATA fd;
    fs_result result;
    fs_win32_path query;

    /* An empty path means the current directory. Win32 will want us to specify "." in this case. */
    if (pDirectoryPath == NULL || pDirectoryPath[0] == '\0') {
        pDirectoryPath = ".";
        directoryPathLen = 1;
    }

    result = fs_win32_path_init(&query, pDirectoryPath, directoryPathLen, fs_get_allocation_callbacks(pFS));
    if (result != FS_SUCCESS) {
        return NULL;
    }

    /*
    At this point we have converted the first part of the query. Now we need to append "\*" to it. To do this
    properly, we'll first need to remove any trailing slash, if any.
    */
    if (query.len > 0 && query.path[query.len - 1] == '\\') {
        query.len -= 1;
        query.path[query.len] = '\0';
    }

    result = fs_win32_path_append(&query, "\\*", fs_get_allocation_callbacks(pFS));
    if (result != FS_SUCCESS) {
        fs_win32_path_uninit(&query, fs_get_allocation_callbacks(pFS));
        return NULL;
    }

    hFind = FindFirstFile(query.path, &fd);
    fs_win32_path_uninit(&query, fs_get_allocation_callbacks(pFS));

    if (hFind == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    return fs_iterator_win32_resolve(NULL, pFS, hFind, &fd);
}

static fs_iterator* fs_next_win32(fs_iterator* pIterator)
{
    fs_iterator_win32* pIteratorWin32 = (fs_iterator_win32*)pIterator;
    WIN32_FIND_DATA fd;

    if (!FindNextFile(pIteratorWin32->hFind, &fd)) {
        fs_free_iterator_win32(pIterator);
        return NULL;
    }

    return fs_iterator_win32_resolve(pIteratorWin32, pIterator->pFS, pIteratorWin32->hFind, &fd);
}

static void fs_free_iterator_win32(fs_iterator* pIterator)
{
    fs_iterator_win32* pIteratorWin32 = (fs_iterator_win32*)pIterator;

    FindClose(pIteratorWin32->hFind);
    fs_free(pIteratorWin32, fs_get_allocation_callbacks(pIterator->pFS));
}

static fs_backend fs_win32_backend =
{
    fs_alloc_size_win32,
    fs_init_win32,
    fs_uninit_win32,
    fs_ioctl_win32,
    fs_remove_win32,
    fs_rename_win32,
    fs_mkdir_win32,
    fs_info_win32,
    fs_file_alloc_size_win32,
    fs_file_open_win32,
    fs_file_close_win32,
    fs_file_read_win32,
    fs_file_write_win32,
    fs_file_seek_win32,
    fs_file_tell_win32,
    fs_file_flush_win32,
    fs_file_truncate_win32,
    fs_file_info_win32,
    fs_file_duplicate_win32,
    fs_first_win32,
    fs_next_win32,
    fs_free_iterator_win32
};

const fs_backend* FS_BACKEND_WIN32 = &fs_win32_backend;
#else
const fs_backend* FS_BACKEND_WIN32 = NULL;
#endif

#endif  /* fs_win32_c */
