#ifndef fs_pak_c
#define fs_pak_c

#include "../../../fs.h"
#include "fs_pak.h"

#include <assert.h>
#include <string.h>

#ifndef FS_PAK_COPY_MEMORY
#define FS_PAK_COPY_MEMORY(dst, src, sz) memcpy((dst), (src), (sz))
#endif

#ifndef FS_PAK_ASSERT
#define FS_PAK_ASSERT(x) assert(x)
#endif

#define FS_PAK_ABS(x)   (((x) > 0) ? (x) : -(x))


/* BEG fs_pak.c */
static FS_INLINE fs_bool32 fs_pak_is_le(void)
{
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
    return FS_TRUE;
#elif defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && __BYTE_ORDER == __LITTLE_ENDIAN
    return FS_TRUE;
#else
    int n = 1;
    return (*(char*)&n) == 1;
#endif
}

static FS_INLINE unsigned int fs_pak_swap_endian_32(unsigned int n)
{
    return
        ((n & 0xFF000000) >> 24) |
        ((n & 0x00FF0000) >>  8) |
        ((n & 0x0000FF00) <<  8) |
        ((n & 0x000000FF) << 24);
}

static FS_INLINE unsigned int fs_pak_le2ne_32(unsigned int n)
{
    if (!fs_pak_is_le()) {
        return fs_pak_swap_endian_32(n);
    }

    return n;
}


typedef struct fs_pak_toc_entry
{
    char name[56];
    fs_uint32 offset;
    fs_uint32 size;
} fs_pak_toc_entry;

typedef struct fs_pak
{
    fs_uint32 fileCount;
    fs_pak_toc_entry* pTOC;
} fs_pak;


static size_t fs_alloc_size_pak(const void* pBackendConfig)
{
    (void)pBackendConfig;
    return sizeof(fs_pak);
}

static fs_result fs_init_pak(fs* pFS, const void* pBackendConfig, fs_stream* pStream)
{
    fs_pak* pPak;
    fs_result result;
    char fourcc[4];
    fs_uint32 tocOffset;
    fs_uint32 tocSize;

    /* No need for a backend config. */
    (void)pBackendConfig;

    if (pStream == NULL) {
        return FS_INVALID_OPERATION;    /* Most likely the FS is being opened without a stream. */
    }

    pPak = (fs_pak*)fs_get_backend_data(pFS);
    FS_PAK_ASSERT(pPak != NULL);

    result = fs_stream_read(pStream, fourcc, sizeof(fourcc), NULL);
    if (result != FS_SUCCESS) {
        return result;
    }

    if (fourcc[0] != 'P' || fourcc[1] != 'A' || fourcc[2] != 'C' || fourcc[3] != 'K') {
        return FS_INVALID_FILE;    /* Not a PAK file. */
    }

    result = fs_stream_read(pStream, &tocOffset, 4, NULL);
    if (result != FS_SUCCESS) {
        return result;
    }

    result = fs_stream_read(pStream, &tocSize, 4, NULL);
    if (result != FS_SUCCESS) {
        return result;
    }

    tocOffset = fs_pak_le2ne_32(tocOffset);
    tocSize   = fs_pak_le2ne_32(tocSize);

    /* Seek to the TOC so we can read it. */
    result = fs_stream_seek(pStream, tocOffset, FS_SEEK_SET);
    if (result != FS_SUCCESS) {
        return result;
    }

    pPak->pTOC = (fs_pak_toc_entry*)fs_malloc(tocSize, fs_get_allocation_callbacks(pFS));
    if (pPak->pTOC == NULL) {
        return FS_OUT_OF_MEMORY;
    }

    result = fs_stream_read(pStream, pPak->pTOC, tocSize, NULL);
    if (result != FS_SUCCESS) {
        fs_free(pPak->pTOC, fs_get_allocation_callbacks(pFS));
        pPak->pTOC = NULL;
        return result;
    }

    pPak->fileCount = tocSize / sizeof(fs_pak_toc_entry);

    /* Swap the endianness of the TOC. */
    if (!fs_pak_is_le()) {
        fs_uint32 i;
        for (i = 0; i < pPak->fileCount; i += 1) {
            pPak->pTOC[i].offset = fs_pak_swap_endian_32(pPak->pTOC[i].offset);
            pPak->pTOC[i].size   = fs_pak_swap_endian_32(pPak->pTOC[i].size);
        }
    }

    return FS_SUCCESS;
}

static void fs_uninit_pak(fs* pFS)
{
    fs_pak* pPak = (fs_pak*)fs_get_backend_data(pFS);
    FS_PAK_ASSERT(pPak != NULL);

    fs_free(pPak->pTOC, fs_get_allocation_callbacks(pFS));
    return;
}

static fs_result fs_info_pak(fs* pFS, const char* pPath, int openMode, fs_file_info* pInfo)
{
    fs_pak* pPak;
    fs_uint32 tocIndex;

    (void)openMode;
    
    pPak = (fs_pak*)fs_get_backend_data(pFS);
    FS_PAK_ASSERT(pPak != NULL);

    for (tocIndex = 0; tocIndex < pPak->fileCount; tocIndex += 1) {
        if (fs_path_compare(pPak->pTOC[tocIndex].name, FS_NULL_TERMINATED, pPath, FS_NULL_TERMINATED) == 0) {
            pInfo->size      = pPak->pTOC[tocIndex].size;
            pInfo->directory = 0;

            return FS_SUCCESS;
        }
    }

    /* Getting here means we couldn't find a file with the given path, but it might be a folder. */
    for (tocIndex = 0; tocIndex < pPak->fileCount; tocIndex += 1) {
        if (fs_path_begins_with(pPak->pTOC[tocIndex].name, FS_NULL_TERMINATED, pPath, FS_NULL_TERMINATED)) {
            pInfo->size      = 0;
            pInfo->directory = 1;

            return FS_SUCCESS;
        }
    }

    /* Getting here means neither a file nor folder was found. */
    return FS_DOES_NOT_EXIST;
}


typedef struct fs_file_pak
{
    fs_stream* pStream;
    fs_uint32 tocIndex;
    fs_uint32 cursor;
} fs_file_pak;

static size_t fs_file_alloc_size_pak(fs* pFS)
{
    (void)pFS;
    return sizeof(fs_file_pak);
}

static fs_result fs_file_open_pak(fs* pFS, fs_stream* pStream, const char* pPath, int openMode, fs_file* pFile)
{
    fs_pak* pPak;
    fs_file_pak* pPakFile;
    fs_uint32 tocIndex;
    fs_result result;

    pPak = (fs_pak*)fs_get_backend_data(pFS);
    FS_PAK_ASSERT(pPak != NULL);

    pPakFile = (fs_file_pak*)fs_file_get_backend_data(pFile);
    FS_PAK_ASSERT(pPakFile != NULL);

    /* Write mode is currently unsupported. */
    if ((openMode & FS_WRITE) != 0) {
        return FS_INVALID_OPERATION;
    }

    for (tocIndex = 0; tocIndex < pPak->fileCount; tocIndex += 1) {
        if (fs_path_compare(pPak->pTOC[tocIndex].name, FS_NULL_TERMINATED, pPath, FS_NULL_TERMINATED) == 0) {
            pPakFile->tocIndex = tocIndex;
            pPakFile->cursor   = 0;
            pPakFile->pStream  = pStream;

            result = fs_stream_seek(pPakFile->pStream, pPak->pTOC[tocIndex].offset, FS_SEEK_SET);
            if (result != FS_SUCCESS) {
                return FS_INVALID_FILE;    /* Failed to seek. Archive is probably corrupt. */
            }

            return FS_SUCCESS;
        }
    }

    /* Getting here means the file was not found. */
    return FS_DOES_NOT_EXIST;
}

static void fs_file_close_pak(fs_file* pFile)
{
    /* Nothing to do. */
    (void)pFile;
}

static fs_result fs_file_read_pak(fs_file* pFile, void* pDst, size_t bytesToRead, size_t* pBytesRead)
{
    fs_file_pak* pPakFile;
    fs_pak* pPak;
    fs_result result;
    fs_uint32 bytesRemainingInFile;

    pPakFile = (fs_file_pak*)fs_file_get_backend_data(pFile);
    FS_PAK_ASSERT(pPakFile != NULL);

    pPak = (fs_pak*)fs_get_backend_data(fs_file_get_fs(pFile));
    FS_PAK_ASSERT(pPak != NULL);

    bytesRemainingInFile = pPak->pTOC[pPakFile->tocIndex].size - pPakFile->cursor;
    if (bytesRemainingInFile == 0) {
        return FS_AT_END;   /* No more bytes remaining. Must return AT_END. */
    }

    if (bytesToRead > bytesRemainingInFile) {
        bytesToRead = bytesRemainingInFile;
    }

    result = fs_stream_read(pPakFile->pStream, pDst, bytesToRead, pBytesRead);
    if (result != FS_SUCCESS) {
        return result;
    }

    pPakFile->cursor += (fs_uint32)*pBytesRead;
    FS_PAK_ASSERT(pPakFile->cursor <= pPak->pTOC[pPakFile->tocIndex].size);

    return FS_SUCCESS;
}

static fs_result fs_file_seek_pak(fs_file* pFile, fs_int64 offset, fs_seek_origin origin)
{
    fs_file_pak* pPakFile;
    fs_pak* pPak;
    fs_result result;
    fs_int64 newCursor;

    pPakFile = (fs_file_pak*)fs_file_get_backend_data(pFile);
    FS_PAK_ASSERT(pPakFile != NULL);

    pPak = (fs_pak*)fs_get_backend_data(fs_file_get_fs(pFile));
    FS_PAK_ASSERT(pPak != NULL);

    if (FS_PAK_ABS(offset) > 0xFFFFFFFF) {
        return FS_BAD_SEEK;    /* Offset is too large. */
    }

    if (origin == FS_SEEK_SET) {
        newCursor = 0;
    } else if (origin == FS_SEEK_CUR) {
        newCursor = pPakFile->cursor;
    } else if (origin == FS_SEEK_END) {
        newCursor = pPak->pTOC[pPakFile->tocIndex].size;
    } else {
        FS_PAK_ASSERT(!"Invalid seek origin.");
        return FS_INVALID_ARGS;
    }

    newCursor += offset;
    if (newCursor < 0) {
        return FS_BAD_SEEK;    /* Negative offset. */
    }
    if (newCursor > pPak->pTOC[pPakFile->tocIndex].size) {
        return FS_BAD_SEEK;    /* Offset is larger than file size. */
    }

    result = fs_stream_seek(pPakFile->pStream, newCursor, FS_SEEK_SET);
    if (result != FS_SUCCESS) {
        return result;
    }

    pPakFile->cursor = (fs_uint32)newCursor;    /* Safe cast. */

    return FS_SUCCESS;
}

static fs_result fs_file_tell_pak(fs_file* pFile, fs_int64* pCursor)
{
    fs_file_pak* pPakFile;

    pPakFile = (fs_file_pak*)fs_file_get_backend_data(pFile);
    FS_PAK_ASSERT(pPakFile != NULL);

    *pCursor = pPakFile->cursor;

    return FS_SUCCESS;
}

static fs_result fs_file_flush_pak(fs_file* pFile)
{
    /* Nothing to do. */
    (void)pFile;
    return FS_SUCCESS;
}

static fs_result fs_file_info_pak(fs_file* pFile, fs_file_info* pInfo)
{
    fs_file_pak* pPakFile;
    fs_pak* pPak;

    pPakFile = (fs_file_pak*)fs_file_get_backend_data(pFile);
    FS_PAK_ASSERT(pPakFile != NULL);

    pPak = (fs_pak*)fs_get_backend_data(fs_file_get_fs(pFile));
    FS_PAK_ASSERT(pPak != NULL);

    FS_PAK_ASSERT(pInfo != NULL);
    pInfo->size      = pPak->pTOC[pPakFile->tocIndex].size;
    pInfo->directory = FS_FALSE; /* An opened file should never be a directory. */
    
    return FS_SUCCESS;
}

static fs_result fs_file_duplicate_pak(fs_file* pFile, fs_file* pDuplicatedFile)
{
    fs_file_pak* pPakFile;
    fs_file_pak* pDuplicatedPakFile;

    pPakFile = (fs_file_pak*)fs_file_get_backend_data(pFile);
    FS_PAK_ASSERT(pPakFile != NULL);

    pDuplicatedPakFile = (fs_file_pak*)fs_file_get_backend_data(pDuplicatedFile);
    FS_PAK_ASSERT(pDuplicatedPakFile != NULL);

    /* We should be able to do this with a simple memcpy. */
    FS_PAK_COPY_MEMORY(pDuplicatedPakFile, pPakFile, fs_file_alloc_size_pak(fs_file_get_fs(pFile)));

    return FS_SUCCESS;
}


typedef struct fs_iterator_pak
{
    fs_iterator base;
    fs_uint32 index;    /* The index of the current item. */
    fs_uint32 count;    /* The numebr of entries in items. */
    struct
    {
        char name[56];
        fs_uint32 tocIndex; /* Will be set to 0xFFFFFFFF for directories.  */
    } items[1];
} fs_iterator_pak;

static fs_bool32 fs_iterator_item_exists_pak(fs_iterator_pak* pIteratorPak, const char* pName, size_t nameLen)
{
    fs_uint32 i;

    for (i = 0; i < pIteratorPak->count; i += 1) {
        if (fs_strncmp(pIteratorPak->items[i].name, pName, nameLen) == 0) {
            return FS_TRUE;
        }
    }

    return FS_FALSE;
}

static void fs_iterator_resolve_pak(fs_iterator_pak* pIteratorPak)
{
    fs_pak* pPak;

    pPak = (fs_pak*)fs_get_backend_data(pIteratorPak->base.pFS);
    FS_PAK_ASSERT(pPak != NULL);

    pIteratorPak->base.pName = pIteratorPak->items[pIteratorPak->index].name;
    pIteratorPak->base.nameLen = strlen(pIteratorPak->base.pName);

    memset(&pIteratorPak->base.info, 0, sizeof(fs_file_info));
    if (pIteratorPak->items[pIteratorPak->index].tocIndex == 0xFFFFFFFF) {
        pIteratorPak->base.info.directory = FS_TRUE;
        pIteratorPak->base.info.size = 0;
    } else {
        pIteratorPak->base.info.directory = FS_FALSE;
        pIteratorPak->base.info.size = pPak->pTOC[pIteratorPak->items[pIteratorPak->index].tocIndex].size;
    }
}

FS_API fs_iterator* fs_first_pak(fs* pFS, const char* pDirectoryPath, size_t directoryPathLen)
{
    /*
    PAK files only list files. They do not include any explicit directory entries. We'll therefore need
    to derive folders from the list of file paths. This means we'll need to accumulate the list of
    entries in this functions.
    */
    fs_pak* pPak;
    fs_iterator_pak* pIteratorPak;
    fs_uint32 tocIndex;
    fs_uint32 itemCap = 16;

    pPak = (fs_pak*)fs_get_backend_data(pFS);
    FS_PAK_ASSERT(pPak != NULL);

    pIteratorPak = (fs_iterator_pak*)fs_calloc(sizeof(fs_iterator_pak) + (itemCap - 1) * sizeof(pIteratorPak->items[0]), fs_get_allocation_callbacks(pFS));
    if (pIteratorPak == NULL) {
        return NULL;
    }

    pIteratorPak->base.pFS = pFS;
    pIteratorPak->index = 0;
    pIteratorPak->count = 0;

    /* Skip past "/" if it was specified. */
    if (directoryPathLen > 0) {
        if (pDirectoryPath[0] == '/') {
            pDirectoryPath += 1;
            if (directoryPathLen != FS_NULL_TERMINATED) {
                directoryPathLen -= 1;
            }
        }
    }

    for (tocIndex = 0; tocIndex < pPak->fileCount; tocIndex += 1) {
        const char* pPathTail = fs_path_trim_base(pPak->pTOC[tocIndex].name, FS_NULL_TERMINATED, pDirectoryPath, directoryPathLen);
        if (pPathTail != NULL) {
            /*
            The file is contained within the directory, but it might not necessarily be appropriate to
            add this entry. We need to look at the next segments. If there is only one segment, it means
            this is a file and we can add it straight to the list. Otherwise, if there is an additional
            segment it means it's a folder, in which case we'll need to ensure there are no duplicates.
            */
            fs_path_iterator iPathSegment;
            if (fs_path_first(pPathTail, FS_NULL_TERMINATED, &iPathSegment) == FS_SUCCESS) {
                /*
                It's a candidate. If this item is valid for this iteration, the name will be that of the
                first segment.
                */
                if (!fs_iterator_item_exists_pak(pIteratorPak, iPathSegment.pFullPath + iPathSegment.segmentOffset, iPathSegment.segmentLength)) {
                    if (pIteratorPak->count >= itemCap) {
                        fs_iterator_pak* pNewIterator;

                        itemCap *= 2;
                        pNewIterator = (fs_iterator_pak*)fs_realloc(pIteratorPak, sizeof(fs_iterator_pak) + (itemCap - 1) * sizeof(pIteratorPak->items[0]), fs_get_allocation_callbacks(pFS));
                        if (pNewIterator == NULL) {
                            fs_free(pIteratorPak, fs_get_allocation_callbacks(pFS));
                            return NULL;    /* Out of memory. */
                        }

                        pIteratorPak = pNewIterator;
                    }

                    FS_PAK_COPY_MEMORY(pIteratorPak->items[pIteratorPak->count].name, iPathSegment.pFullPath + iPathSegment.segmentOffset, iPathSegment.segmentLength);
                    pIteratorPak->items[pIteratorPak->count].name[iPathSegment.segmentLength] = '\0';

                    if (fs_path_is_last(&iPathSegment)) {
                        /* It's a file. */
                        pIteratorPak->items[pIteratorPak->count].tocIndex = tocIndex;
                    } else {
                        /* It's a directory. */
                        pIteratorPak->items[pIteratorPak->count].tocIndex = 0xFFFFFFFF;
                    }

                    pIteratorPak->count += 1;
                } else {
                    /* An item with the same name already exists. Skip. */
                }
            } else {
                /*
                pDirectoryPath is exactly equal to this item. Since PAK archives only explicitly list files
                and not directories, it means pDirectoryPath is actually a file. It is invalid to try iterating
                a file, so we need to abort.
                */
                return NULL;
            }
        } else {
            /* This file is not contained within the given directory. */
        }
    }

    if (pIteratorPak->count == 0) {
        fs_free(pIteratorPak, fs_get_allocation_callbacks(pFS));
        return NULL;
    }

    pIteratorPak->index = 0;
    fs_iterator_resolve_pak(pIteratorPak);

    return (fs_iterator*)pIteratorPak;
}

FS_API fs_iterator* fs_next_pak(fs_iterator* pIterator)
{
    fs_iterator_pak* pIteratorPak = (fs_iterator_pak*)pIterator;
    
    FS_PAK_ASSERT(pIteratorPak != NULL);

    if (pIteratorPak->index + 1 >= pIteratorPak->count) {
        fs_free(pIterator, fs_get_allocation_callbacks(pIteratorPak->base.pFS));
        return NULL;    /* No more items. */
    }

    pIteratorPak->index += 1;
    fs_iterator_resolve_pak(pIteratorPak);

    return (fs_iterator*)pIteratorPak;
}

FS_API void fs_free_iterator_pak(fs_iterator* pIterator)
{
    fs_iterator_pak* pIteratorPak = (fs_iterator_pak*)pIterator;
    FS_PAK_ASSERT(pIteratorPak != NULL);

    fs_free(pIteratorPak, fs_get_allocation_callbacks(pIteratorPak->base.pFS));
}


fs_backend fs_pak_backend =
{
    fs_alloc_size_pak,
    fs_init_pak,
    fs_uninit_pak,
    NULL,   /* ioctl */
    NULL,   /* remove */
    NULL,   /* rename */
    NULL,   /* mkdir */
    fs_info_pak,
    fs_file_alloc_size_pak,
    fs_file_open_pak,
    fs_file_close_pak,
    fs_file_read_pak,
    NULL,   /* write */
    fs_file_seek_pak,
    fs_file_tell_pak,
    fs_file_flush_pak,
    fs_file_info_pak,
    fs_file_duplicate_pak,
    fs_first_pak,
    fs_next_pak,
    fs_free_iterator_pak
};
const fs_backend* FS_PAK = &fs_pak_backend;
/* END fs_pak.c */

#endif  /* fs_pak_c */
