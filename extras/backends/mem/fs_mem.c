#ifndef fs_mem_c
#define fs_mem_c

#include "../../../fs.h"
#include "fs_mem.h"

#include <assert.h>
#include <string.h>
#include <time.h>

#ifndef FS_MEM_COPY_MEMORY
#define FS_MEM_COPY_MEMORY(dst, src, sz) memcpy((dst), (src), (sz))
#endif

#ifndef FS_MEM_MOVE_MEMORY
#define FS_MEM_MOVE_MEMORY(dst, src, sz) memmove((dst), (src), (sz))
#endif

#ifndef FS_MEM_ZERO_MEMORY
#define FS_MEM_ZERO_MEMORY(p, sz) memset((p), 0, (sz))
#endif

#ifndef FS_MEM_ASSERT
#define FS_MEM_ASSERT(x) assert(x)
#endif

#define FS_MEM_ZERO_OBJECT(p)           FS_MEM_ZERO_MEMORY((p), sizeof(*(p)))
#define FS_MEM_MAX(x, y)                (((x) > (y)) ? (x) : (y))
#define FS_MEM_MIN(x, y)                (((x) < (y)) ? (x) : (y))
#define FS_MEM_OFFSET_PTR(p, offset)    (((fs_uint8*)(p)) + (offset))


typedef enum fs_mem_node_type
{
    FS_MEM_NODE_TYPE_FILE = 0,
    FS_MEM_NODE_TYPE_DIRECTORY = 1
} fs_mem_node_type;


typedef struct fs_mem_node fs_mem_node;

typedef struct fs_mem_file_data
{
    void* pData;                    /* File content. */
    size_t size;                    /* Current file size. */
    size_t capacity;                /* Allocated capacity. */
    time_t creationTime;            /* File creation time. */
    time_t modificationTime;        /* Last modification time. */
} fs_mem_file_data;

typedef struct fs_mem_directory_data
{
    fs_mem_node** ppChildren;       /* Array of child nodes. */
    size_t childCount;              /* Number of children. */
    size_t childCapacity;           /* Capacity of children array. */
    time_t creationTime;            /* Directory creation time. */
    time_t modificationTime;        /* Last modification time. */
} fs_mem_directory_data;

struct fs_mem_node
{
    char* pName;                    /* Node name (null-terminated). */
    size_t nameLen;                 /* Length of name. */
    fs_mem_node_type type;          /* Node type (file or directory). */
    fs_mem_node* pParent;           /* Parent directory. */
    union
    {
        fs_mem_file_data file;      /* File-specific data. */
        fs_mem_directory_data dir;  /* Directory-specific data. */
    } data;
};


typedef struct fs_mem
{
    fs_mem_node* pRoot;             /* Root directory node. */
    fs_mtx lock;
} fs_mem;

typedef struct fs_file_mem
{
    fs_mem_node* pNode;             /* Associated file node. */
    fs_uint64 cursor;               /* Current file position. */
    int openMode;                   /* File open mode. */
} fs_file_mem;

typedef struct fs_iterator_mem
{
    fs_iterator base;               /* Base iterator structure. */
    fs_mem_node* pDirectory;        /* Directory being iterated. */
    size_t currentIndex;            /* Current child index. */
} fs_iterator_mem;



static void fs_mem_lock(fs_mem* pMem)
{
    FS_MEM_ASSERT(pMem != NULL);
    fs_mtx_lock(&pMem->lock);
}

static void fs_mem_unlock(fs_mem* pMem)
{
    FS_MEM_ASSERT(pMem != NULL);
    fs_mtx_unlock(&pMem->lock);
}


static char* fs_mem_strdup(const char* pStr, size_t len, const fs_allocation_callbacks* pAllocationCallbacks)
{
    char* pDuplicate;
    
    if (len == FS_NULL_TERMINATED) {
        len = strlen(pStr);
    }
    
    pDuplicate = (char*)fs_malloc(len + 1, pAllocationCallbacks);
    if (pDuplicate == NULL) {
        return NULL;
    }
    
    FS_MEM_COPY_MEMORY(pDuplicate, pStr, len);
    pDuplicate[len] = '\0';
    
    return pDuplicate;
}

static fs_mem_node* fs_mem_node_create(const char* pName, size_t nameLen, fs_mem_node_type type, const fs_allocation_callbacks* pAllocationCallbacks)
{
    fs_mem_node* pNode;
    time_t currentTime;
    
    pNode = (fs_mem_node*)fs_malloc(sizeof(fs_mem_node), pAllocationCallbacks);
    if (pNode == NULL) {
        return NULL;
    }
    
    FS_MEM_ZERO_OBJECT(pNode);
    
    pNode->pName = fs_mem_strdup(pName, nameLen, pAllocationCallbacks);
    if (pNode->pName == NULL) {
        fs_free(pNode, pAllocationCallbacks);
        return NULL;
    }
    
    if (nameLen == FS_NULL_TERMINATED) {
        nameLen = strlen(pName);
    }
    
    pNode->nameLen = nameLen;
    pNode->type = type;
    
    currentTime = time(NULL);
    
    if (type == FS_MEM_NODE_TYPE_FILE) {
        pNode->data.file.pData            = NULL;
        pNode->data.file.size             = 0;
        pNode->data.file.capacity         = 0;
        pNode->data.file.creationTime     = currentTime;
        pNode->data.file.modificationTime = currentTime;
    } else {
        pNode->data.dir.ppChildren        = NULL;
        pNode->data.dir.childCount        = 0;
        pNode->data.dir.childCapacity     = 0;
        pNode->data.dir.creationTime      = currentTime;
        pNode->data.dir.modificationTime  = currentTime;
    }
    
    return pNode;
}

static void fs_mem_node_destroy(fs_mem_node* pNode, const fs_allocation_callbacks* pAllocationCallbacks)
{
    if (pNode == NULL) {
        return;
    }
    
    if (pNode->type == FS_MEM_NODE_TYPE_FILE) {
        if (pNode->data.file.pData != NULL) {
            fs_free(pNode->data.file.pData, pAllocationCallbacks);
        }
    } else {
        /* Destroy all children first. */
        size_t i;
        for (i = 0; i < pNode->data.dir.childCount; i += 1) {
            fs_mem_node_destroy(pNode->data.dir.ppChildren[i], pAllocationCallbacks);
        }
        
        if (pNode->data.dir.ppChildren != NULL) {
            fs_free(pNode->data.dir.ppChildren, pAllocationCallbacks);
        }
    }
    
    if (pNode->pName != NULL) {
        fs_free(pNode->pName, pAllocationCallbacks);
    }
    
    fs_free(pNode, pAllocationCallbacks);
}

static fs_result fs_mem_directory_add_child(fs_mem_node* pDirectory, fs_mem_node* pChild, const fs_allocation_callbacks* pAllocationCallbacks)
{
    FS_MEM_ASSERT(pDirectory != NULL);
    FS_MEM_ASSERT(pDirectory->type == FS_MEM_NODE_TYPE_DIRECTORY);
    FS_MEM_ASSERT(pChild != NULL);
    
    /* Check if we need to resize the children array. */
    if (pDirectory->data.dir.childCount >= pDirectory->data.dir.childCapacity) {
        size_t newCapacity = FS_MEM_MAX(pDirectory->data.dir.childCapacity * 2, 4);
        fs_mem_node** ppNewChildren = (fs_mem_node**)fs_realloc(pDirectory->data.dir.ppChildren, newCapacity * sizeof(fs_mem_node*), pAllocationCallbacks);
        if (ppNewChildren == NULL) {
            return FS_OUT_OF_MEMORY;
        }
        
        pDirectory->data.dir.ppChildren = ppNewChildren;
        pDirectory->data.dir.childCapacity = newCapacity;
    }
    
    pDirectory->data.dir.ppChildren[pDirectory->data.dir.childCount] = pChild;
    pDirectory->data.dir.childCount += 1;
    pDirectory->data.dir.modificationTime = time(NULL);

    pChild->pParent = pDirectory;
    
    return FS_SUCCESS;
}

static fs_result fs_mem_directory_remove_child(fs_mem_node* pDirectory, fs_mem_node* pChild, const fs_allocation_callbacks* pAllocationCallbacks)
{
    size_t i;
    
    FS_MEM_ASSERT(pDirectory != NULL);
    FS_MEM_ASSERT(pDirectory->type == FS_MEM_NODE_TYPE_DIRECTORY);
    FS_MEM_ASSERT(pChild != NULL);
    
    (void)pAllocationCallbacks; /* Not used in this function. */
    
    /* Find the child. */
    for (i = 0; i < pDirectory->data.dir.childCount; i += 1) {
        if (pDirectory->data.dir.ppChildren[i] == pChild) {
            /* Found. Move remaining children down to overwrite the removed child. */
            size_t j;
            for (j = i + 1; j < pDirectory->data.dir.childCount; j += 1) {
                pDirectory->data.dir.ppChildren[j - 1] = pDirectory->data.dir.ppChildren[j];
            }

            pChild->pParent = NULL;
            
            pDirectory->data.dir.childCount -= 1;
            pDirectory->data.dir.modificationTime = time(NULL);
            
            return FS_SUCCESS;
        }
    }
    
    return FS_DOES_NOT_EXIST;
}

static fs_mem_node* fs_mem_directory_find_child(fs_mem_node* pDirectory, const char* pName, size_t nameLen)
{
    size_t i;
    
    FS_MEM_ASSERT(pDirectory != NULL);
    FS_MEM_ASSERT(pDirectory->type == FS_MEM_NODE_TYPE_DIRECTORY);
    
    if (nameLen == FS_NULL_TERMINATED) {
        nameLen = strlen(pName);
    }
    
    for (i = 0; i < pDirectory->data.dir.childCount; i += 1) {
        fs_mem_node* pChild = pDirectory->data.dir.ppChildren[i];
        FS_MEM_ASSERT(pChild != NULL);

        if (pChild->nameLen == nameLen && memcmp(pChild->pName, pName, nameLen) == 0) {
            return pChild;
        }
    }
    
    return NULL;
}

static fs_result fs_mem_resolve_path(fs* pFS, const char* pPath, size_t pathLen, fs_mem_node** ppNode, fs_mem_node** ppParent, char** ppLastSegment)
{
    fs_result result;
    fs_mem* pMem = (fs_mem*)fs_get_backend_data(pFS);
    char pNormalizedPathStack[256];
    char* pNormalizedPathHeap = NULL;
    char* pNormalizedPath;
    int normalizedLen;
    fs_mem_node* pCurrent;
    fs_path_iterator iPath;

    if (ppParent != NULL) {
        *ppParent = NULL;
    }

    if (ppLastSegment != NULL) {
        *ppLastSegment = NULL;
    }

    if (ppNode == NULL) {
        return FS_INVALID_ARGS;
    }

    *ppNode = NULL;
    
    if (pMem == NULL || pPath == NULL) {
        return FS_INVALID_ARGS;
    }
    
    /* Try normalizing directly to the stack buffer first. */
    normalizedLen = fs_path_normalize(pNormalizedPathStack, sizeof(pNormalizedPathStack), pPath, pathLen, 0);
    if (normalizedLen < 0) {
        return FS_INVALID_ARGS;
    }
    
    if ((size_t)(normalizedLen + 1) <= sizeof(pNormalizedPathStack)) {
        pNormalizedPath = pNormalizedPathStack;
    } else {
        pNormalizedPathHeap = (char*)fs_malloc(normalizedLen + 1, fs_get_allocation_callbacks(pFS));
        if (pNormalizedPathHeap == NULL) {
            return FS_OUT_OF_MEMORY;
        }
        
        fs_path_normalize(pNormalizedPathHeap, normalizedLen + 1, pPath, pathLen, 0);

        pNormalizedPath = pNormalizedPathHeap;
    }
    
    /* Empty path refers to root. */
    if (normalizedLen == 0) {
        *ppNode = pMem->pRoot;
        fs_free(pNormalizedPathHeap, fs_get_allocation_callbacks(pFS));
        return FS_SUCCESS;
    }
    
    pCurrent = pMem->pRoot;
    

    /* Traverse the path. */
    result = fs_path_first(pNormalizedPath, (size_t)normalizedLen, &iPath);
    if (result != FS_SUCCESS) {
        fs_free(pNormalizedPathHeap, fs_get_allocation_callbacks(pFS));
        return result;
    }
    
    for (;;) {
        const char* pComponent = iPath.pFullPath + iPath.segmentOffset;
        size_t componentLen = iPath.segmentLength;
        fs_mem_node* pNext;
        
        /* Skip empty segments (which occur with leading "/" paths). */
        if (componentLen == 0) {
            result = fs_path_next(&iPath);
            if (result != FS_SUCCESS) {
                /* Reached end with empty segment - this means we're at root. */
                *ppNode = pCurrent;

                if (ppParent != NULL) {
                    *ppParent = pCurrent->pParent;
                }

                fs_free(pNormalizedPathHeap, fs_get_allocation_callbacks(pFS));
                return FS_SUCCESS;
            }
            
            continue;
        }
        
        /* Find the child with the name of this component. */
        pNext = fs_mem_directory_find_child(pCurrent, pComponent, componentLen);
        
        /* If it's the last component we can terminate the traversal. */
        if (fs_path_is_last(&iPath)) {
            /* Set parent to the current directory (parent of the final component). */
            if (ppParent != NULL) {
                *ppParent = pCurrent;
            }

            if (ppLastSegment != NULL) {
                char* pLastSegment = fs_mem_strdup(pComponent, componentLen, fs_get_allocation_callbacks(pFS));
                if (pLastSegment == NULL) {
                    fs_free(pNormalizedPathHeap, fs_get_allocation_callbacks(pFS));
                    return FS_OUT_OF_MEMORY;
                }

                *ppLastSegment = pLastSegment;
            }

            *ppNode = pNext; /* May be NULL if not found. */

            fs_free(pNormalizedPathHeap, fs_get_allocation_callbacks(pFS));

            if (pNext != NULL) {
                return FS_SUCCESS;
            } else {
                return FS_DOES_NOT_EXIST;
            }
        }
        
        /* Not the final component - it must exist and be a directory. */
        if (pNext == NULL) {
            *ppNode = NULL;
            fs_free(pNormalizedPathHeap, fs_get_allocation_callbacks(pFS));
            return FS_DOES_NOT_EXIST;
        }

        /* Not the final component. It must be a directory. */
        if (pNext->type != FS_MEM_NODE_TYPE_DIRECTORY) {
            *ppNode = NULL;
            fs_free(pNormalizedPathHeap, fs_get_allocation_callbacks(pFS));
            return FS_NOT_DIRECTORY;
        }
        
        pCurrent = pNext;
        
        result = fs_path_next(&iPath);
        if (result != FS_SUCCESS) {
            /* We should actually never get here because the last segment should have been caught in the fs_path_is_last() check just above. */
            break;
        }
    }
    
    /* We should never get here. */
    FS_MEM_ASSERT(!"Unreachable code reached in fs_mem_resolve_path");

    *ppNode = pCurrent;
    if (ppParent != NULL) {
        *ppParent = pCurrent->pParent;
    }
    
    fs_free(pNormalizedPathHeap, fs_get_allocation_callbacks(pFS));
    return FS_SUCCESS;
}



static size_t fs_alloc_size_mem(const void* pBackendConfig)
{
    (void)pBackendConfig;
    return sizeof(fs_mem);
}

static fs_result fs_init_mem(fs* pFS, const void* pBackendConfig, fs_stream* pStream)
{
    fs_mem* pMem;
    
    /* Stream is not used for in-memory backend. */
    (void)pStream;
    (void)pBackendConfig; /* Config not used yet. */
    
    pMem = (fs_mem*)fs_get_backend_data(pFS);
    FS_MEM_ASSERT(pMem != NULL);
    
    FS_MEM_ZERO_OBJECT(pMem);
    
    fs_mtx_init(&pMem->lock, fs_mtx_recursive);
    
    /* Create root directory. */
    pMem->pRoot = fs_mem_node_create("", 0, FS_MEM_NODE_TYPE_DIRECTORY, fs_get_allocation_callbacks(pFS));
    if (pMem->pRoot == NULL) {
        return FS_OUT_OF_MEMORY;
    }
    
    return FS_SUCCESS;
}

static void fs_uninit_mem(fs* pFS)
{
    fs_mem* pMem = (fs_mem*)fs_get_backend_data(pFS);
    FS_MEM_ASSERT(pMem != NULL);
    
    if (pMem->pRoot != NULL) {
        fs_mem_node_destroy(pMem->pRoot, fs_get_allocation_callbacks(pFS));
    }
    
    fs_mtx_destroy(&pMem->lock);
}

static fs_result fs_remove_mem_nolock(fs* pFS, const char* pFilePath)
{
    fs_mem_node* pNode;
    fs_mem_node* pParent;
    fs_result result;
    
    result = fs_mem_resolve_path(pFS, pFilePath, FS_NULL_TERMINATED, &pNode, &pParent, NULL);
    if (result != FS_SUCCESS) {
        return result;
    }
    
    if (pNode == NULL) {
        return FS_DOES_NOT_EXIST;
    }
    
    /* Cannot remove root directory. */
    if (pParent == NULL) {
        return FS_ACCESS_DENIED;
    }
    
    /* Check if directory is empty. */
    if (pNode->type == FS_MEM_NODE_TYPE_DIRECTORY && pNode->data.dir.childCount > 0) {
        return FS_DIRECTORY_NOT_EMPTY;
    }
    
    /* Remove from parent. */
    result = fs_mem_directory_remove_child(pParent, pNode, fs_get_allocation_callbacks(pFS));
    if (result != FS_SUCCESS) {
        return result;
    }
    
    /* Destroy the node. */
    fs_mem_node_destroy(pNode, fs_get_allocation_callbacks(pFS));
    
    return FS_SUCCESS;
}

static fs_result fs_remove_mem(fs* pFS, const char* pFilePath)
{
    fs_mem* pMem;
    fs_result result;
    
    pMem = (fs_mem*)fs_get_backend_data(pFS);
    FS_MEM_ASSERT(pMem != NULL);
    
    fs_mem_lock(pMem);
    {
        result = fs_remove_mem_nolock(pFS, pFilePath);
    }
    fs_mem_unlock(pMem);
    
    return result;
}

static fs_result fs_rename_mem_nolock(fs* pFS, const char* pOldPath, const char* pNewPath)
{
    fs_mem_node* pOldNode;
    fs_mem_node* pOldParent;
    fs_mem_node* pNewParent;
    fs_mem_node* pExistingNode;
    char* pNewName = NULL;
    fs_result result;
    
    /* Find the old node. */
    result = fs_mem_resolve_path(pFS, pOldPath, FS_NULL_TERMINATED, &pOldNode, &pOldParent, NULL);
    if (result != FS_SUCCESS || pOldNode == NULL) {
        return FS_DOES_NOT_EXIST;
    }
    
    /* Cannot rename root. */
    if (pOldParent == NULL) {
        return FS_ACCESS_DENIED;
    }
    
    /* Find the new parent and name. */
    result = fs_mem_resolve_path(pFS, pNewPath, FS_NULL_TERMINATED, &pExistingNode, &pNewParent, &pNewName);
    if (result != FS_SUCCESS && result != FS_DOES_NOT_EXIST) {
        return result;
    }
    
    if (pNewParent == NULL || pNewParent->type != FS_MEM_NODE_TYPE_DIRECTORY) {
        return FS_NOT_DIRECTORY;
    }
    
    /* Check if target already exists. */
    if (pExistingNode != NULL) {
        fs_free(pNewName, fs_get_allocation_callbacks(pFS));
        return FS_ALREADY_EXISTS;
    }

    FS_MEM_ASSERT(pNewName != NULL);
    
    /* Remove from old parent. */
    result = fs_mem_directory_remove_child(pOldParent, pOldNode, fs_get_allocation_callbacks(pFS));
    if (result != FS_SUCCESS) {
        fs_free(pNewName, fs_get_allocation_callbacks(pFS));
        return result;
    }
    
    /* Add to new parent. */
    result = fs_mem_directory_add_child(pNewParent, pOldNode, fs_get_allocation_callbacks(pFS));
    if (result != FS_SUCCESS) {
        /* Failed. Try to re-add to old parent to avoid losing the node. */
        fs_mem_directory_add_child(pOldParent, pOldNode, fs_get_allocation_callbacks(pFS));
        fs_free(pNewName, fs_get_allocation_callbacks(pFS));
        return result;
    }

    /* Update the node's name. */
    fs_free(pOldNode->pName, fs_get_allocation_callbacks(pFS));
    pOldNode->pName = pNewName;
    pOldNode->nameLen = strlen(pNewName);
    
    return FS_SUCCESS;
}

static fs_result fs_rename_mem(fs* pFS, const char* pOldPath, const char* pNewPath)
{
    fs_mem* pMem;
    fs_result result;
    
    pMem = (fs_mem*)fs_get_backend_data(pFS);
    FS_MEM_ASSERT(pMem != NULL);

    fs_mem_lock(pMem);
    {
        result = fs_rename_mem_nolock(pFS, pOldPath, pNewPath);
    }
    fs_mem_unlock(pMem);
    
    return result;
}

static fs_result fs_mkdir_mem_nolock(fs* pFS, const char* pPath)
{
    fs_mem_node* pNode;
    fs_mem_node* pParent;
    fs_mem_node* pNewDir;
    char* pName = NULL;
    fs_result result;
    
    /* First check if the directory already exists. */
    result = fs_mem_resolve_path(pFS, pPath, FS_NULL_TERMINATED, &pNode, &pParent, &pName);
    if (result == FS_SUCCESS && pNode != NULL) {
        fs_free(pName, fs_get_allocation_callbacks(pFS));
        return FS_ALREADY_EXISTS;
    }
    
    if (pParent == NULL || pParent->type != FS_MEM_NODE_TYPE_DIRECTORY) {
        fs_free(pName, fs_get_allocation_callbacks(pFS));
        return FS_DOES_NOT_EXIST; /* Parent directory doesn't exist. */
    }
    
    FS_MEM_ASSERT(pName != NULL);
    
    pNewDir = fs_mem_node_create(pName, FS_NULL_TERMINATED, FS_MEM_NODE_TYPE_DIRECTORY, fs_get_allocation_callbacks(pFS));
    if (pNewDir == NULL) {
        fs_free(pName, fs_get_allocation_callbacks(pFS));
        return FS_OUT_OF_MEMORY;
    }
    
    result = fs_mem_directory_add_child(pParent, pNewDir, fs_get_allocation_callbacks(pFS));
    if (result != FS_SUCCESS) {
        fs_mem_node_destroy(pNewDir, fs_get_allocation_callbacks(pFS));
        fs_free(pName, fs_get_allocation_callbacks(pFS));
        return result;
    }
    
    fs_free(pName, fs_get_allocation_callbacks(pFS));
    return FS_SUCCESS;
}

static fs_result fs_mkdir_mem(fs* pFS, const char* pPath)
{
    fs_mem* pMem;
    fs_result result;
    
    pMem = (fs_mem*)fs_get_backend_data(pFS);
    FS_MEM_ASSERT(pMem != NULL);
    
    fs_mem_lock(pMem);
    {
        result = fs_mkdir_mem_nolock(pFS, pPath);
    }
    fs_mem_unlock(pMem);
    
    return result;
}

static fs_result fs_info_mem_nolock(fs* pFS, const char* pPath, int openMode, fs_file_info* pInfo)
{
    fs_mem_node* pNode;
    fs_result result;
    
    (void)openMode;
    
    if (pInfo == NULL) {
        return FS_INVALID_ARGS;
    }
    
    result = fs_mem_resolve_path(pFS, pPath, FS_NULL_TERMINATED, &pNode, NULL, NULL);
    if (result != FS_SUCCESS || pNode == NULL) {
        return FS_DOES_NOT_EXIST;
    }
    
    FS_MEM_ZERO_OBJECT(pInfo);
    
    if (pNode->type == FS_MEM_NODE_TYPE_FILE) {
        pInfo->size = pNode->data.file.size;
        pInfo->directory = 0;
    } else {
        pInfo->size = 0;
        pInfo->directory = 1;
    }
    
    pInfo->symlink = 0;
    
    return FS_SUCCESS;
}

static fs_result fs_info_mem(fs* pFS, const char* pPath, int openMode, fs_file_info* pInfo)
{
    fs_mem* pMem;
    fs_result result;
    
    pMem = (fs_mem*)fs_get_backend_data(pFS);
    FS_MEM_ASSERT(pMem != NULL);
    
    fs_mem_lock(pMem);
    {
        result = fs_info_mem_nolock(pFS, pPath, openMode, pInfo);
    }
    fs_mem_unlock(pMem);
    
    return result;
}


static size_t fs_file_alloc_size_mem(fs* pFS)
{
    (void)pFS;
    return sizeof(fs_file_mem);
}

static fs_result fs_file_open_mem_nolock(fs* pFS, const char* pFilePath, int openMode, fs_file_mem* pFileMem)
{
    fs_result result;
    fs_mem_node* pNode;
    fs_mem_node* pParent;
    char* pName = NULL;

    /* Try to find existing file. */
    result = fs_mem_resolve_path(pFS, pFilePath, FS_NULL_TERMINATED, &pNode, &pParent, &pName);
    
    if (result == FS_SUCCESS && pNode != NULL) {
        /* File exists. */
        fs_free(pName, fs_get_allocation_callbacks(pFS));

        if (pNode->type != FS_MEM_NODE_TYPE_FILE) {
            return FS_IS_DIRECTORY;
        }
        
        /* Check for exclusive mode - should fail if file exists. */
        if ((openMode & FS_EXCLUSIVE) != 0) {
            return FS_ALREADY_EXISTS;
        }
        
        pFileMem->pNode = pNode;
        pFileMem->openMode = openMode;
        
        /* Position cursor based on open mode. */
        if ((openMode & FS_APPEND) != 0) {
            pFileMem->cursor = pNode->data.file.size;
        } else {
            pFileMem->cursor = 0;
        }
        
        /* Truncate if requested. */
        if (openMode & FS_TRUNCATE) {
            pNode->data.file.size = 0;
            pNode->data.file.modificationTime = time(NULL);
            pFileMem->cursor = 0;
        }
        
        return FS_SUCCESS;
    }
    
    /* If path resolution failed for reasons other than file not existing, return the error. */
    if (result != FS_SUCCESS && result != FS_DOES_NOT_EXIST) {
        fs_free(pName, fs_get_allocation_callbacks(pFS));
        return result;
    }
    
    /* Getting here means the file file does not exist. */
    if ((openMode & FS_WRITE) == 0) {
        /* Read-only mode. The file must exist in read-only mode, so we'll need to bomb out with an error. */
        fs_free(pName, fs_get_allocation_callbacks(pFS));
        return FS_DOES_NOT_EXIST;
    }
    
    /* Create new file. */
    if (pParent == NULL || pParent->type != FS_MEM_NODE_TYPE_DIRECTORY) {
        fs_free(pName, fs_get_allocation_callbacks(pFS));
        return FS_DOES_NOT_EXIST; /* Parent directory doesn't exist. */
    }
    
    FS_MEM_ASSERT(pName != NULL);
    
    /* Create new file node. */
    pNode = fs_mem_node_create(pName, FS_NULL_TERMINATED, FS_MEM_NODE_TYPE_FILE, fs_get_allocation_callbacks(pFS));
    if (pNode == NULL) {
        fs_free(pName, fs_get_allocation_callbacks(pFS));
        return FS_OUT_OF_MEMORY;
    }
    
    /* Add to parent directory. */
    result = fs_mem_directory_add_child(pParent, pNode, fs_get_allocation_callbacks(pFS));
    if (result != FS_SUCCESS) {
        fs_mem_node_destroy(pNode, fs_get_allocation_callbacks(pFS));
        fs_free(pName, fs_get_allocation_callbacks(pFS));
        return result;
    }
    
    pFileMem->pNode    = pNode;
    pFileMem->openMode = openMode;
    pFileMem->cursor   = 0;
    
    fs_free(pName, fs_get_allocation_callbacks(pFS));
    return FS_SUCCESS;
}

static fs_result fs_file_open_mem(fs* pFS, fs_stream* pStream, const char* pFilePath, int openMode, fs_file* pFile)
{
    fs_mem* pMem;
    fs_file_mem* pFileMem;
    fs_result result;
    
    /* Stream is not used for in-memory backend. */
    if (pStream != NULL) {
        return FS_INVALID_OPERATION;    /* Trying to open a file from a stream. */
    }
    
    pMem = (fs_mem*)fs_get_backend_data(pFS);
    FS_MEM_ASSERT(pMem != NULL);
    
    pFileMem = (fs_file_mem*)fs_file_get_backend_data(pFile);
    FS_MEM_ASSERT(pFileMem != NULL);
    
    FS_MEM_ZERO_OBJECT(pFileMem);
    
    fs_mem_lock(pMem);
    {
        result = fs_file_open_mem_nolock(pFS, pFilePath, openMode, pFileMem);
    }
    fs_mem_unlock(pMem);
    
    return result;
}

static void fs_file_close_mem(fs_file* pFile)
{
    fs_file_mem* pFileMem = (fs_file_mem*)fs_file_get_backend_data(pFile);
    FS_MEM_ASSERT(pFileMem != NULL);
    
    /* Nothing special needed for cleanup. */
    FS_MEM_ZERO_OBJECT(pFileMem);
}

static fs_result fs_file_read_mem_nolock(fs_file_mem* pFileMem, void* pDst, size_t bytesToRead, size_t* pBytesRead)
{
    fs_mem_node* pNode;
    size_t bytesAvailable;

    FS_MEM_ASSERT(pBytesRead != NULL);
    
    pNode = pFileMem->pNode;
    FS_MEM_ASSERT(pNode != NULL);
    FS_MEM_ASSERT(pNode->type == FS_MEM_NODE_TYPE_FILE);

    *pBytesRead = 0;
    
    if (pDst == NULL && bytesToRead > 0) {
        return FS_INVALID_ARGS;
    }
    
    /* Check if we're at or past the end of file. */
    if (pFileMem->cursor >= pNode->data.file.size) {
        return FS_AT_END;
    }
    
    /* Calculate how many bytes we can actually read. */
    bytesAvailable = pNode->data.file.size - (size_t)pFileMem->cursor;
    if (bytesToRead > bytesAvailable) {
        bytesToRead = bytesAvailable;
    }
    
    /* Copy the data. */
    if (bytesToRead > 0 && pNode->data.file.pData != NULL) {
        FS_MEM_COPY_MEMORY(pDst, FS_MEM_OFFSET_PTR(pNode->data.file.pData, pFileMem->cursor), bytesToRead);
    }
    
    pFileMem->cursor += bytesToRead;
    
    *pBytesRead = bytesToRead;
    return FS_SUCCESS;
}

static fs_result fs_file_read_mem(fs_file* pFile, void* pDst, size_t bytesToRead, size_t* pBytesRead)
{
    fs_file_mem* pFileMem;
    fs_mem* pMem;
    fs_result result;
    
    pFileMem = (fs_file_mem*)fs_file_get_backend_data(pFile);
    FS_MEM_ASSERT(pFileMem != NULL);
    
    pMem = (fs_mem*)fs_get_backend_data(fs_file_get_fs(pFile));
    
    fs_mem_lock(pMem);
    {
        result = fs_file_read_mem_nolock(pFileMem, pDst, bytesToRead, pBytesRead);
    }
    fs_mem_unlock(pMem);
    
    return result;
}

static fs_result fs_file_write_mem_nolock(fs_file_mem* pFileMem, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten, const fs_allocation_callbacks* pAllocationCallbacks)
{
    fs_mem_node* pNode;
    size_t newSize;

    FS_MEM_ASSERT(pBytesWritten != NULL);
    
    pNode = pFileMem->pNode;
    FS_MEM_ASSERT(pNode != NULL);
    FS_MEM_ASSERT(pNode->type == FS_MEM_NODE_TYPE_FILE);

    *pBytesWritten = 0;
    
    /* Check if writing is allowed. */
    if ((pFileMem->openMode & FS_WRITE) == 0) {
        return FS_ACCESS_DENIED;
    }
    
    if (pSrc == NULL && bytesToWrite > 0) {
        return FS_INVALID_ARGS;
    }
    
    if (bytesToWrite == 0) {
        return FS_SUCCESS;
    }
    
    /* Calculate the new file size after writing. */
    newSize = FS_MEM_MAX(pNode->data.file.size, (size_t)(pFileMem->cursor + bytesToWrite));
    
    /* Check if we need to expand the buffer. */
    if (newSize > pNode->data.file.capacity) {
        size_t newCapacity;
        void* pNewData;

        newCapacity = FS_MEM_MAX(newSize, pNode->data.file.capacity * 2);
        
        pNewData = fs_realloc(pNode->data.file.pData, newCapacity, pAllocationCallbacks);
        if (pNewData == NULL) {
            return FS_OUT_OF_MEMORY;
        }
        
        /* Zero out any gap between old size and cursor. */
        if (pFileMem->cursor > pNode->data.file.size) {
            FS_MEM_ZERO_MEMORY(FS_MEM_OFFSET_PTR(pNewData, pNode->data.file.size), (size_t)(pFileMem->cursor - pNode->data.file.size));
        }
        
        pNode->data.file.pData = pNewData;
        pNode->data.file.capacity = newCapacity;
    } else if (pFileMem->cursor > pNode->data.file.size) {
        /* Zero out any gap between old size and cursor. */
        FS_MEM_ZERO_MEMORY(FS_MEM_OFFSET_PTR(pNode->data.file.pData, pNode->data.file.size), (size_t)(pFileMem->cursor - pNode->data.file.size));
    }
    
    /* Copy the data. */
    FS_MEM_COPY_MEMORY(FS_MEM_OFFSET_PTR(pNode->data.file.pData, pFileMem->cursor), pSrc, bytesToWrite);
    
    /* Update file size and cursor. */
    pNode->data.file.size = newSize;
    pNode->data.file.modificationTime = time(NULL);
    pFileMem->cursor += bytesToWrite;
    
    *pBytesWritten = bytesToWrite;
    return FS_SUCCESS;
}

static fs_result fs_file_write_mem(fs_file* pFile, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten)
{
    fs_file_mem* pFileMem;
    fs_mem* pMem;
    fs_result result;
    
    pFileMem = (fs_file_mem*)fs_file_get_backend_data(pFile);
    FS_MEM_ASSERT(pFileMem != NULL);
    
    pMem = (fs_mem*)fs_get_backend_data(fs_file_get_fs(pFile));
    FS_MEM_ASSERT(pMem != NULL);
    
    fs_mem_lock(pMem);
    {
        result = fs_file_write_mem_nolock(pFileMem, pSrc, bytesToWrite, pBytesWritten, fs_get_allocation_callbacks(fs_file_get_fs(pFile)));
    }
    fs_mem_unlock(pMem);
    
    return result;
}

static fs_result fs_file_seek_mem_nolock(fs_file_mem* pFileMem, fs_int64 offset, fs_seek_origin origin)
{
    fs_mem_node* pNode;
    fs_int64 newCursor;
    
    pNode = pFileMem->pNode;
    FS_MEM_ASSERT(pNode != NULL);
    FS_MEM_ASSERT(pNode->type == FS_MEM_NODE_TYPE_FILE);
    
    switch (origin) {
        case FS_SEEK_SET:
        {
            newCursor = offset;
        } break;
        case FS_SEEK_CUR:
        {
            newCursor = (fs_int64)pFileMem->cursor + offset;
        } break;
        case FS_SEEK_END:
        {
            newCursor = (fs_int64)pNode->data.file.size + offset;
        } break;
        default:
        {
            return FS_INVALID_ARGS;
        }
    }
    
    if (newCursor < 0) {
        return FS_BAD_SEEK;
    }
    
    pFileMem->cursor = (fs_uint64)newCursor;
    
    return FS_SUCCESS;
}

static fs_result fs_file_seek_mem(fs_file* pFile, fs_int64 offset, fs_seek_origin origin)
{
    fs_file_mem* pFileMem;
    fs_mem* pMem;
    fs_result result;
    
    pFileMem = (fs_file_mem*)fs_file_get_backend_data(pFile);
    FS_MEM_ASSERT(pFileMem != NULL);
    
    pMem = (fs_mem*)fs_get_backend_data(fs_file_get_fs(pFile));
    FS_MEM_ASSERT(pMem != NULL);
    
    fs_mem_lock(pMem);
    {
        result = fs_file_seek_mem_nolock(pFileMem, offset, origin);
    }
    fs_mem_unlock(pMem);
    
    return result;
}

static fs_result fs_file_tell_mem(fs_file* pFile, fs_int64* pCursor)
{
    fs_file_mem* pFileMem;
    
    if (pCursor == NULL) {
        return FS_INVALID_ARGS;
    }
    
    pFileMem = (fs_file_mem*)fs_file_get_backend_data(pFile);
    FS_MEM_ASSERT(pFileMem != NULL);
    
    *pCursor = (fs_int64)pFileMem->cursor;
    
    return FS_SUCCESS;
}

static fs_result fs_file_flush_mem(fs_file* pFile)
{
    /* Nothing to flush for in-memory files. */
    (void)pFile;
    return FS_SUCCESS;
}

static fs_result fs_file_truncate_mem_nolock(fs_file_mem* pFileMem)
{
    fs_mem_node* pNode;
    
    pNode = pFileMem->pNode;
    FS_MEM_ASSERT(pNode != NULL);
    FS_MEM_ASSERT(pNode->type == FS_MEM_NODE_TYPE_FILE);
    
    /* Check if writing is allowed. */
    if ((pFileMem->openMode & FS_WRITE) == 0) {
        return FS_ACCESS_DENIED;
    }
    
    /* Truncate at current cursor position. */
    pNode->data.file.size = (size_t)pFileMem->cursor;
    pNode->data.file.modificationTime = time(NULL);
    
    return FS_SUCCESS;
}

static fs_result fs_file_truncate_mem(fs_file* pFile)
{
    fs_file_mem* pFileMem;
    fs_mem* pMem;
    fs_result result;
    
    pFileMem = (fs_file_mem*)fs_file_get_backend_data(pFile);
    FS_MEM_ASSERT(pFileMem != NULL);
    
    pMem = (fs_mem*)fs_get_backend_data(fs_file_get_fs(pFile));
    FS_MEM_ASSERT(pMem != NULL);
    
    fs_mem_lock(pMem);
    {
        result = fs_file_truncate_mem_nolock(pFileMem);
    }
    fs_mem_unlock(pMem);
    
    return result;
}

static fs_result fs_file_info_mem(fs_file* pFile, fs_file_info* pInfo)
{
    fs_file_mem* pFileMem;
    fs_mem_node* pNode;
    
    if (pInfo == NULL) {
        return FS_INVALID_ARGS;
    }
    
    pFileMem = (fs_file_mem*)fs_file_get_backend_data(pFile);
    FS_MEM_ASSERT(pFileMem != NULL);
    
    pNode = pFileMem->pNode;
    FS_MEM_ASSERT(pNode != NULL);
    FS_MEM_ASSERT(pNode->type == FS_MEM_NODE_TYPE_FILE);
    
    FS_MEM_ZERO_OBJECT(pInfo);
    pInfo->size = pNode->data.file.size;
    pInfo->directory = 0;
    pInfo->symlink = 0;
    
    return FS_SUCCESS;
}

static fs_result fs_file_duplicate_mem(fs_file* pFile, fs_file* pDuplicate)
{
    fs_file_mem* pFileMem;
    fs_file_mem* pDuplicateMem;
    
    pFileMem = (fs_file_mem*)fs_file_get_backend_data(pFile);
    FS_MEM_ASSERT(pFileMem != NULL);
    
    pDuplicateMem = (fs_file_mem*)fs_file_get_backend_data(pDuplicate);
    FS_MEM_ASSERT(pDuplicateMem != NULL);
    
    /* Copy the file handle data. */
    *pDuplicateMem = *pFileMem;
    
    return FS_SUCCESS;
}


static fs_iterator* fs_first_mem_nolock(fs* pFS, const char* pDirectoryPath, size_t pathLen)
{
    fs_mem_node* pDirectory;
    fs_iterator_mem* pIterator;
    fs_result result;
    
    /* Find the directory. */
    result = fs_mem_resolve_path(pFS, pDirectoryPath, pathLen, &pDirectory, NULL, NULL);
    if (result != FS_SUCCESS || pDirectory == NULL || pDirectory->type != FS_MEM_NODE_TYPE_DIRECTORY) {
        return NULL;
    }
    
    /* Find first valid entry. */
    if (pDirectory->data.dir.childCount == 0) {
        /* Empty directory. */
        return NULL;
    }
    
    /* Get the first child to determine name length for allocation. */
    {
        fs_mem_node* pChild = pDirectory->data.dir.ppChildren[0];
        size_t nameLen = pChild->nameLen;
        
        /* Allocate iterator with space for the name at the end. */
        pIterator = (fs_iterator_mem*)fs_malloc(sizeof(fs_iterator_mem) + nameLen + 1, fs_get_allocation_callbacks(pFS));
        if (pIterator == NULL) {
            return NULL;
        }
        
        FS_MEM_ZERO_OBJECT(pIterator);
        
        pIterator->base.pFS = pFS;
        pIterator->pDirectory = pDirectory;
        pIterator->currentIndex = 0;
        
        /* Copy name to the end of the structure. */
        FS_MEM_COPY_MEMORY((char*)pIterator + sizeof(fs_iterator_mem), pChild->pName, nameLen);
        ((char*)pIterator + sizeof(fs_iterator_mem))[nameLen] = '\0';
        
        pIterator->base.pName = (const char*)pIterator + sizeof(fs_iterator_mem);
        pIterator->base.nameLen = nameLen;
        pIterator->base.info.symlink = 0;
        
        /* Fill in file info. */
        if (pChild->type == FS_MEM_NODE_TYPE_FILE) {
            pIterator->base.info.size = pChild->data.file.size;
            pIterator->base.info.directory = 0;
        } else {
            pIterator->base.info.size = 0;
            pIterator->base.info.directory = 1;
        }
    }
    
    return (fs_iterator*)pIterator;
}

static fs_iterator* fs_first_mem(fs* pFS, const char* pDirectoryPath, size_t directoryPathLen)
{
    fs_mem* pMem;
    fs_iterator* pIterator;
    
    pMem = (fs_mem*)fs_get_backend_data(pFS);

    fs_mem_lock(pMem);
    {
        pIterator = fs_first_mem_nolock(pFS, pDirectoryPath, directoryPathLen);
    }
    fs_mem_unlock(pMem);
    
    return pIterator;
}

static fs_iterator* fs_next_mem_nolock(fs_iterator_mem* pIteratorMem, const fs_allocation_callbacks* pAllocationCallbacks)
{
    fs_mem_node* pChild;
    fs_iterator_mem* pNewIterator;
    size_t nameLen;
    
    /* Move to next entry. */
    pIteratorMem->currentIndex += 1;
    
    /* Check if we've reached the end. */
    if (pIteratorMem->currentIndex >= pIteratorMem->pDirectory->data.dir.childCount) {
        fs_free(pIteratorMem, pAllocationCallbacks);
        return NULL;
    }
    
    /* Get the next child. */
    pChild = pIteratorMem->pDirectory->data.dir.ppChildren[pIteratorMem->currentIndex];
    nameLen = pChild->nameLen;
    
    /* Reallocate iterator with space for the new name. */
    pNewIterator = (fs_iterator_mem*)fs_realloc(pIteratorMem, sizeof(fs_iterator_mem) + nameLen + 1, pAllocationCallbacks);
    if (pNewIterator == NULL) {
        fs_free(pIteratorMem, pAllocationCallbacks);
        return NULL;
    }
    
    /* Copy name to the end of the structure. */
    FS_MEM_COPY_MEMORY((char*)pNewIterator + sizeof(fs_iterator_mem), pChild->pName, nameLen);
    ((char*)pNewIterator + sizeof(fs_iterator_mem))[nameLen] = '\0';
    
    pNewIterator->base.pName = (const char*)pNewIterator + sizeof(fs_iterator_mem);
    pNewIterator->base.nameLen = nameLen;
    pNewIterator->base.info.symlink = 0;
    
    /* Fill in file info. */
    if (pChild->type == FS_MEM_NODE_TYPE_FILE) {
        pNewIterator->base.info.size      = pChild->data.file.size;
        pNewIterator->base.info.directory = 0;
    } else {
        pNewIterator->base.info.size      = 0;
        pNewIterator->base.info.directory = 1;
    }

    return (fs_iterator*)pNewIterator;
}

static fs_iterator* fs_next_mem(fs_iterator* pIterator)
{
    fs_iterator_mem* pIteratorMem;
    fs_mem* pMem;
    fs_iterator* pResult;
    
    if (pIterator == NULL) {
        return NULL;
    }
    
    pIteratorMem = (fs_iterator_mem*)pIterator;

    pMem = (fs_mem*)fs_get_backend_data(pIterator->pFS);
    FS_MEM_ASSERT(pMem != NULL);
    
    fs_mem_lock(pMem);
    {
        pResult = fs_next_mem_nolock(pIteratorMem, fs_get_allocation_callbacks(pIterator->pFS));
    }
    fs_mem_unlock(pMem);
    
    return pResult;
}

static void fs_free_iterator_mem(fs_iterator* pIterator)
{
    fs_iterator_mem* pIteratorMem;
    
    if (pIterator == NULL) {
        return;
    }
    
    pIteratorMem = (fs_iterator_mem*)pIterator;
    
    /* Name is allocated as part of the iterator, so no separate free needed. */
    fs_free(pIteratorMem, fs_get_allocation_callbacks(pIterator->pFS));
}


static fs_backend fs_mem_backend =
{
    fs_alloc_size_mem,
    fs_init_mem,
    fs_uninit_mem,
    fs_remove_mem,
    fs_rename_mem,
    fs_mkdir_mem,
    fs_info_mem,
    fs_file_alloc_size_mem,
    fs_file_open_mem,
    fs_file_close_mem,
    fs_file_read_mem,
    fs_file_write_mem,
    fs_file_seek_mem,
    fs_file_tell_mem,
    fs_file_flush_mem,
    fs_file_truncate_mem,
    fs_file_info_mem,
    fs_file_duplicate_mem,
    fs_first_mem,
    fs_next_mem,
    fs_free_iterator_mem
};
const fs_backend* FS_MEM = &fs_mem_backend;

#endif  /* fs_mem_c. */