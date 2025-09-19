#include "../fs.c"
#include "../extras/backends/zip/fs_zip.h"
#include "../extras/backends/pak/fs_pak.h"
#include "../extras/backends/mem/fs_mem.h"

#include "files/test1.zip.c"
#include "files/test2.zip.c"

#include <stdio.h>
#include <assert.h>
#include <string.h>

const fs_backend* fs_test_get_backend(int argc, char** argv)
{
    int iarg;

    /* Check if a specific backend is requested via the command line. */
    for (iarg = 1; iarg < argc; iarg += 1) {
        /*  */ if (strcmp(argv[iarg], "posix") == 0) {
            return FS_BACKEND_POSIX;
        } else if (strcmp(argv[iarg], "win32") == 0) {
            return FS_BACKEND_WIN32;
        } else {
            printf("Unknown backend: %s\n", argv[iarg]);
            break;
        }
    }

    /* Getting here means no backend was passed onto the command line. Fall back to defaults. */
    return fs_get_default_backend();
}

const char* fs_test_get_backend_name(const fs_backend* pBackend)
{
    /*  */ if (pBackend == FS_BACKEND_POSIX) {
        return "POSIX";
    } else if (pBackend == FS_BACKEND_WIN32) {
        return "Win32";
    } else {
        return "Unknown";
    }
}


/* BEG fs_test.c */
typedef struct fs_test fs_test;

typedef int (* fs_test_proc)(fs_test* pUserData);

struct fs_test
{
    const char* name;
    fs_test_proc proc;
    void* pUserData;
    int result;
    fs_test* pFirstChild;
    fs_test* pNextSibling;
};

void fs_test_init(fs_test* pTest, const char* name, fs_test_proc proc, void* pUserData, fs_test* pParent)
{
    if (pTest == NULL) {
        return;
    }

    memset(pTest, 0, sizeof(fs_test));
    pTest->name = name;
    pTest->proc = proc;
    pTest->pUserData = pUserData;
    pTest->result = FS_SUCCESS;
    pTest->pFirstChild = NULL;
    pTest->pNextSibling = NULL;

    if (pParent != NULL) {
        if (pParent->pFirstChild == NULL) {
            pParent->pFirstChild = pTest;
        } else {
            fs_test* pSibling = pParent->pFirstChild;
            while (pSibling->pNextSibling != NULL) {
                pSibling = pSibling->pNextSibling;
            }

            pSibling->pNextSibling = pTest;
        }
    }
}

void fs_test_count(fs_test* pTest, int* pCount, int* pPassed)
{
    fs_test* pChild;

    if (pTest == NULL) {
        return;
    }

    *pCount += 1;

    if (pTest->result == FS_SUCCESS) {
        *pPassed += 1;
    }

    pChild = pTest->pFirstChild;
    while (pChild != NULL) {
        fs_test_count(pChild, pCount, pPassed);
        pChild = pChild->pNextSibling;
    }
}

int fs_test_run(fs_test* pTest)
{
    /* Start our counts at -1 to exclude the root test. */
    int testCount = -1;
    int passedCount = -1;

    if (pTest == NULL) {
        return FS_ERROR;
    }

    if (pTest->name != NULL && pTest->proc != NULL) {
        printf("Running Test: %s\n", pTest->name);
    }

    if (pTest->proc != NULL) {
        pTest->result = pTest->proc(pTest);
        if (pTest->result != FS_SUCCESS) {
            return pTest->result;
        }
    }

    /* Now we need to recursively execute children. If any child test fails, the parent test needs to be marked as failed as well. */
    {
        fs_test* pChild = pTest->pFirstChild;
        while (pChild != NULL) {
            int result = fs_test_run(pChild);
            if (result != FS_SUCCESS) {
                pTest->result = result;
            }

            pChild = pChild->pNextSibling;
        }
    }

    /* Now count the number of failed tests and report success or failure depending on the result. */
    fs_test_count(pTest, &testCount, &passedCount);

    return (testCount == passedCount) ? FS_SUCCESS : FS_ERROR;
}

void fs_test_print_local_result(fs_test* pTest, int level)
{
    if (pTest == NULL) {
        return;
    }

    printf("[%s] %*s%s\n", pTest->result == FS_SUCCESS ? "PASS" : "FAIL", level * 2, "", pTest->name);
}

void fs_test_print_child_results(fs_test* pTest, int level)
{
    fs_test* pChild;

    if (pTest == NULL) {
        return;
    }

    pChild = pTest->pFirstChild;
    while (pChild != NULL) {
        fs_test_print_local_result(pChild, level);
        fs_test_print_child_results(pChild, level + 1);

        pChild = pChild->pNextSibling;
    }
}

void fs_test_print_result(fs_test* pTest, int level)
{
    fs_test* pChild;

    if (pTest == NULL) {
        return;
    }

    if (pTest->name != NULL) {
        printf("[%s] %*s%s\n", pTest->result == FS_SUCCESS ? "PASS" : "FAIL", level * 2, "", pTest->name);
        level += 1;
    }

    pChild = pTest->pFirstChild;
    while (pChild != NULL) {
        fs_test_print_result(pChild, level);
        pChild = pChild->pNextSibling;
    }
}

void fs_test_print_summary(fs_test* pTest)
{
    /* Start our counts at -1 to exclude the root test. */
    int testCount = -1;
    int passedCount = -1;

    if (pTest == NULL) {
        return;
    }

    /* This should only be called on a root test. */
    assert(pTest->name == NULL);

    printf("=== Test Summary ===\n");
    fs_test_print_result(pTest, 0);

    /* We need to count how many tests failed. */
    fs_test_count(pTest, &testCount, &passedCount);
    printf("---\n%s%d / %d tests passed.\n", (testCount == passedCount) ? "[PASS]: " : "[FAIL]: ", passedCount, testCount);
}
/* END fs_test.c */


/* BEG common */
fs_result fs_test_open_and_write_file(fs_test* pTest, fs* pFS, const char* pPath, int openMode, const void* pData, size_t dataSize)
{
    fs_result result;
    fs_file* pFile;

    result = fs_file_open(pFS, pPath, openMode, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file \"%s\" for writing.\n", pTest->name, pPath);
        return result;
    }

    if (pData != NULL && dataSize > 0) {
        size_t bytesWritten;

        result = fs_file_write(pFile, pData, dataSize, &bytesWritten);
        if (result != FS_SUCCESS) {
            printf("%s: Failed to write to file \"%s\".\n", pTest->name, pPath);
            fs_file_close(pFile);
            return result;
        }

        if (bytesWritten != dataSize) {
            printf("%s: Wrote %d bytes to file \"%s\", expected to write %d bytes.\n", pTest->name, (int)bytesWritten, pPath, (int)dataSize);
            fs_file_close(pFile);
            return FS_ERROR;
        }
    }

    fs_file_close(pFile);
    return FS_SUCCESS;
}

fs_result fs_test_open_and_read_file(fs_test* pTest, fs* pFS, const char* pPath, int openMode, const void* pExpectedData, size_t expectedDataSize)
{
    fs_result result;
    fs_file* pFile;
    void* pActualData;
    size_t actualDataSize;
    
    result = fs_file_open(pFS, pPath, openMode, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file \"%s\".\n", pTest->name, pPath);
        return result;
    }

    result = fs_file_read_to_end(pFile, FS_FORMAT_BINARY, &pActualData, &actualDataSize);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to read file \"%s\".\n", pTest->name, pPath);
        fs_file_close(pFile);
        return result;
    }

    if (actualDataSize != expectedDataSize || memcmp(pActualData, pExpectedData, expectedDataSize) != 0) {
        printf("%s: File \"%s\" contents do not match expected data.\n", pTest->name, pPath);
        fs_free(pActualData, fs_get_allocation_callbacks(pFS));
        fs_file_close(pFile);
        return FS_ERROR;
    }

    fs_free(pActualData, fs_get_allocation_callbacks(pFS));
    fs_file_close(pFile);
    return FS_SUCCESS;
}
/* END common */


/* BEG path_iteration */
static int fs_test_breakup_path_forward(const char* pPath, size_t pathLen, fs_path_iterator pIterator[32], size_t* pCount)
{
    fs_result result;
    fs_path_iterator i;

    *pCount = 0;

    for (result = fs_path_first(pPath, pathLen, &i); result == FS_SUCCESS; result = fs_path_next(&i)) {
        pIterator[*pCount] = i;
        *pCount += 1;
    }

    if (result == FS_SUCCESS || result == FS_AT_END) {
        return 0;
    } else {
        return 1;
    }
}

static int fs_test_breakup_path_reverse(const char* pPath, size_t pathLen, fs_path_iterator pIterator[32], size_t* pCount)
{
    fs_result result;
    fs_path_iterator i;

    *pCount = 0;
    
    for (result = fs_path_last(pPath, pathLen, &i); result == FS_SUCCESS; result = fs_path_prev(&i)) {
        pIterator[*pCount] = i;
        *pCount += 1;
    }

    if (result == FS_SUCCESS || result == FS_AT_END) {
        return 0;
    } else {
        return 1;
    }
}

static int fs_test_reconstruct_path_forward(const fs_path_iterator* pIterator, size_t iteratorCount, char pPath[256])
{
    size_t i;
    size_t len = 0;

    pPath[0] = '\0';

    for (i = 0; i < iteratorCount; i += 1) {
        fs_strncpy(pPath + len, pIterator[i].pFullPath + pIterator[i].segmentOffset, pIterator[i].segmentLength);
        len += pIterator[i].segmentLength;

        if (i+1 < iteratorCount) {
            pPath[len] = '/';
            len += 1;
        }
    }

    pPath[len] = '\0';
    return 0;
}

static int fs_test_reconstruct_path_reverse(const fs_path_iterator* pIterator, size_t iteratorCount, char pPath[256])
{
    size_t i;
    size_t len = 0;

    pPath[0] = '\0';

    for (i = iteratorCount; i > 0; i--) {
        fs_strncpy(pPath + len, pIterator[i-1].pFullPath + pIterator[i-1].segmentOffset, pIterator[i-1].segmentLength);
        len += pIterator[i-1].segmentLength;

        if (i-1 > 0) {
            pPath[len] = '/';
            len += 1;
        }
    }

    pPath[len] = '\0';
    return 0;
}

static int fs_test_path_iteration_internal(fs_test* pTest, const char* pPath)
{
    fs_path_iterator segmentsForward[32];
    fs_path_iterator segmentsReverse[32];
    size_t segmentsForwardCount;
    size_t segmentsReverseCount;
    char pPathReconstructedForward[256];
    char pPathReconstructedReverse[256];
    int forwardResult = 0;
    int reverseResult = 0;

    (void)pTest;

    fs_test_breakup_path_forward(pPath, (size_t)-1, segmentsForward, &segmentsForwardCount);
    fs_test_breakup_path_reverse(pPath, (size_t)-1, segmentsReverse, &segmentsReverseCount);

    fs_test_reconstruct_path_forward(segmentsForward, segmentsForwardCount, pPathReconstructedForward);
    fs_test_reconstruct_path_reverse(segmentsReverse, segmentsReverseCount, pPathReconstructedReverse);

    if (strcmp(pPath, pPathReconstructedForward) != 0) {
        printf("%s: Forward reconstruction failed. Expecting \"%s\", got \"%s\"\n", pTest->name, pPath, pPathReconstructedForward);
        forwardResult = 1;
    }
    if (strcmp(pPath, pPathReconstructedReverse) != 0) {
        printf("%s: Reverse reconstruction failed. Expecting \"%s\", got \"%s\"\n", pTest->name, pPath, pPathReconstructedReverse);
        reverseResult = 1;
    }

    if (forwardResult == 0 && reverseResult == 0) {
        return 0;
    } else {
        return 1;
    }
}

int fs_test_path_iteration(fs_test* pTest)
{
    int errorCount = 0;

    errorCount += fs_test_path_iteration_internal(pTest, "/");
    errorCount += fs_test_path_iteration_internal(pTest, "");
    errorCount += fs_test_path_iteration_internal(pTest, "/abc");
    errorCount += fs_test_path_iteration_internal(pTest, "/abc/");
    errorCount += fs_test_path_iteration_internal(pTest, "abc/");
    errorCount += fs_test_path_iteration_internal(pTest, "/abc/def/ghi");
    errorCount += fs_test_path_iteration_internal(pTest, "/abc/def/ghi/");
    errorCount += fs_test_path_iteration_internal(pTest, "abc/def/ghi/");
    errorCount += fs_test_path_iteration_internal(pTest, "C:");
    errorCount += fs_test_path_iteration_internal(pTest, "C:/");
    errorCount += fs_test_path_iteration_internal(pTest, "C:/abc");
    errorCount += fs_test_path_iteration_internal(pTest, "C:/abc/");
    errorCount += fs_test_path_iteration_internal(pTest, "C:/abc/def/ghi");
    errorCount += fs_test_path_iteration_internal(pTest, "C:/abc/def/ghi/");
    errorCount += fs_test_path_iteration_internal(pTest, "//localhost");
    errorCount += fs_test_path_iteration_internal(pTest, "//localhost/abc");
    errorCount += fs_test_path_iteration_internal(pTest, "//localhost//abc");
    errorCount += fs_test_path_iteration_internal(pTest, "~");
    errorCount += fs_test_path_iteration_internal(pTest, "~/Documents");

    if (errorCount == 0) {
        return FS_SUCCESS;
    } else {
        return FS_ERROR;
    }
}
/* END path_iteration */

/* BEG path_normalize */
static int fs_test_path_normalize_internal(fs_test* pTest, const char* pPath, const char* pExpected)
{
    char pNormalizedPath[256];
    int result;
    int length;

    /* Get the length first so we can check that it's working correctly. */
    length = fs_path_normalize(NULL, 0, pPath, FS_NULL_TERMINATED, 0);

    result = fs_path_normalize(pNormalizedPath, sizeof(pNormalizedPath), pPath, FS_NULL_TERMINATED, 0);
    if (result < 0) {
        if (pExpected != NULL) {
            printf("%s: Failed to normalize \"%s\"\n", pTest->name, pPath);
            return 1;
        } else {
            /* pExpected is NULL. We use this to indicate that we're expecting this to fail. So a failure is a successful test. */
            return 0;
        }
    }

    if (pExpected == NULL) {
        /* We expected this to fail, but it succeeded. */
        printf("%s: Expected normalization of \"%s\" to fail, but it succeeded as \"%s\"\n", pTest->name, pPath, pNormalizedPath);
        return 1;
    }

    /* Compare the length. */
    if (length != result) {
        printf("%s: Length mismatch for \"%s\": expected %d, got %d\n", pTest->name, pPath, length, result);
        return 1;
    }

    /* Compare the result with expected. */
    if (strcmp(pNormalizedPath, pExpected) != 0) {
        printf("%s: Normalized \"%s\" does not match expected \"%s\"\n", pTest->name, pNormalizedPath, pExpected);
        return 1;
    }

    return 0;
}

int fs_test_path_normalize(fs_test* pTest)
{
    int errorCount = 0;

    errorCount += fs_test_path_normalize_internal(pTest, "", "");
    errorCount += fs_test_path_normalize_internal(pTest, "/", "/");
    errorCount += fs_test_path_normalize_internal(pTest, "/abc/def/ghi", "/abc/def/ghi");
    errorCount += fs_test_path_normalize_internal(pTest, "/..", NULL);   /* Expecting error. */
    errorCount += fs_test_path_normalize_internal(pTest, "..", "..");
    errorCount += fs_test_path_normalize_internal(pTest, "abc/../def", "def");
    errorCount += fs_test_path_normalize_internal(pTest, "abc/./def", "abc/def");
    errorCount += fs_test_path_normalize_internal(pTest, "../abc/def", "../abc/def");
    errorCount += fs_test_path_normalize_internal(pTest, "abc/def/..", "abc");
    errorCount += fs_test_path_normalize_internal(pTest, "abc/../../def", "../def");
    errorCount += fs_test_path_normalize_internal(pTest, "/abc/../../def", NULL);   /* Expecting error. */
    errorCount += fs_test_path_normalize_internal(pTest, "abc/def/", "abc/def");
    errorCount += fs_test_path_normalize_internal(pTest, "/abc/def/", "/abc/def");

    if (errorCount == 0) {
        return FS_SUCCESS;
    } else {
        return FS_ERROR;
    }
}
/* END path_normalize */

/* BEG path_trim_base */
int fs_test_path_trim_base_internal(fs_test* pTest, const char* pPath, size_t pathLen, const char* pBasePath, size_t basePathLen, const char* pExpected)
{
    const char* pResult;
    char pPathNT[256] = {0};
    char pBaseNT[256] = {0};
    size_t resultOffset;

    /* Make a copy of the input strings just for diagnostics. */
    if (pPath != NULL) {
        if (pathLen == FS_NULL_TERMINATED) {
            fs_strncpy(pPathNT, pPath, sizeof(pPathNT));
        } else {
            fs_strncpy(pPathNT, pPath, pathLen);
        }
    }

    if (pBasePath != NULL) {
        if (basePathLen == FS_NULL_TERMINATED) {
            fs_strncpy(pBaseNT, pBasePath, sizeof(pBaseNT));
        } else {
            fs_strncpy(pBaseNT, pBasePath, basePathLen);
        }
    }

    pResult = fs_path_trim_base(pPath, pathLen, pBasePath, basePathLen);
    if (pResult == NULL) {
        if (pExpected == NULL) {
            return 0;   /* Expected failure. */
        } else {
            printf("%s: Unexpected failure when trimming \"%s\" with base \"%s\"\n", pTest->name, pPathNT, pBaseNT);
            return 1;   /* Unexpected failure. */
        }
    }

    if (pExpected == NULL) {
        printf("%s: Expected failure when trimming \"%s\" with base \"%s\", but got success.\n", pTest->name, pPathNT, pBaseNT);
        return 1;   /* Unexpected success. */
    }

    resultOffset = (size_t)(pResult - pPath);

    if (fs_strncmp(pResult, pExpected, pathLen - resultOffset) != 0) {
        printf("%s: Trimmed path does not match expected. Got \"%s\", expected \"%s\"\n", pTest->name, pPathNT + resultOffset, pExpected);
        return 1;
    }

    return 0;
}

int fs_test_path_trim_base(fs_test* pTest)
{
    int errorCount = 0;

    errorCount += fs_test_path_trim_base_internal(pTest, "/abc/def", FS_NULL_TERMINATED, "/abc",     FS_NULL_TERMINATED, "def");
    errorCount += fs_test_path_trim_base_internal(pTest, "/abc/def", FS_NULL_TERMINATED, "/abc/def", FS_NULL_TERMINATED, "");
    errorCount += fs_test_path_trim_base_internal(pTest, "/abc/def", FS_NULL_TERMINATED, "/xyz",     FS_NULL_TERMINATED, NULL);         /* Does not start with the base. We should expect NULL to be returned here. */
    errorCount += fs_test_path_trim_base_internal(pTest, "/abc/def", FS_NULL_TERMINATED, NULL,       FS_NULL_TERMINATED, "/abc/def");   /* A base path of NULL is equivalent to "". */
    errorCount += fs_test_path_trim_base_internal(pTest, "/abc/def", FS_NULL_TERMINATED, "/abc/",    FS_NULL_TERMINATED, "def");        /* A trailing slash at the end of the end of the base should be ignored. */
    errorCount += fs_test_path_trim_base_internal(pTest, "/abc/def", FS_NULL_TERMINATED, "/",        FS_NULL_TERMINATED, "abc/def");    /* A base path of "/" should result in the entire path being returned, minus the leading slash. */
    errorCount += fs_test_path_trim_base_internal(pTest, "abc/def",  FS_NULL_TERMINATED, "abc",      FS_NULL_TERMINATED, "def");
    errorCount += fs_test_path_trim_base_internal(pTest, "abc/def",  FS_NULL_TERMINATED, "abc/def",  FS_NULL_TERMINATED, "");
    errorCount += fs_test_path_trim_base_internal(pTest, "abc/def",  FS_NULL_TERMINATED, "xyz",      FS_NULL_TERMINATED, NULL);         /* Does not start with the base. We should expect NULL to be returned here. */
    errorCount += fs_test_path_trim_base_internal(pTest, "abc/def",  FS_NULL_TERMINATED, NULL,       FS_NULL_TERMINATED, "abc/def");    /* A base path of NULL is equivalent to "". */
    errorCount += fs_test_path_trim_base_internal(pTest, "abc/def",  FS_NULL_TERMINATED, "abc/",     FS_NULL_TERMINATED, "def");        /* A trailing slash at the end of the end of the base should be ignored. */
    errorCount += fs_test_path_trim_base_internal(pTest, "abc/def",  FS_NULL_TERMINATED, "",         FS_NULL_TERMINATED, "abc/def");    /* A base path of "/" should result in the entire path being returned, minus the leading slash. */
    errorCount += fs_test_path_trim_base_internal(pTest, "/abc",     FS_NULL_TERMINATED, "/abc",     FS_NULL_TERMINATED, "");
    errorCount += fs_test_path_trim_base_internal(pTest, "/abc",     FS_NULL_TERMINATED, "/abc/def", FS_NULL_TERMINATED, NULL);
    errorCount += fs_test_path_trim_base_internal(pTest, "/abc",     FS_NULL_TERMINATED, "/xyz",     FS_NULL_TERMINATED, NULL);
    errorCount += fs_test_path_trim_base_internal(pTest, "/abc",     FS_NULL_TERMINATED, NULL,       FS_NULL_TERMINATED, "/abc");
    errorCount += fs_test_path_trim_base_internal(pTest, "/abc",     FS_NULL_TERMINATED, "/abc/",    FS_NULL_TERMINATED, "");
    errorCount += fs_test_path_trim_base_internal(pTest, "/abc",     FS_NULL_TERMINATED, "/",        FS_NULL_TERMINATED, "abc");
    errorCount += fs_test_path_trim_base_internal(pTest, "abc",      FS_NULL_TERMINATED, "abc",      FS_NULL_TERMINATED, "");
    errorCount += fs_test_path_trim_base_internal(pTest, "abc",      FS_NULL_TERMINATED, "abc/def",  FS_NULL_TERMINATED, NULL);
    errorCount += fs_test_path_trim_base_internal(pTest, "abc",      FS_NULL_TERMINATED, "xyz",      FS_NULL_TERMINATED, NULL);
    errorCount += fs_test_path_trim_base_internal(pTest, "abc",      FS_NULL_TERMINATED, NULL,       FS_NULL_TERMINATED, "abc");
    errorCount += fs_test_path_trim_base_internal(pTest, "abc",      FS_NULL_TERMINATED, "abc/",     FS_NULL_TERMINATED, "");
    errorCount += fs_test_path_trim_base_internal(pTest, "abc",      FS_NULL_TERMINATED, "",         FS_NULL_TERMINATED, "abc");
    errorCount += fs_test_path_trim_base_internal(pTest, NULL,       0,                  "/abc",     FS_NULL_TERMINATED, NULL);         /* A NULL path should always return NULL. */

    if (errorCount == 0) {
        return FS_SUCCESS;
    } else {
        return FS_ERROR;
    }
}
/* END path_trim_base*/





/* BEG test_state */
typedef struct
{
    const fs_backend* pBackend;
    fs* pFS;
    char pTempDir[256];
} fs_test_state;

fs_test_state fs_test_state_init(const fs_backend* pBackend)
{
    fs_test_state state;

    memset(&state, 0, sizeof(state));
    state.pBackend = pBackend;

    return state;
}
/* END test_state */

/* BEG system_sysdir */
int fs_test_system_sysdir_internal(fs_test* pTest, fs_sysdir_type type, const char* pTypeName)
{
    char path[256];
    size_t pathLen;

    pathLen = fs_sysdir(type, path, sizeof(path));
    if (pathLen == 0) {
        printf("%s: Failed to get system directory path for %s\n", pTest->name, pTypeName);
        return 1;
    }

    printf("%s: %s = %s\n", pTest->name, pTypeName, path);

    return 0;
}

int fs_test_system_sysdir(fs_test* pTest)
{
    int errorCount = 0;

    errorCount += fs_test_system_sysdir_internal(pTest, FS_SYSDIR_HOME,   "HOME"  );
    errorCount += fs_test_system_sysdir_internal(pTest, FS_SYSDIR_TEMP,   "TEMP"  );
    errorCount += fs_test_system_sysdir_internal(pTest, FS_SYSDIR_CONFIG, "CONFIG");
    errorCount += fs_test_system_sysdir_internal(pTest, FS_SYSDIR_DATA,   "DATA"  );
    errorCount += fs_test_system_sysdir_internal(pTest, FS_SYSDIR_CACHE,  "CACHE" );

    if (errorCount == 0) {
        return FS_SUCCESS;
    } else {
        return FS_ERROR;
    }
}
/* END system_sysdir */

/* BEG system_init */
int fs_test_system_init(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_config fsConfig;
    fs* pFS;

    fsConfig = fs_config_init(pTestState->pBackend, NULL, NULL);

    result = fs_init(&fsConfig, &pFS);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to initialize file system.\n", pTest->name);
        return FS_ERROR;
    }

    pTestState->pFS = pFS;

    return FS_SUCCESS;
}
/* END system_init */

/* BEG system_mktmp */
int fs_test_system_mktmp(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    char pTempFile[256];

    if (pTestState->pFS == NULL) {
        printf("%s: File system not initialized. Aborting test.\n", pTest->name);
        return FS_ERROR;
    }

    /* Start with creating a temporary directory. If this works, the output from this will be where we output our test files going forward in future tests. */
    result = fs_mktmp("fs_", pTestState->pTempDir, sizeof(pTestState->pTempDir), FS_MKTMP_DIR);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create temporary directory.\n", pTest->name);
        return FS_ERROR;
    }

    printf("%s: [DIR]  %s\n", pTest->name, pTestState->pTempDir);


    /* Now for a file. We just discard with this straight away. */
    result = fs_mktmp("fs_", pTempFile, sizeof(pTempFile), FS_MKTMP_FILE);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create temporary file.\n", pTest->name);
        return FS_ERROR;
    }

    printf("%s: [FILE] %s\n", pTest->name, pTempFile);

    /* We're going to delete the temp file just to keep the temp folder cleaner and easier to find actual test files. */
    result = fs_remove(pTestState->pFS, pTempFile, FS_IGNORE_MOUNTS);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to delete temporary file.\n", pTest->name);
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END system_mktmp */

/* BEG system_mkdir */
int fs_test_system_mkdir(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_file_info info;
    char pDirPath[256];

    fs_path_append(pDirPath, sizeof(pDirPath), pTestState->pTempDir, (size_t)-1, "dir1", (size_t)-1);

    /* Normal mkdir(). */
    result = fs_mkdir(pTestState->pFS, pDirPath, FS_IGNORE_MOUNTS);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create directory %s\n", pTest->name, pDirPath);
        return FS_ERROR;
    }

    /* Recursive. */
    fs_path_append(pDirPath, sizeof(pDirPath), pTestState->pTempDir, (size_t)-1, "dir1/dir2/dir3", (size_t)-1);

    result = fs_mkdir(pTestState->pFS, pDirPath, FS_IGNORE_MOUNTS);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create directory %s\n", pTest->name, pDirPath);
        return FS_ERROR;
    }

    /* Check that the folder was actually created. */
    result = fs_info(pTestState->pFS, pDirPath, FS_READ, &info);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to get info for directory %s\n", pTest->name, pDirPath);
        return FS_ERROR;
    }

    if (info.directory == 0) {
        printf("%s: ERROR: Path %s is not a directory.\n", pTest->name, pDirPath);
        return FS_ERROR;
    }

    /* Test that FS_ALREADY_EXISTS is returned for a directory that already exists. */
    result = fs_mkdir(pTestState->pFS, pDirPath, FS_IGNORE_MOUNTS);
    if (result != FS_ALREADY_EXISTS) {
        printf("%s: ERROR: Expected FS_ALREADY_EXISTS, but got %d\n", pTest->name, result);
        return FS_ERROR;
    }

    /* Test that FS_DOES_NOT_EXIST is returned when a parent directory does not exist. */
    result = fs_mkdir(pTestState->pFS, "does/not/exist", FS_IGNORE_MOUNTS | FS_NO_CREATE_DIRS);
    if (result != FS_DOES_NOT_EXIST) {
        printf("%s: ERROR: Expected FS_DOES_NOT_EXIST, but got %d\n", pTest->name, result);
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END system_mkdir */

/* BEG system_write_new */
int fs_test_system_write_new(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    char pFilePath[256];
    const char data[4] = {1, 2, 3, 4};

    if (pTestState->pFS == NULL) {
        printf("%s: File system not initialized. Aborting test.\n", pTest->name);
        return FS_ERROR;
    }

    fs_path_append(pFilePath, sizeof(pFilePath), pTestState->pTempDir, (size_t)-1, "a", (size_t)-1);

    result = fs_test_open_and_write_file(pTest, pTestState->pFS, pFilePath, FS_WRITE | FS_TRUNCATE | FS_IGNORE_MOUNTS, data, sizeof(data));
    if (result != FS_SUCCESS) {
        return result;
    }


    /* Now we need to open the file and verify. */
    result = fs_test_open_and_read_file(pTest, pTestState->pFS, pFilePath, FS_READ, data, sizeof(data));
    if (result != FS_SUCCESS) {
        return result;
    }

    return FS_SUCCESS;
}
/* END system_write_new */

/* BEG system_write_overwrite */
static int fs_test_system_write_overwrite_internal(fs_test* pTest, char newData[4])
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    char pFilePath[256];
    size_t newDataSize = 4;

    fs_path_append(pFilePath, sizeof(pFilePath), pTestState->pTempDir, (size_t)-1, "a", (size_t)-1);

    result = fs_test_open_and_write_file(pTest, pTestState->pFS, pFilePath, FS_WRITE | FS_IGNORE_MOUNTS, newData, newDataSize);
    if (result != FS_SUCCESS) {
        return result;
    }

    /* Now we need to open the file and verify. */
    result = fs_test_open_and_read_file(pTest, pTestState->pFS, pFilePath, FS_READ, newData, newDataSize);
    if (result != FS_SUCCESS) {
        return result;
    }

    return FS_SUCCESS;
}

int fs_test_system_write_overwrite(fs_test* pTest)
{
    /*
    At the time this test is called, the "a" file should be 4 bytes in size with the data {1, 2, 3, 4}. We'll overwrite this
    with the data {5, 6, 7, 8}, close the file, and then reopen it to verify the contents. Then we'll revert the content back
    to {1, 2, 3, 4} for the benefit of future tests which will need to read the original data.
    */
    int result;
    char data1234[4] = {1, 2, 3, 4};
    char data5678[4] = {5, 6, 7, 8};

    /* Actual test. */
    result = fs_test_system_write_overwrite_internal(pTest, data5678);
    if (result != FS_SUCCESS) {
        printf("%s: Overwrite test failed.\n", pTest->name);
        return FS_ERROR;
    }

    /* Revert back to the original data for future tests. */
    result = fs_test_system_write_overwrite_internal(pTest, data1234);
    if (result != FS_SUCCESS) {
        printf("%s: Revert test failed.\n", pTest->name);
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END system_write_overwrite */

/* BEG system_write_append */
int fs_test_system_write_append(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_file* pFile;
    fs_file_info fileInfo;
    char pFilePath[256];
    const char dataToAppend[4] = {5, 6, 7, 8};
    char dataExpected[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    size_t bytesWritten;

    if (pTestState->pFS == NULL) {
        printf("%s: File system not initialized. Aborting test.\n", pTest->name);
        return FS_ERROR;
    }

    fs_path_append(pFilePath, sizeof(pFilePath), pTestState->pTempDir, (size_t)-1, "a", (size_t)-1);

    result = fs_test_open_and_write_file(pTest, pTestState->pFS, pFilePath, FS_WRITE | FS_APPEND | FS_IGNORE_MOUNTS, dataToAppend, sizeof(dataToAppend));
    if (result != FS_SUCCESS) {
        return result;
    }


    /* Now we need to open the file and verify. */
    result = fs_test_open_and_read_file(pTest, pTestState->pFS, pFilePath, FS_READ, dataExpected, sizeof(dataExpected));
    if (result != FS_SUCCESS) {
        return result;
    }


    /*
    A detail with append mode is that it should not be able to make holes. To test this we'll seek
    beyond the end of the file and write out another 8 bytes, bringing the total length to 16. If
    the file is larger than this, it means it erroneously created a hole.
    */
    result = fs_file_open(pTestState->pFS, pFilePath, FS_WRITE | FS_APPEND | FS_IGNORE_MOUNTS, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file for writing.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_file_seek(pFile, 8, FS_SEEK_END);   /* <-- This should still succeed even though writes in append mode do not create holes. */
    if (result != FS_SUCCESS) {
        printf("%s: Failed to seek in file.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    result = fs_file_write(pFile, dataExpected, sizeof(dataExpected), &bytesWritten);
    if (result != FS_SUCCESS || bytesWritten != sizeof(dataExpected)) {
        printf("%s: Failed to write to file.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    fs_file_close(pFile);

    /* Verify that we did not get left with a hole. */
    result = fs_info(pTestState->pFS, pFilePath, FS_READ, &fileInfo);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to get file info.\n", pTest->name);
        return FS_ERROR;
    }

    if (fileInfo.size != 16) {
        printf("%s: ERROR: Expecting file size to be 16 bytes, but got %u.\n", pTest->name, (unsigned int)fileInfo.size);
        return FS_ERROR;
    }


    return FS_SUCCESS;
}
/* END system_write_append */

/* BEG system_write_exclusive */
int fs_test_system_write_exclusive(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_file* pFile;
    char pFilePathA[256];
    char pFilePathB[256];

    if (pTestState->pFS == NULL) {
        printf("%s: File system not initialized. Aborting test.\n", pTest->name);
        return FS_ERROR;
    }

    fs_path_append(pFilePathA, sizeof(pFilePathA), pTestState->pTempDir, (size_t)-1, "a", (size_t)-1);
    fs_path_append(pFilePathB, sizeof(pFilePathB), pTestState->pTempDir, (size_t)-1, "b", (size_t)-1);


    /* The first test we expect to fail because the file should already exist. This is the "a" file that we created from earlier tests. */
    result = fs_file_open(pTestState->pFS, pFilePathA, FS_WRITE | FS_EXCLUSIVE | FS_IGNORE_MOUNTS, &pFile);
    if (result == FS_SUCCESS) { /* <-- Detail: This must be "==" and not "!=". */
        printf("%s: Unexpected success opening file exclusively.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }


    /* The second test we expect to succeed. */
    result = fs_file_open(pTestState->pFS, pFilePathB, FS_WRITE | FS_EXCLUSIVE | FS_IGNORE_MOUNTS, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create new file.\n", pTest->name);
        return FS_ERROR;
    }

    fs_file_close(pFile);

    return FS_SUCCESS;
}
/* END system_write_exclusive */

/* BEG system_write_truncate */
int fs_test_system_write_truncate(fs_test* pTest)
{
    /*
    The truncate test is the same as the "new" test, with the only difference being that this test will
    be opening an existing file rather than creating a new one. This is functionally the same so we can
    just call straight into the "new" test rather than duplicating code here.
    */
    return fs_test_system_write_new(pTest);
}
/* END system_write_truncate */

/* BEG system_write_seek */
int fs_test_system_write_seek(fs_test* pTest)
{
    /*
    This test will open the "a" file in append mode, seek to the start, write out 6 bytes, and then
    verify the content. It should *not* overwrite the first two bytes. The final size should be 8
    bytes in length.

    Then we'll test that the three seeking origins all work as expected. These will be verified with
    fs_file_tell() which will also act as the test for pointer retrieval.
    */
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_file* pFile;
    char pFilePath[256];
    size_t bytesWritten;
    char data[4] = {5, 6, 7, 8};
    char dataExpected[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    fs_int64 cursor;

    fs_path_append(pFilePath, sizeof(pFilePath), pTestState->pTempDir, (size_t)-1, "a", (size_t)-1);

    result = fs_file_open(pTestState->pFS, pFilePath, FS_WRITE | FS_APPEND | FS_IGNORE_MOUNTS, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_file_seek(pFile, 0, FS_SEEK_SET);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to seek in file.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    result = fs_file_write(pFile, data, sizeof(data), &bytesWritten);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to write to file.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    if (bytesWritten != sizeof(data)) {
        printf("%s: ERROR: Expecting %d bytes written, but got %d.\n", pTest->name, (int)sizeof(data), (int)bytesWritten);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    fs_file_close(pFile);


    /* Now we need to open the file in read mode and verify the data was written correctly. */
    result = fs_test_open_and_read_file(pTest, pTestState->pFS, pFilePath, FS_READ, dataExpected, sizeof(dataExpected));
    if (result != FS_SUCCESS) {
        return result;
    }


    /* At this point the file should contain 8 bytes of data. Now we'll re-open it and test the different seek origins. */
    result = fs_file_open(pTestState->pFS, pFilePath, FS_WRITE | FS_APPEND | FS_IGNORE_MOUNTS, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file for reading.\n", pTest->name);
        return FS_ERROR;
    }

    /* SEEK_SET */
    result = fs_file_seek(pFile, 2, FS_SEEK_SET);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to seek in file.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    result = fs_file_tell(pFile, &cursor);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to tell file position.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    if (cursor != 2) {
        printf("%s: ERROR: Expecting cursor position 2, but got %d.\n", pTest->name, (int)cursor);
        fs_file_close(pFile);
        return FS_ERROR;
    }


    /* SEEK_CUR */
    result = fs_file_seek(pFile, 2, FS_SEEK_CUR);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to seek in file.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    result = fs_file_tell(pFile, &cursor);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to tell file position.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    if (cursor != 4) {
        printf("%s: ERROR: Expecting cursor position 4, but got %d.\n", pTest->name, (int)cursor);
        fs_file_close(pFile);
        return FS_ERROR;
    }


    /* SEEK_END */
    result = fs_file_seek(pFile, -8, FS_SEEK_END);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to seek in file.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    result = fs_file_tell(pFile, &cursor);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to tell file position.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    if (cursor != 0) {
        printf("%s: ERROR: Expecting cursor position 0, but got %d.\n", pTest->name, (int)cursor);
        fs_file_close(pFile);
        return FS_ERROR;
    }


    /* We must be able to seek beyond the end of the file without an error. */
    result = fs_file_seek(pFile, 16, FS_SEEK_END);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to seek in file.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    /* It is an error to seek to before the start file. */
    result = fs_file_seek(pFile, -1, FS_SEEK_SET);
    if (result == FS_SUCCESS) { /* <-- Detail: This must be "==" and not "!=". */
        printf("%s: ERROR: Seeking before start of file did not return an error.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }


    fs_file_close(pFile);

    return FS_SUCCESS;
}
/* END system_write_seek */

/* BEG system_write_truncate2 */
int fs_test_system_write_truncate2(fs_test* pTest)
{
    /*
    This tests the fs_file_truncate() function. We'll open the file "a", which should be 4 bytes in length
    at the time of running this test, and then truncate the last two bytes, leaving it 2 bytes in length.
    We'll then read the file back and verify the new size.

    A detail with this test. When running with POSIX, we can expect a FS_NOT_IMPLEMENTED when running in
    struct C89 mode (`-std=c89`) which is due to `ftruncate()` being unavailable.
    */
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_file* pFile;
    fs_file_info fileInfo;
    char pFilePath[256];
    char data[6] = {3, 4, 5, 6, 7, 8};

    fs_path_append(pFilePath, sizeof(pFilePath), pTestState->pTempDir, (size_t)-1, "a", (size_t)-1);

    result = fs_file_open(pTestState->pFS, pFilePath, FS_WRITE | FS_IGNORE_MOUNTS, &pFile); /* <-- Detail: Make sure this is opened in overwrite mode (FS_WRITE by itself). */
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create new file.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_file_seek(pFile, 2, FS_SEEK_SET);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to seek in file.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    result = fs_file_truncate(pFile);
    if (result != FS_SUCCESS) {
        

        /*
        If we get a FS_NOT_IMPLEMENTED, and the backend is POSIX, it means ftruncate() is not available
        internally. This is expected in certain build configurations, particularly when `-std=c89` is used
        without an explicitly defined feature macro, such as `_XOPEN_SOURCE >= 500`.
        */
        if (result == FS_NOT_IMPLEMENTED && pTestState->pBackend == FS_BACKEND_POSIX) {
            printf("%s: WARNING: ftruncate() is not available on this system. Skipping test.\n", pTest->name);
            fs_file_write(pFile, data, sizeof(data), NULL); /* <-- We need to write out the tail in order to set up the data for future tests. */
            fs_file_close(pFile);
            return FS_SUCCESS;
        } else {
            printf("%s: Failed to truncate file.\n", pTest->name);
            fs_file_close(pFile);
            return FS_ERROR;
        }
    }

    fs_file_close(pFile);


    /* Now get the info and check the size. We didn't modify any content so nothing should have changed. */
    result = fs_info(pTestState->pFS, pFilePath, FS_READ, &fileInfo);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to get file info.\n", pTest->name);
        return FS_ERROR;
    }

    if (fileInfo.size != 2) {
        printf("%s: Unexpected file size after truncation. Expected 2, got %u.\n", pTest->name, (unsigned int)fileInfo.size);
        return FS_ERROR;
    }

    /* We're now going to append another 6 bytes in preparation for the read tests. */
    {
        result = fs_file_open(pTestState->pFS, pFilePath, FS_WRITE | FS_APPEND | FS_IGNORE_MOUNTS, &pFile);
        if (result != FS_SUCCESS) {
            printf("%s: Failed to open file for writing.\n", pTest->name);
            return FS_ERROR;
        }

        result = fs_file_write(pFile, data, sizeof(data), NULL);
        if (result != FS_SUCCESS) {
            printf("%s: Failed to write to file.\n", pTest->name);
            fs_file_close(pFile);
            return FS_ERROR;
        }

        fs_file_close(pFile);
    }

    return FS_SUCCESS;
}
/* END system_write_truncate2 */

/* BEG system_write_flush */
int fs_test_system_write_flush(fs_test* pTest)
{
    /* This test doesn't actually do anything practical. It just verifies that the flush operation can be called without error. */
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_file* pFile = NULL;
    fs_result result;
    char pFilePath[256];

    fs_path_append(pFilePath, sizeof(pFilePath), pTestState->pTempDir, (size_t)-1, "a", (size_t)-1);

    result = fs_file_open(pTestState->pFS, pFilePath, FS_WRITE | FS_APPEND | FS_IGNORE_MOUNTS, &pFile); /* Using append here to be friendly with the stdio backend since it does not support overwrite mode and we don't want to be truncating this file. */
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file for writing.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_file_flush(pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to flush file.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    fs_file_close(pFile);

    return FS_SUCCESS;
}
/* END system_write_flush */

/* BEG system_read */
int fs_test_system_read(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_file* pFile = NULL;
    char pFilePath[256];
    char dataExpected[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    size_t dataRead[16];
    size_t bytesRead;

    fs_path_append(pFilePath, sizeof(pFilePath), pTestState->pTempDir, (size_t)-1, "a", (size_t)-1);

    result = fs_file_open(pTestState->pFS, pFilePath, FS_READ | FS_IGNORE_MOUNTS, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file for reading.\n", pTest->name);
        return FS_ERROR;
    }

    /*
    Read the file contents and verify. The file should be 8 bytes. We're going to try reading more than that
    and confirm that the bytes written is clamped appropriately and that it does not return FS_AT_END, since
    that return code should only be returned when zero bytes have been read.
    */
    result = fs_file_read(pFile, dataRead, sizeof(dataRead), &bytesRead);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to read file.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    if (bytesRead != sizeof(dataExpected)) {
        printf("%s: ERROR: Unexpected number of bytes read. Expected %d, got %d.\n", pTest->name, (int)sizeof(dataExpected), (int)bytesRead);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    if (memcmp(dataRead, dataExpected, sizeof(dataExpected)) != 0) {
        printf("%s: ERROR: Data read from file does not match expected data.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    /* Attempting to read again should now return FS_AT_END with a bytes read count of 0. */
    result = fs_file_read(pFile, dataRead, sizeof(dataRead), &bytesRead);
    if (result != FS_AT_END || bytesRead != 0) {
        printf("%s: ERROR: Expected FS_AT_END with 0 bytes read, got %d with %d bytes read.\n", pTest->name, result, (int)bytesRead);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    fs_file_close(pFile);

    return FS_SUCCESS;
}
/* END system_read */

/* BEG system_read_readonly */
int fs_test_system_read_readonly(fs_test* pTest)
{
    /* This test opens the file in read-only, and then attempts to write to the file. We should get an error. */
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_file* pFile = NULL;
    char pFilePath[256];
    char data[8] = {1, 2, 3, 4};
    size_t bytesWritten;
    
    fs_path_append(pFilePath, sizeof(pFilePath), pTestState->pTempDir, (size_t)-1, "a", (size_t)-1);

    result = fs_file_open(pTestState->pFS, pFilePath, FS_READ | FS_IGNORE_MOUNTS, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file for reading.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_file_write(pFile, data, sizeof(data), &bytesWritten);
    if (result == FS_SUCCESS) { /* <-- Detail: This must be "==" and not "!=". */
        printf("%s: ERROR: Unexpected success when writing to read-only file.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    /* Getting here means writing failed as expected, but double check that the bytes written is still zero. */
    if (bytesWritten != 0) {
        printf("%s: ERROR: Expecting 0 bytes written, but got %d.\n", pTest->name, (int)bytesWritten);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    fs_file_close(pFile);

    return FS_SUCCESS;
}
/* END system_read_readonly */

/* BEG system_read_noexist */
int fs_test_system_read_noexist(fs_test* pTest)
{
    /*
    In write mode there was once a bug that resulting in the library not failing gracefully. Since opening
    in write mode runs through a different code path as read mode, we'll test this as well.
    */
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_file* pFile = NULL;

    result = fs_file_open(pTestState->pFS, "does_not_exist", FS_READ | FS_IGNORE_MOUNTS, &pFile);
    if (result == FS_SUCCESS) { /* <-- Detail: This must be "==" and not "!=". */
        printf("%s: ERROR: Unexpected success opening non-existent file for reading.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END system_read_noexist */

/* BEG system_duplicate */
int fs_test_system_duplicate(fs_test* pTest)
{
    /*
    When duplicating a file, it must be a fully independent copy. That is, each instance must have it's
    own read/write pointers.
    */
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_file* pFile1 = NULL;
    fs_file* pFile2 = NULL;
    char pFilePath[256];
    char data1[2];
    char data2[2];

    fs_path_append(pFilePath, sizeof(pFilePath), pTestState->pTempDir, (size_t)-1, "a", (size_t)-1);

    result = fs_file_open(pTestState->pFS, pFilePath, FS_READ | FS_IGNORE_MOUNTS, &pFile1);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file 'a' for reading.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_file_duplicate(pFile1, &pFile2);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to duplicate file 'a'.\n", pTest->name);
        fs_file_close(pFile1);
        return FS_ERROR;
    }

    /* Now we have two file handles, pFile1 and pFile2. They should be independent. */
    fs_file_read(pFile1, data1, sizeof(data1), NULL);   /* Should advance the pointer of the first file, but not the second file... */
    fs_file_read(pFile2, data2, sizeof(data2), NULL);   /* Should still be reading from the start of the file since the first read should not have moved its pointer. */

    if (memcmp(data1, data2, sizeof(data1)) != 0) {
        printf("%s: ERROR: Duplicated file contents do not match.\n", pTest->name);
        fs_file_close(pFile1);
        fs_file_close(pFile2);
        return FS_ERROR;
    }

    fs_file_close(pFile1);
    fs_file_close(pFile2);

    return FS_SUCCESS;
}
/* END system_duplicate */

/* BEG system_rename */
int fs_test_system_rename(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    char pFilePathA[256];
    char pFilePathC[256];
    char pFilePathD[256];
    fs_file_info fileInfo;

    /* We're going to rename "a" to "c", and then verify with fs_info(). */
    fs_path_append(pFilePathA, sizeof(pFilePathA), pTestState->pTempDir, (size_t)-1, "a", (size_t)-1);
    fs_path_append(pFilePathC, sizeof(pFilePathC), pTestState->pTempDir, (size_t)-1, "c", (size_t)-1);

    result = fs_rename(pTestState->pFS, pFilePathA, pFilePathC, FS_IGNORE_MOUNTS);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to rename file.\n", pTest->name);
        return FS_ERROR;
    }

    /* "a" should no longer exist, and "c" should exist. */
    result = fs_info(pTestState->pFS, pFilePathA, FS_READ | FS_IGNORE_MOUNTS, &fileInfo);
    if (result == FS_SUCCESS) { /* <-- Detail: This must be "==" and not "!=". */
        printf("%s: ERROR: File 'a' still exists after rename.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_info(pTestState->pFS, pFilePathC, FS_READ | FS_IGNORE_MOUNTS, &fileInfo);
    if (result != FS_SUCCESS) {
        printf("%s: ERROR: File 'c' does not exist after rename.\n", pTest->name);
        return FS_ERROR;
    }

    /* Now we need to check if moving a file also works. We'll move it into a sub-directory. */
    fs_path_append(pFilePathD, sizeof(pFilePathD), pTestState->pTempDir, (size_t)-1, "dir1/a", (size_t)-1);

    result = fs_rename(pTestState->pFS, pFilePathC, pFilePathD, FS_IGNORE_MOUNTS);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to move file.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_info(pTestState->pFS, pFilePathD, FS_READ | FS_IGNORE_MOUNTS, &fileInfo);
    if (result != FS_SUCCESS) {
        printf("%s: ERROR: File 'dir1/a' does not exist after move.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_info(pTestState->pFS, pFilePathC, FS_READ | FS_IGNORE_MOUNTS, &fileInfo);
    if (result == FS_SUCCESS) { /* <-- Detail: This must be "==" and not "!=".*/
        printf("%s: ERROR: File 'c' still exists after move.\n", pTest->name);
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END system_rename */

/* BEG system_remove */
static fs_result fs_test_system_remove_directory(fs_test* pTest, const char* pDirPath)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_iterator* pIterator;
    
    for (pIterator = fs_first(pTestState->pFS, pDirPath, FS_IGNORE_MOUNTS | FS_OPAQUE); pIterator != NULL; pIterator = fs_next(pIterator)) {
        char pSubPath[256];
        fs_path_append(pSubPath, sizeof(pSubPath), pDirPath, (size_t)-1, pIterator->pName, pIterator->nameLen);

        if (pIterator->info.directory) {
            /* It's a directory. We need to remove it recursively. */
            result = fs_test_system_remove_directory(pTest, pSubPath);
            if (result != FS_SUCCESS) {
                printf("%s: Failed to remove directory '%s'.\n", pTest->name, pSubPath);
                fs_free_iterator(pIterator);
                return result;
            }
        } else {
            /* It's a file. Just delete it. */
            if (fs_remove(pTestState->pFS, pSubPath, FS_IGNORE_MOUNTS) != FS_SUCCESS) {
                printf("%s: Failed to remove file '%s'.\n", pTest->name, pSubPath);
                fs_free_iterator(pIterator);
                return FS_ERROR;
            }
        }
    }

    /* At this point the directory should be empty. */
    result = fs_remove(pTestState->pFS, pDirPath, FS_IGNORE_MOUNTS);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to remove directory '%s'.\n", pTest->name, pDirPath);
        return FS_ERROR;
    }

    return FS_SUCCESS;
}

int fs_test_system_remove(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;

    /*
    The first thing to test is that we cannot delete a non-empty folder. Our temp folder itself
    should have content so we can just try deleting that now.
    */
    result = fs_remove(pTestState->pFS, pTestState->pTempDir, FS_IGNORE_MOUNTS);
    if (result == FS_SUCCESS) { /* <-- Detail: This must be "==" and not "!=". */
        printf("%s: Unexpectedly succeeded in removing non-empty directory.\n", pTest->name);
        return FS_ERROR;
    }

    if (result != FS_DIRECTORY_NOT_EMPTY) {
        printf("%s: Unexpected error when removing non-empty directory.\n", pTest->name);
        return FS_ERROR;
    }

    /* Now we'll recursive delete everything in the temp folder. */
    return fs_test_system_remove_directory(pTest, pTestState->pTempDir);
}
/* END system_remove */

/* BEG system_uninit */
int fs_test_system_uninit(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;

    if (pTestState->pFS != NULL) {
        fs_uninit(pTestState->pFS);
        pTestState->pFS = NULL;
    }

    return FS_SUCCESS;
}
/* END system_uninit */


/* BEG mounts */
int fs_test_mounts(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_config fsConfig;
    char pRootPath[256];
    char pDir1Path[256];
    char pDir2Path[256];
    char pDir3Path[256];

    fsConfig = fs_config_init(pTestState->pBackend, NULL, NULL);

    result = fs_init(&fsConfig, &pTestState->pFS);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to initialize file system.\n", pTest->name);
        return FS_ERROR;
    }

    /* We will do all of our mounting tests in a temp folder, so create that now. */
    result = fs_mktmp("fs_mounts_", pTestState->pTempDir, sizeof(pTestState->pTempDir), FS_MKTMP_DIR);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create temp directory for mounting tests.\n", pTest->name);
        return FS_ERROR;
    }

    /*
    We're going to mount our temp directory as the default path. This will allow us to open
    a file like "a" without a prefix. This will be mounted for both read and write.
    */
    fs_path_append(pRootPath, sizeof(pRootPath), pTestState->pTempDir, (size_t)-1, "dir", (size_t)-1);

    result = fs_mount(pTestState->pFS, pRootPath, NULL, FS_READ | FS_WRITE);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to mount temp directory.\n", pTest->name);
        return FS_ERROR;
    }

    /*
    Now we want to test mounting the same directory. Previously we mounted to NULL, which is
    the equivalent to mounting an empty string. This is allowed, and should return success.
    Internally the mount just becomes a no-op.
    */
    result = fs_mount(pTestState->pFS, pRootPath, "", FS_READ | FS_WRITE);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to mount temp directory (empty string).\n", pTest->name);
        return FS_ERROR;
    }

    /* We'll create some sub-directories for later use. */
    fs_path_append(pDir1Path, sizeof(pDir1Path), pRootPath, (size_t)-1, "dir1", (size_t)-1);
    fs_path_append(pDir2Path, sizeof(pDir2Path), pRootPath, (size_t)-1, "dir2", (size_t)-1);
    fs_path_append(pDir3Path, sizeof(pDir3Path), pRootPath, (size_t)-1, "dir3", (size_t)-1);

    /*
    When we make these directory, do *not* use FS_IGNORE_MOUNTS. This way we can test that
    fs_mkdir() is indeed taking mounts into account. Do not use pDir1Path, etc. here.
    */
    result = fs_mkdir(pTestState->pFS, "dir1",  0);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create dir1.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_mkdir(pTestState->pFS, "dir2", 0);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create dir2.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_mkdir(pTestState->pFS, "dir3",  0);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create dir3.\n", pTest->name);
        return FS_ERROR;
    }

    /*
    We'll want to test mounts with a leading "/" which is used to prevent above-root navigation.
    We're going to use the sub-directory "dir1" for this test. We'll mount this twice - once
    with a leading "/" and once without.

    We're going to mount all three sub directories so we can test the priority system when 
    opening files. We'll use a variety of priority options to get decent coverage. The final
    priority for read mode will be the following:

        dir1 - highest
        dir2
        dir3 - lowest

    For write mode, only dir1 will be mounted.
    */
    result = fs_mount(pTestState->pFS, pDir2Path, "/inner", FS_READ); /* This will eventually end up being the middle priority. */
    if (result != FS_SUCCESS) {
        printf("%s: Failed to mount dir2 as /inner.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_mount(pTestState->pFS, pDir3Path, "/inner", FS_READ | FS_LOWEST_PRIORITY); /* This will eventually be the lowest priority. */
    if (result != FS_SUCCESS) {
        printf("%s: Failed to mount dir3 as /inner.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_mount(pTestState->pFS, pDir1Path, "/inner", FS_READ | FS_WRITE);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to mount dir1 as /inner.\n", pTest->name);
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END mounts */

/* BEG mounts_write */
static fs_result fs_test_mounts_write_file(fs_test* pTest, const char* pFilePath, const void* pData, size_t dataSize)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    
    return fs_test_open_and_write_file(pTest, pTestState->pFS, pFilePath, FS_WRITE | FS_TRUNCATE | FS_NO_CREATE_DIRS, pData, dataSize);
}

int fs_test_mounts_write(fs_test* pTest)
{
    /*
    This test will write out some files using mounts. We'll then verify they were actually created
    using the low-level API directly without mounts.

    The files output from this test will be used as inputs for the reading test.
    */
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_file* pFile;
    fs_file_info fileInfo;
    char pFilePath1[256];
    char pFilePath2[256];
    char pFilePath3[256];
    char data1[1] = {1};
    char data2[2] = {2};
    char data3[3] = {3};

    /* This one tests that writing to the correct "/inner" mount works as expected. It should end up in "{tmp}/dir1/a". */
    result = fs_test_mounts_write_file(pTest, "/inner/a", data1, sizeof(data1));
    if (result != FS_SUCCESS) {
        return FS_ERROR;
    }

    /* This one tests that we can write from the root write mount. */
    result = fs_test_mounts_write_file(pTest, "dir2/a", data2, sizeof(data2));
    if (result != FS_SUCCESS) {
        return FS_ERROR;
    }

    /* This one is for testing later in read mode. */
    result = fs_test_mounts_write_file(pTest, "dir3/a", data3, sizeof(data3));
    if (result != FS_SUCCESS) {
        return FS_ERROR;
    }

    /* Now we need to verify the files were created correctly, which we do using the low-level API without considering mounts. */
    fs_path_append(pFilePath1, sizeof(pFilePath1), pTestState->pTempDir, (size_t)-1, "dir/dir1/a", (size_t)-1);
    fs_path_append(pFilePath2, sizeof(pFilePath2), pTestState->pTempDir, (size_t)-1, "dir/dir2/a", (size_t)-1);
    fs_path_append(pFilePath3, sizeof(pFilePath3), pTestState->pTempDir, (size_t)-1, "dir/dir3/a", (size_t)-1);

    result = fs_info(pTestState->pFS, pFilePath1, FS_READ | FS_IGNORE_MOUNTS, &fileInfo);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to get info for %s.\n", pTest->name, pFilePath1);
        return FS_ERROR;
    }

    result = fs_info(pTestState->pFS, pFilePath2, FS_READ | FS_IGNORE_MOUNTS, &fileInfo);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to get info for %s.\n", pTest->name, pFilePath2);
        return FS_ERROR;
    }

    result = fs_info(pTestState->pFS, pFilePath3, FS_READ | FS_IGNORE_MOUNTS, &fileInfo);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to get info for %s.\n", pTest->name, pFilePath3);
        return FS_ERROR;
    }


    /* We'll now write out a few more files for testing later by the read tests. */
    result = fs_test_mounts_write_file(pTest, "dir2/b", data2, sizeof(data2));
    if (result != FS_SUCCESS) {
        return FS_ERROR;
    }

    result = fs_test_mounts_write_file(pTest, "dir3/b", data3, sizeof(data3));
    if (result != FS_SUCCESS) {
        return FS_ERROR;
    }

    result = fs_test_mounts_write_file(pTest, "dir3/c", data3, sizeof(data3));
    if (result != FS_SUCCESS) {
        return FS_ERROR;
    }

    /* Test that above-root navigation fails as expected. */
    result = fs_file_open(pTestState->pFS, "/inner/../dir2/a", FS_WRITE, &pFile);
    if (result == FS_SUCCESS) { /* <-- Detail: This must be "==" and not "!=". */
        printf("%s: Above-root navigation succeeded unexpectedly.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    result = fs_file_open(pTestState->pFS, "../a", FS_WRITE | FS_NO_ABOVE_ROOT_NAVIGATION, &pFile);
    if (result == FS_SUCCESS) {
        printf("%s: Above-root navigation succeeded unexpectedly.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    /* Test that writing above the root works when allowed. */
    result = fs_test_mounts_write_file(pTest, "../a", data1, sizeof(data1));
    if (result != FS_SUCCESS) {
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END mounts_write */

/* BEG mounts_read */
static fs_result fs_test_mounts_read_file(fs_test* pTest, const char* pFilePath, const void* pExpectedData, size_t expectedDataSize)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    
    return fs_test_open_and_read_file(pTest, pTestState->pFS, pFilePath, FS_READ, pExpectedData, expectedDataSize);
}

int fs_test_mounts_read(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_file* pFile;
    char pData1[1] = {1};
    char pData2[2] = {2};
    char pData3[3] = {3};

    /* First thing we're going to test is reading from specific sub-directories. This is our basic test. */
    result = fs_test_mounts_read_file(pTest, "dir1/a", pData1, sizeof(pData1));
    if (result != FS_SUCCESS) {
        return FS_ERROR;
    }

    result = fs_test_mounts_read_file(pTest, "dir2/a", pData2, sizeof(pData2));
    if (result != FS_SUCCESS) {
        return FS_ERROR;
    }

    result = fs_test_mounts_read_file(pTest, "dir3/a", pData3, sizeof(pData3));
    if (result != FS_SUCCESS) {
        return FS_ERROR;
    }

    /* Now we want to read from the "/inner" mount. This tests our priority system. It should always be the same file as dir1/a since that is the highest priority. */
    result = fs_test_mounts_read_file(pTest, "/inner/a", pData1, sizeof(pData1));
    if (result != FS_SUCCESS) {
        return FS_ERROR;
    }

    /* The "b" file only exist in "dir2" and "dir3". "dir2" is higher priority that "dir3", so that is the one we should get. */
    result = fs_test_mounts_read_file(pTest, "/inner/b", pData2, sizeof(pData2));
    if (result != FS_SUCCESS) {
        return FS_ERROR;
    }

    /* The "c" file only exists in "dir3". */
    result = fs_test_mounts_read_file(pTest, "/inner/c", pData3, sizeof(pData3));
    if (result != FS_SUCCESS) {
        return FS_ERROR;
    }

    /* Here we are testing that trying to open navigate above the root fails. */
    result = fs_file_open(pTestState->pFS, "/inner/../dir2/a", FS_READ, &pFile);
    if (result == FS_SUCCESS) { /* <-- Detail: This must be "==" and not "!=". */
        printf("%s: Unexpectedly succeeded in reading /inner/../dir2/a.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    result = fs_file_open(pTestState->pFS, "../a", FS_READ | FS_NO_ABOVE_ROOT_NAVIGATION, &pFile);
    if (result == FS_SUCCESS) { /* <-- Detail: This must be "==" and not "!=". */
        printf("%s: Unexpectedly succeeded in reading ../a.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    /* This tests that above root navigation, when allowed, works correctly. Loading from "../a" should work. */
    result = fs_test_mounts_read_file(pTest, "../a", pData1, sizeof(pData1));
    if (result != FS_SUCCESS) {
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END mounts_read */

/* BEG mounts_mkdir */
int fs_test_mounts_mkdir(fs_test* pTest)
{
    /*
    The "mounts" test set up some directories with fs_mkdir() for future tests, but we need to test some
    more complex scenarios as well, such as restricting navigation above mount points.

    The directories created here will be deleted by a later test.
    */
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    
    /* First check that basic directory creation works. */
    result = fs_mkdir(pTestState->pFS, "testdir", 0);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create testdir.\n", pTest->name);
        return FS_ERROR;
    }

    /* Check that FS_ALREADY_EXISTS is returned for a directory that already exists. */
    result = fs_mkdir(pTestState->pFS, "testdir", 0);
    if (result != FS_ALREADY_EXISTS) {
        printf("%s: ERROR: Expected FS_ALREADY_EXISTS, but got %d\n", pTest->name, result);
        return FS_ERROR;
    }

    /* Check that recursively creating a directory fails when FS_NO_CREATE_DIRS is specified. */
    result = fs_mkdir(pTestState->pFS, "testdir/a/b/c", FS_NO_CREATE_DIRS);
    if (result == FS_SUCCESS) { /* <-- Detail: This must be "==" and not "!=". */
        printf("%s: Unexpected success when creating testdir/a/b/c with FS_NO_CREATE_DIRS.\n", pTest->name);
        return FS_ERROR;
    }

    /* Now check that recursively creating a directory works when FS_NO_CREATE_DIRS is not specified. */
    result = fs_mkdir(pTestState->pFS, "testdir/a/b/c", 0);
    if (result != FS_SUCCESS) {
        printf("%s: ERROR: Expected FS_SUCCESS, but got %d\n", pTest->name, result);
        return FS_ERROR;
    }

    /* Check that above-root navigation results in an error if disallowed. */
    result = fs_mkdir(pTestState->pFS, "/inner/../dir2/subdir", 0);
    if (result == FS_SUCCESS) { /* <-- Detail: This must be "==" and not "!=". */
        printf("%s: Unexpected success when creating /inner/../dir2/subdir.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_mkdir(pTestState->pFS, "../subdir", FS_NO_ABOVE_ROOT_NAVIGATION);
    if (result == FS_SUCCESS) { /* <-- Detail: This must be "==" and not "!=". */
        printf("%s: Unexpected success when creating ../subdir.\n", pTest->name);
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END mounts_mkdir */

/* BEG mounts_rename */
int fs_test_mounts_rename(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_file* pFile;
    fs_file_info fileInfo;

    /* First create a couple of test files. */
    result = fs_file_open(pTestState->pFS, "rename/oldname1", FS_WRITE, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create oldname1.\n", pTest->name);
        return FS_ERROR;
    }

    fs_file_close(pFile);


    result = fs_file_open(pTestState->pFS, "rename/oldname2", FS_WRITE, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create oldname2.\n", pTest->name);
        return FS_ERROR;
    }

    fs_file_close(pFile);


    /*
    Unfortunately the behavior of renaming a file to an existing one is not well defined between
    backends. I'm going to skip this test.
    */
    #if 0
    /* test that renaming the file to an existing one fails as expected. */
    result = fs_rename(pTestState->pFS, "rename/oldname1", "rename/oldname2", 0);
    if (result == FS_SUCCESS) { /* <-- Detail: This must be "==" and not "!=". */
        printf("%s: Unexpected success when renaming oldname1 to oldname2.\n", pTest->name);
        return FS_ERROR;
    }
    #endif


    /* Test a normal in-place rename (no moving) and confirm it actually exists. */
    result = fs_rename(pTestState->pFS, "rename/oldname1", "rename/newname", 0);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to rename oldname1 to newname.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_info(pTestState->pFS, "rename/newname", FS_READ, &fileInfo);
    if (result != FS_SUCCESS) {
        printf("%s: newname does not exist.\n", pTest->name);
        return FS_ERROR;
    }


    /* Test that a move (rename across directories) works. */
    fs_mkdir(pTestState->pFS, "rename/dir1", 0);

    result = fs_rename(pTestState->pFS, "rename/newname", "rename/dir1/newname", 0);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to move newname to dir1/newname.\n", pTest->name);
        return FS_ERROR;
    }


    /* Test that above-root navigation fails as expected. */
    result = fs_rename(pTestState->pFS, "rename/dir1/newname", "/inner/../dir2/a", 0);
    if (result == FS_SUCCESS) { /* <-- Detail: This must be "==" and not "!=". */
        printf("%s: Unexpected success when removing non-existent file.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_rename(pTestState->pFS, "rename/dir1/newname", "../rename/oldname2", FS_NO_ABOVE_ROOT_NAVIGATION);
    if (result == FS_SUCCESS) { /* <-- Detail: This must be "==" and not "!=". */
        printf("%s: Unexpected success when removing non-existent file.\n", pTest->name);
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END mounts_rename */

/* BEG mounts_remove */
int fs_test_mounts_remove(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_file* pFile;

    /* First create a test file. */
    result = fs_file_open(pTestState->pFS, "remove/file", FS_WRITE, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create file.\n", pTest->name);
        return FS_ERROR;
    }

    fs_file_close(pFile);

    /* Test removing a non-existent file. */
    result = fs_remove(pTestState->pFS, "remove/does_not_exist", 0);
    if (result == FS_SUCCESS) { /* <-- Detail: This must be "==" and not "!=". */
        printf("%s: Unexpected success when removing non-existent file.\n", pTest->name);
        return FS_ERROR;
    }

    /* Test removing a non-empty directory. */
    result = fs_remove(pTestState->pFS, "remove", 0);
    if (result == FS_SUCCESS) { /* <-- Detail: This must be "==" and not "!=". */
        printf("%s: Unexpected success when removing non-empty directory.\n", pTest->name);
        return FS_ERROR;
    }

    /* Test removing a file. */
    result = fs_remove(pTestState->pFS, "remove/file", 0);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to remove file.\n", pTest->name);
        return FS_ERROR;
    }

    /* Test removing an empty directory. */
    result = fs_remove(pTestState->pFS, "remove", 0);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to remove directory.\n", pTest->name);
        return FS_ERROR;
    }

    /* Test that above-root navigation fails as expected. */
    result = fs_remove(pTestState->pFS, "/inner/../dir2/a", 0);
    if (result == FS_SUCCESS) { /* <-- Detail: This must be "==" and not "!=". */
        printf("%s: Unexpected success when removing non-existent file.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_remove(pTestState->pFS, "../rename/oldname2", FS_NO_ABOVE_ROOT_NAVIGATION);
    if (result == FS_SUCCESS) { /* <-- Detail: This must be "==" and not "!=". */
        printf("%s: Unexpected success when removing non-existent file.\n", pTest->name);
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END mounts_remove */

/* BEG mounts_iteration */
int fs_test_mounts_iteration(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_iterator* pIterator;
    
    /* Set up a sub-folder with some files for testing. */
    result = fs_mkdir(pTestState->pFS, "iteration/dir1", 0);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create directory.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_mkdir(pTestState->pFS, "iteration/dir2", 0);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create directory.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_test_open_and_write_file(pTest, pTestState->pFS, "iteration/file1", FS_WRITE, NULL, 0);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create file.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_test_open_and_write_file(pTest, pTestState->pFS, "iteration/file2", FS_WRITE, NULL, 0);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create file.\n", pTest->name);
        return FS_ERROR;
    }

    /* TODO: Expand on this test once we have archive tests written and the iteration API has been improved. */
    pIterator = fs_first(pTestState->pFS, "iteration", FS_READ | FS_OPAQUE);    /* <-- Use opaque here to ignore the archive code paths. That will be tested later in the archive tests. */
    if (pIterator == NULL) {
        printf("%s: Failed to create iterator.\n", pTest->name);
        return FS_ERROR;
    }

    while (pIterator != NULL) {
        /* The iterator needs to be one of the expected names. */
        const char* pExpectedNames[] = {
            "file1",
            "file2",
            "dir1",
            "dir2"
        };
        size_t i;
        fs_bool32 found = FS_FALSE;

        for (i = 0; i < FS_COUNTOF(pExpectedNames); i += 1) {
            if (fs_strncmp(pExpectedNames[i], pIterator->pName, pIterator->nameLen) == 0) {
                found = FS_TRUE;
                break;
            }
        }

        if (!found) {
            printf("%s: Unexpected file found: %.*s\n", pTest->name, (int)pIterator->nameLen, pIterator->pName);
            return FS_ERROR;
        }

        /* Make sure the files and folders are correctly identified as such. */
        if (pIterator->info.directory) {
            if (pIterator->pName[0] != 'd' || pIterator->pName[1] != 'i' || pIterator->pName[2] != 'r') {
                printf("%s: Directory incorrectly identified: %.*s\n", pTest->name, (int)pIterator->nameLen, pIterator->pName);
                return FS_ERROR;
            }
        } else {
            if (pIterator->pName[0] != 'f' || pIterator->pName[1] != 'i' || pIterator->pName[2] != 'l' || pIterator->pName[3] != 'e') {
                printf("%s: File incorrectly identified: %.*s\n", pTest->name, (int)pIterator->nameLen, pIterator->pName);
                return FS_ERROR;
            }
        }

        pIterator = fs_next(pIterator);
    }

    /* TODO: Do an iteration tests against write mounts and FS_WRITE | FS_OPAQUE. */

    return FS_SUCCESS;
}
/* END mounts_iteration */

/* BEG unmount */
int fs_test_unmount(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    char pDir1Path[256];
    char pDir2Path[256];
    char pDir3Path[256];

    fs_path_append(pDir1Path, sizeof(pDir1Path), pTestState->pTempDir, (size_t)-1, "dir1", (size_t)-1);
    fs_path_append(pDir2Path, sizeof(pDir2Path), pTestState->pTempDir, (size_t)-1, "dir2", (size_t)-1);
    fs_path_append(pDir3Path, sizeof(pDir3Path), pTestState->pTempDir, (size_t)-1, "dir3", (size_t)-1);

    result = fs_unmount(pTestState->pFS, pDir1Path, FS_READ | FS_WRITE);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to unmount %s.\n", pTest->name, pDir1Path);
        return FS_ERROR;
    }

    result = fs_unmount(pTestState->pFS, pDir2Path, FS_READ);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to unmount %s.\n", pTest->name, pDir2Path);
        return FS_ERROR;
    }

    result = fs_unmount(pTestState->pFS, pDir3Path, FS_READ);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to unmount %s.\n", pTest->name, pDir3Path);
        return FS_ERROR;
    }

    result = fs_unmount(pTestState->pFS, pTestState->pTempDir, FS_READ | FS_WRITE);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to unmount %s.\n", pTest->name, pTestState->pTempDir);
        return FS_ERROR;
    }

    /* Finally we need to delete the temp folder recursively to clean everything up. */
    result = fs_test_system_remove_directory(pTest, pTestState->pTempDir);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to remove temp directory.\n", pTest->name);
        return FS_ERROR;
    }

    /* We're done with the file system. */
    fs_uninit(pTestState->pFS);
    pTestState->pFS = NULL;

    return FS_SUCCESS;
}
/* END unmount */

/* BEG archives */
int fs_test_archives(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_config config;
    fs_archive_type pArchiveTypes[2];
    char pRootPath[256];
    char dataA[] = { 1, 2, 3, 4 };

    pArchiveTypes[0] = fs_archive_type_init(FS_ZIP, "zip");
    pArchiveTypes[1] = fs_archive_type_init(FS_PAK, "pak");

    config = fs_config_init(pTestState->pBackend, NULL, NULL);
    config.pArchiveTypes    = pArchiveTypes;
    config.archiveTypeCount = FS_COUNTOF(pArchiveTypes);

    result = fs_init(&config, &pTestState->pFS);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to initialize file system.\n", pTest->name);
        return FS_ERROR;
    }

    /* Setup the root folder for the archive tests. */
    result = fs_mktmp("fs_archives_", pTestState->pTempDir, sizeof(pTestState->pTempDir), FS_MKTMP_DIR);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create temp directory for archive tests.\n", pTest->name);
        return FS_ERROR;
    }

    /* We'll mount the temp directory as our root directory. */
    fs_path_append(pRootPath, sizeof(pRootPath), pTestState->pTempDir, (size_t)-1, "root", (size_t)-1);

    result = fs_mount(pTestState->pFS, pRootPath, "", FS_READ | FS_WRITE);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to mount temp directory.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_mount(pTestState->pFS, pRootPath, "/", FS_READ | FS_WRITE);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to mount temp directory as root.\n", pTest->name);
        return FS_ERROR;
    }

    /* Output our test archives into the root folder for later use. */
    result = fs_test_open_and_write_file(pTest, pTestState->pFS, "test1.zip", FS_WRITE, fs_test_file_test1_zip, sizeof(fs_test_file_test1_zip));
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create test1.zip.\n", pTest->name);
        return FS_ERROR;
    }

    /*
    We want some files side-by-side with the archive, with the same name as those inside the archive itself. This is for
    testing that the correct file is loaded in transparent mode (tested later).
    */
    result = fs_test_open_and_write_file(pTest, pTestState->pFS, "a", FS_WRITE, dataA, sizeof(dataA));
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create file 'a'.\n", pTest->name);
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END archives */

/* BEG archives_opaque */
int fs_test_archives_opaque(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_file* pFile;

    /* Test that attempting to open a file inside an archive in opaque mode fails. */
    result = fs_file_open(pTestState->pFS, "test1.zip/b", FS_READ | FS_OPAQUE, &pFile);
    if (result == FS_SUCCESS) {
        printf("%s: Unexpected success when opening file inside archive in opaque mode.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    /* Same test, this time with a transparent path (no explicit mention of the archive) */
    result = fs_file_open(pTestState->pFS, "b", FS_READ | FS_OPAQUE, &pFile);
    if (result == FS_SUCCESS) {
        printf("%s: Unexpected success when opening file inside archive in opaque mode.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END archives_opaque */

/* BEG archives_verbose */
int fs_test_archives_verbose(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_file* pFile;

    /* Test that we can successfully open a file inside the archive with an explicit (verbose) path. */
    result = fs_file_open(pTestState->pFS, "test1.zip/b", FS_READ | FS_VERBOSE, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file inside archive with verbose path.\n", pTest->name);
        return FS_ERROR;
    }

    fs_file_close(pFile);


    /* Same test, this time with a transparent path (no explicit mention of the archive). This should fail. */
    result = fs_file_open(pTestState->pFS, "b", FS_READ | FS_VERBOSE, &pFile);
    if (result == FS_SUCCESS) {
        printf("%s: Unexpected success when opening file inside archive in verbose mode.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END archives_verbose */

/* BEG archives_transparent */
int fs_test_archives_transparent(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_file* pFile;

    /* Test that we can successfully open a file inside the archive with an explicit (verbose) path. */
    result = fs_file_open(pTestState->pFS, "test1.zip/b", FS_READ | FS_TRANSPARENT, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file inside archive with verbose path.\n", pTest->name);
        return FS_ERROR;
    }

    fs_file_close(pFile);


    /* Same test, this time with a transparent path (no explicit mention of the archive). */
    result = fs_file_open(pTestState->pFS, "b", FS_READ | FS_TRANSPARENT, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file inside archive with transparent path.\n", pTest->name);
        return FS_ERROR;
    }

    fs_file_close(pFile);


    /*
    Opening files must prioritize the normal file system before archives. In our set up we have a file called "a"
    in both the archive and the normal file system. On the normal file system it is 4 bytes, whereas the one in
    the archive is only 1 bytes. We'll now check this.
    */
    {
        char pExpectedDataA[] = { 1, 2, 3, 4 };
        
        result = fs_test_open_and_read_file(pTest, pTestState->pFS, "a", FS_READ | FS_TRANSPARENT, pExpectedDataA, sizeof(pExpectedDataA));
        if (result != FS_SUCCESS) {
            return result;
        }
    }

    return FS_SUCCESS;
}
/* END archives_transparent */

/* BEG archives_mount */
int fs_test_archives_mount(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_file* pFile;
    char pActualPath[256];

    /* Test that we can mount an archive directly by it's path. */
    fs_path_append(pActualPath, sizeof(pActualPath), pTestState->pTempDir, (size_t)-1, "root/test1.zip", (size_t)-1);

    result = fs_mount(pTestState->pFS, pActualPath, "archive", FS_READ);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to mount archive.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_file_open(pTestState->pFS, "archive/b", FS_READ, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file inside mounted archive.\n", pTest->name);
        return FS_ERROR;
    }

    fs_file_close(pFile);

    result = fs_unmount(pTestState->pFS, pActualPath, FS_READ);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to unmount archive.\n", pTest->name);
        return FS_ERROR;
    }

    /* TODO: Consider adding support for this. */
    #if 0
    /* Test that we can mount a sub-directory inside an archive. */
    fs_path_append(pActualPath, sizeof(pActualPath), pTestState->pTempDir, (size_t)-1, "root/test1.zip/dir1", (size_t)-1);

    result = fs_mount(pTestState->pFS, pActualPath, "archive", FS_READ);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to mount sub-directory inside archive.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_file_open(pTestState->pFS, "archive/c", FS_READ, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file inside mounted archive.\n", pTest->name);
        return FS_ERROR;
    }

    fs_file_close(pFile);

    result = fs_unmount(pTestState->pFS, pActualPath, FS_READ);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to unmount archive.\n", pTest->name);
        return FS_ERROR;
    }
    #endif

    /* Test that attempting to mount an archive for writing fails as expected (write mode for archives is not supported). */
    result = fs_mount(pTestState->pFS, pActualPath, "archive", FS_WRITE);
    if (result == FS_SUCCESS) {
        printf("%s: Unexpected success when mounting archive for writing.\n", pTest->name);
        return FS_ERROR;
    }

    /* Test that attempting to mount a path for reading fails if the actual path does not exist. */
    result = fs_mount(pTestState->pFS, "does_not_exist", "should_fail", FS_READ);
    if (result == FS_SUCCESS) {
        printf("%s: Unexpected success when mounting non-existent path for reading.\n", pTest->name);
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END archives_mount */

/* BEG archives_iteration */
fs_result fs_test_archives_iteration_basic_test(fs_test* pTest, const char* pPath, int mode, const char** pExpectedNames, size_t expectedNameCount)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_iterator* pIterator;
    size_t i;
    size_t iterationCount = 0;

    pIterator = fs_first(pTestState->pFS, pPath, mode);
    if (pIterator == NULL) {
        printf("%s: Failed to create iterator for path: %s\n", pTest->name, pPath);
        return FS_ERROR;
    }

    while (pIterator != NULL) {
        fs_bool32 found = FS_FALSE;

        iterationCount += 1;

        for (i = 0; i < expectedNameCount; i += 1) {
            if (fs_strncmp(pExpectedNames[i], pIterator->pName, pIterator->nameLen) == 0) {
                found = FS_TRUE;
                break;
            }
        }

        if (!found) {
            printf("%s: Unexpected file found: %.*s\n", pTest->name, (int)pIterator->nameLen, pIterator->pName);
            return FS_ERROR;
        }

        pIterator = fs_next(pIterator);
    }

    if (iterationCount < expectedNameCount) {
        printf("%s: Not all expected files were found.\n", pTest->name);
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END archives_iteration */

/* BEG archives_iteration_opaque */
int fs_test_archives_iteration_opaque(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_iterator* pIterator;
    const char* pExpectedNames[] = {
        "a",
        "test1.zip"
    };

    /* First a basic opaque iteration. This should not scan inside archives and should not allow us to iterate over directories inside archives. */
    result = fs_test_archives_iteration_basic_test(pTest, "/", FS_READ | FS_OPAQUE | FS_ONLY_MOUNTS, pExpectedNames, FS_COUNTOF(pExpectedNames));
    if (result != FS_SUCCESS) {
        return FS_ERROR;
    }

    result = fs_test_archives_iteration_basic_test(pTest, "/", FS_WRITE | FS_OPAQUE | FS_ONLY_MOUNTS, pExpectedNames, FS_COUNTOF(pExpectedNames));
    if (result != FS_SUCCESS) {
        return FS_ERROR;
    }

    /* Test that attempting to iterate over the contents of an archive fails in opaque mode. */
    pIterator = fs_first(pTestState->pFS, "/test1.zip", FS_READ | FS_OPAQUE | FS_ONLY_MOUNTS);
    if (pIterator != NULL) {
        printf("%s: Unexpected success when iterating over archive contents.\n", pTest->name);
        fs_free_iterator(pIterator);
        return FS_ERROR;
    }

    pIterator = fs_first(pTestState->pFS, "/test1.zip", FS_WRITE | FS_OPAQUE | FS_ONLY_MOUNTS);
    if (pIterator != NULL) {
        printf("%s: Unexpected success when iterating over archive contents.\n", pTest->name);
        fs_free_iterator(pIterator);
        return FS_ERROR;
    }

    /* Test that attempting to iterate over a directory inside an archive fails in opaque mode. */
    pIterator = fs_first(pTestState->pFS, "/test1.zip/dir1", FS_READ | FS_OPAQUE | FS_ONLY_MOUNTS);
    if (pIterator != NULL) {
        printf("%s: Unexpected success when iterating over directory inside archive.\n", pTest->name);
        fs_free_iterator(pIterator);
        return FS_ERROR;
    }

    pIterator = fs_first(pTestState->pFS, "/test1.zip/dir1", FS_WRITE | FS_OPAQUE | FS_ONLY_MOUNTS);
    if (pIterator != NULL) {
        printf("%s: Unexpected success when iterating over directory inside archive.\n", pTest->name);
        fs_free_iterator(pIterator);
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END archives_iteration_opaque */

/* BEG archives_iteration_verbose */
int fs_test_archives_iteration_verbose(fs_test* pTest)
{
    /*fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;*/
    fs_result result;

    /* For verbose iteration we should get the same results as opaque mode when iterating over the root directory. */
    {
        const char* pExpectedNames[] = {
            "a",
            "test1.zip"
        };

        result = fs_test_archives_iteration_basic_test(pTest, "/", FS_READ | FS_VERBOSE | FS_ONLY_MOUNTS, pExpectedNames, FS_COUNTOF(pExpectedNames));
        if (result != FS_SUCCESS) {
            return FS_ERROR;
        }

        result = fs_test_archives_iteration_basic_test(pTest, "/", FS_WRITE | FS_VERBOSE | FS_ONLY_MOUNTS, pExpectedNames, FS_COUNTOF(pExpectedNames));
        if (result != FS_SUCCESS) {
            return FS_ERROR;
        }
    }

    /* We should be able to pass a path to an archive directly and iterate its contents. */
    {
        const char* pExpectedNames[] = {
            "a",
            "b",
            "dir1"
        };

        result = fs_test_archives_iteration_basic_test(pTest, "/test1.zip", FS_READ | FS_VERBOSE | FS_ONLY_MOUNTS, pExpectedNames, FS_COUNTOF(pExpectedNames));
        if (result != FS_SUCCESS) {
            return FS_ERROR;
        }

        result = fs_test_archives_iteration_basic_test(pTest, "/test1.zip", FS_WRITE | FS_VERBOSE | FS_ONLY_MOUNTS, pExpectedNames, FS_COUNTOF(pExpectedNames));
        if (result != FS_SUCCESS) {
            return FS_ERROR;
        }
    }

    /* We should also be able to pass in a sub-directory of an archive. */
    {
        const char* pExpectedNamesp[] = { "a", "b", "c", "d" };

        result = fs_test_archives_iteration_basic_test(pTest, "/test1.zip/dir1", FS_READ | FS_VERBOSE | FS_ONLY_MOUNTS, pExpectedNamesp, FS_COUNTOF(pExpectedNamesp));
        if (result != FS_SUCCESS) {
            return FS_ERROR;
        }

        result = fs_test_archives_iteration_basic_test(pTest, "/test1.zip/dir1", FS_WRITE | FS_VERBOSE | FS_ONLY_MOUNTS, pExpectedNamesp, FS_COUNTOF(pExpectedNamesp));
        if (result != FS_SUCCESS) {
            return FS_ERROR;
        }
    }

    return FS_SUCCESS;
}
/* END archives_iteration_verbose */

/* BEG archives_iteration_transparent */
int fs_test_archives_iteration_transparent(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;

    /*
    Transparent should prioritize files from outside archives. It should also still include the archive itself in the iteration in case the application
    needs explicit knowledge of it.
    */
    {
        fs_iterator* pIterator;
        size_t i;
        size_t iterationCount = 0;
        const char* pExpectedNames[] = {
            "a",        /* <-- This should be from the normal file system and not the archive. We test this by looking at the size in the info. In the archive it is 1 byte, whereas on the normal file system it is 4 bytes. */
            "b",        /* <-- From inside the archive. */
            "dir1",     /* <-- From inside the archive. */
            "test1.zip" /* <-- The archive itself should still be included in the iteration. */
        };

        pIterator = fs_first(pTestState->pFS, "/", FS_READ | FS_TRANSPARENT | FS_ONLY_MOUNTS);
        if (pIterator == NULL) {
            printf("%s: Failed to create iterator for path: %s\n", pTest->name, "/");
            return FS_ERROR;
        }

        while (pIterator != NULL) {
            fs_bool32 found = FS_FALSE;

            iterationCount += 1;

            for (i = 0; i < FS_COUNTOF(pExpectedNames); i += 1) {
                if (fs_strncmp(pExpectedNames[i], pIterator->pName, pIterator->nameLen) == 0) {
                    found = FS_TRUE;

                    /* For the "a" file, we need to check that it picked the file from the file sytem and not the archive. */
                    if (fs_strncmp("a", pIterator->pName, pIterator->nameLen) == 0) {
                        if (pIterator->info.size != 4) {
                            printf("%s: Found archive version of file: \"%.*s\". Expecting file system version.\n", pTest->name, (int)pIterator->nameLen, pIterator->pName);
                        }
                    }

                    break;
                }
            }

            if (!found) {
                printf("%s: Unexpected file found: %.*s\n", pTest->name, (int)pIterator->nameLen, pIterator->pName);
                return FS_ERROR;
            }

            pIterator = fs_next(pIterator);
        }

        if (iterationCount < FS_COUNTOF(pExpectedNames)) {
            printf("%s: Not all expected files were found.\n", pTest->name);
            return FS_ERROR;
        }
    }

    /* We should be able to pass a path to an archive directly and iterate its contents. */
    {
        const char* pExpectedNames[] = {
            "a",
            "b",
            "dir1"
        };

        result = fs_test_archives_iteration_basic_test(pTest, "/test1.zip", FS_READ | FS_TRANSPARENT | FS_ONLY_MOUNTS, pExpectedNames, FS_COUNTOF(pExpectedNames));
        if (result != FS_SUCCESS) {
            return FS_ERROR;
        }

        result = fs_test_archives_iteration_basic_test(pTest, "/test1.zip", FS_WRITE | FS_TRANSPARENT | FS_ONLY_MOUNTS, pExpectedNames, FS_COUNTOF(pExpectedNames));
        if (result != FS_SUCCESS) {
            return FS_ERROR;
        }
    }

    /* We should also be able to pass in a sub-directory of an archive. */
    {
        const char* pExpectedNamesp[] = { "a", "b", "c", "d" };

        result = fs_test_archives_iteration_basic_test(pTest, "/test1.zip/dir1", FS_READ | FS_TRANSPARENT | FS_ONLY_MOUNTS, pExpectedNamesp, FS_COUNTOF(pExpectedNamesp));
        if (result != FS_SUCCESS) {
            return FS_ERROR;
        }

        result = fs_test_archives_iteration_basic_test(pTest, "/test1.zip/dir1", FS_WRITE | FS_TRANSPARENT | FS_ONLY_MOUNTS, pExpectedNamesp, FS_COUNTOF(pExpectedNamesp));
        if (result != FS_SUCCESS) {
            return FS_ERROR;
        }
    }

    return FS_SUCCESS;
}
/* END archives_iteration_transparent */

/* BEG archives_recursive */
int fs_test_archives_recursive(fs_test* pTest)
{
    /* This tests archives inside archives. */
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;

    /* We need to write out our recursive archive. */
    result = fs_test_open_and_write_file(pTest, pTestState->pFS, "test2.zip", FS_WRITE, fs_test_file_test2_zip, sizeof(fs_test_file_test2_zip));
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create test2.zip.\n", pTest->name);
        return FS_ERROR;
    }

    /* Basic read test. */
    {
        const char pExpectedDataA[] = { 'a' };

        /* Opaque. Should fail. */
        result = fs_file_open(pTestState->pFS, "/test2.zip/archives/test1.zip/a", FS_READ | FS_OPAQUE, NULL);
        if (result == FS_SUCCESS) {
            printf("%s: Unexpected success when opening file inside nested archive in opaque mode.\n", pTest->name);
            return FS_ERROR;
        }


        /* Verbose. */
        result = fs_test_open_and_read_file(pTest, pTestState->pFS, "/test2.zip/archives/test1.zip/a", FS_READ | FS_VERBOSE, pExpectedDataA, sizeof(pExpectedDataA));
        if (result != FS_SUCCESS) {
            return FS_ERROR;
        }

        result = fs_file_open(pTestState->pFS, "/test2.zip/archives/a", FS_READ | FS_VERBOSE, NULL);
        if (result == FS_SUCCESS) {
            printf("%s: Unexpected success when opening file inside nested archive with incomplete path in verbose mode.\n", pTest->name);
            return FS_ERROR;
        }


        /* Transparent. */
        result = fs_test_open_and_read_file(pTest, pTestState->pFS, "/test2.zip/archives/test1.zip/a", FS_READ | FS_TRANSPARENT, pExpectedDataA, sizeof(pExpectedDataA));
        if (result != FS_SUCCESS) {
            return FS_ERROR;
        }

        result = fs_test_open_and_read_file(pTest, pTestState->pFS, "/test2.zip/archives/a", FS_READ | FS_TRANSPARENT, pExpectedDataA, sizeof(pExpectedDataA));
        if (result != FS_SUCCESS) {
            return FS_ERROR;
        }

        result = fs_test_open_and_read_file(pTest, pTestState->pFS, "/archives/a", FS_READ | FS_TRANSPARENT, pExpectedDataA, sizeof(pExpectedDataA));
        if (result != FS_SUCCESS) {
            return FS_ERROR;
        }
    }

    /* Basic iteration test. */
    {
        /* Opaque. Should fail. */
        {
            fs_iterator* pIterator;

            pIterator = fs_first(pTestState->pFS, "/test2.zip/archives/test1.zip", FS_READ | FS_OPAQUE | FS_ONLY_MOUNTS);
            if (pIterator != NULL) {
                printf("%s: Unexpected success when iterating over nested archive contents in opaque mode.\n", pTest->name);
                fs_free_iterator(pIterator);
                return FS_ERROR;
            }
        }

        /* Verbose. */
        {
            const char* pExpectedNames[] = {
                "a",
                "dir1"
            };

            result = fs_test_archives_iteration_basic_test(pTest, "/test2.zip/archives/test1.zip", FS_READ | FS_VERBOSE | FS_ONLY_MOUNTS, pExpectedNames, FS_COUNTOF(pExpectedNames));
            if (result != FS_SUCCESS) {
                return FS_ERROR;
            }
        }

        /* Transparent. */
        {
            /* With verbose path. */
            {
                const char* pExpectedNames[] = {
                    "a",
                    "dir1"
                };

                result = fs_test_archives_iteration_basic_test(pTest, "/test2.zip/archives/test1.zip", FS_READ | FS_TRANSPARENT | FS_ONLY_MOUNTS, pExpectedNames, FS_COUNTOF(pExpectedNames));
                if (result != FS_SUCCESS) {
                    return FS_ERROR;
                }
            }

            /* With transparent inner archive. */
            {
                const char* pExpectedNames[] = {
                    "a",
                    "dir1",
                    "test1.zip"
                };

                result = fs_test_archives_iteration_basic_test(pTest, "/test2.zip/archives", FS_READ | FS_TRANSPARENT | FS_ONLY_MOUNTS, pExpectedNames, FS_COUNTOF(pExpectedNames));
                if (result != FS_SUCCESS) {
                    return FS_ERROR;
                }
            }

            /* With transparent inner and outer archive. */
            {
                const char* pExpectedNames[] = {
                    "a",
                    "dir1",
                    "test1.zip"
                };

                result = fs_test_archives_iteration_basic_test(pTest, "/archives", FS_READ | FS_TRANSPARENT | FS_ONLY_MOUNTS, pExpectedNames, FS_COUNTOF(pExpectedNames));
                if (result != FS_SUCCESS) {
                    return FS_ERROR;
                }
            }
        }
    }

    return FS_SUCCESS;
}
/* END archives_recursive */

/* BEG archives_uninit */
int fs_test_archives_uninit(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;

    /* Clean up the entire temp folder. */
    result = fs_test_system_remove_directory(pTest, pTestState->pTempDir);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to remove temp directory.\n", pTest->name);
        return FS_ERROR;
    }

    fs_uninit(pTestState->pFS);
    return FS_SUCCESS;
}
/* END archive_uninit */

/* BEG mem_init */
int fs_test_mem_init(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_config fsConfig;
    fs* pFS;
    fs_file_info info;

    /* Initialize fs_mem backend. */
    fsConfig = fs_config_init(FS_MEM, NULL, NULL);

    result = fs_init(&fsConfig, &pFS);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to initialize fs_mem file system.\n", pTest->name);
        return FS_ERROR;
    }

    pTestState->pFS = pFS;

    /* Verify root directory exists and is accessible. */
    result = fs_info(pFS, "/", FS_IGNORE_MOUNTS, &info);
    if (result != FS_SUCCESS || !info.directory) {
        printf("%s: Root directory is not accessible.\n", pTest->name);
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END mem_init */

/* BEG mem_mkdir */
int fs_test_mem_mkdir(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs* pFS = pTestState->pFS;
    fs_result result;
    fs_file_info info;

    /* Create nested directory structure. */
    result = fs_mkdir(pFS, "/testdir", FS_WRITE | FS_IGNORE_MOUNTS);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create /testdir.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_mkdir(pFS, "/testdir/subdir1", FS_WRITE | FS_IGNORE_MOUNTS);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create /testdir/subdir1.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_mkdir(pFS, "/testdir/subdir2", FS_WRITE | FS_IGNORE_MOUNTS);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create /testdir/subdir2.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_mkdir(pFS, "/testdir/subdir1/nested", FS_WRITE | FS_IGNORE_MOUNTS);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create /testdir/subdir1/nested.\n", pTest->name);
        return FS_ERROR;
    }

    /* Verify directories exist and have correct properties. */
    result = fs_info(pFS, "/testdir", FS_IGNORE_MOUNTS, &info);
    if (result != FS_SUCCESS || !info.directory) {
        printf("%s: /testdir does not exist or is not a directory.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_info(pFS, "/testdir/subdir1/nested", FS_IGNORE_MOUNTS, &info);
    if (result != FS_SUCCESS || !info.directory) {
        printf("%s: Nested directory does not exist or is not a directory.\n", pTest->name);
        return FS_ERROR;
    }

    /* Test duplicate directory creation fails. */
    result = fs_mkdir(pFS, "/testdir", FS_WRITE | FS_IGNORE_MOUNTS);
    if (result == FS_SUCCESS) {
        printf("%s: Duplicate directory creation should have failed.\n", pTest->name);
        return FS_ERROR;
    }

    /* Test error condition: Try to mkdir with file as parent (requires a file to exist first). */
    /* Create a temporary file for this test. */
    {
        fs_file* pFile;
        result = fs_file_open(pFS, "/testdir/temp_file_for_mkdir_test.txt", FS_WRITE | FS_IGNORE_MOUNTS, &pFile);
        if (result == FS_SUCCESS) {
            fs_file_close(pFile);
            
            /* Now try to mkdir with this file as parent. */
            result = fs_mkdir(pFS, "/testdir/temp_file_for_mkdir_test.txt/subdir", FS_WRITE | FS_IGNORE_MOUNTS);
            if (result == FS_SUCCESS) {
                printf("%s: Creating directory with file as parent should have failed.\n", pTest->name);
                return FS_ERROR;
            }
            
            /* Clean up the temp file. */
            fs_remove(pFS, "/testdir/temp_file_for_mkdir_test.txt", FS_WRITE | FS_IGNORE_MOUNTS);
        }
    }

    return FS_SUCCESS;
}
/* END mem_mkdir */

/* BEG mem_write */
int fs_test_mem_write(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;

    /* This is just a container test - actual write tests are in sub-tests. */
    (void)pTestState;
    return FS_SUCCESS;
}
/* END mem_write */

/* BEG mem_write_new */
int fs_test_mem_write_new(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs* pFS = pTestState->pFS;
    fs_result result;
    const char* testContent = "Hello, Memory File System!";
    fs_file_info info;

    result = fs_test_open_and_write_file(pTest, pFS, "/testdir/test_new.txt", FS_WRITE | FS_IGNORE_MOUNTS, testContent, strlen(testContent));
    if (result != FS_SUCCESS) {
        return FS_ERROR;
    }

    /* Verify file exists and has correct size. */
    result = fs_info(pFS, "/testdir/test_new.txt", FS_IGNORE_MOUNTS, &info);
    if (result != FS_SUCCESS) {
        printf("%s: Created file does not exist.\n", pTest->name);
        return FS_ERROR;
    }

    if (info.directory || (unsigned int)info.size != strlen(testContent)) {
        printf("%s: Created file has incorrect properties (directory=%d, size=%u, expected=%u).\n", pTest->name, info.directory, (unsigned int)info.size, (unsigned int)strlen(testContent));
        return FS_ERROR;
    }

    /* Test error condition: Try to create file in non-existent directory - should fail. */
    {
        fs_file* pFile;
        result = fs_file_open(pFS, "/completely_nonexistent_dir/file.txt", FS_WRITE | FS_IGNORE_MOUNTS | FS_NO_CREATE_DIRS, &pFile);
        if (result == FS_SUCCESS) {
            fs_file_close(pFile);
            printf("%s: Creating file in non-existent directory should have failed (result=%d).\n", pTest->name, result);
            return FS_ERROR;
        }

        /* Verify the expected error is FS_DOES_NOT_EXIST. */
        if (result != FS_DOES_NOT_EXIST) {
            printf("%s: Expected FS_DOES_NOT_EXIST (%d) but got %d when creating file in non-existent directory.\n", pTest->name, FS_DOES_NOT_EXIST, result);
            return FS_ERROR;
        }
    }

    return FS_SUCCESS;
}
/* END mem_write_new */

/* BEG mem_write_overwrite */
int fs_test_mem_write_overwrite(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs* pFS = pTestState->pFS;
    fs_result result;
    const char* originalContent = "Hello, Memory File System!";
    const char* newContent = "Overwritten content!";
    fs_file_info info;

    /* First write the original content to ensure we have a baseline. */
    result = fs_test_open_and_write_file(pTest, pFS, "/testdir/overwrite_test.txt", FS_WRITE | FS_IGNORE_MOUNTS, originalContent, strlen(originalContent));
    if (result != FS_SUCCESS) {
        return FS_ERROR;
    }

    /* Overwrite the file with new content. */
    result = fs_test_open_and_write_file(pTest, pFS, "/testdir/overwrite_test.txt", FS_WRITE | FS_TRUNCATE | FS_IGNORE_MOUNTS, newContent, strlen(newContent));
    if (result != FS_SUCCESS) {
        return FS_ERROR;
    }

    /* Verify file has new size. */
    result = fs_info(pFS, "/testdir/overwrite_test.txt", FS_IGNORE_MOUNTS, &info);
    if (result != FS_SUCCESS || (unsigned int)info.size != strlen(newContent)) {
        printf("%s: Overwritten file has incorrect size (got=%u, expected=%u).\n", pTest->name, (unsigned int)info.size, (unsigned int)strlen(newContent));
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END mem_write_overwrite */

/* BEG mem_write_append */
int fs_test_mem_write_append(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs* pFS = pTestState->pFS;
    fs_result result;
    fs_file* pFile;
    const char* appendContent = " Appended!";
    size_t originalSize;
    fs_file_info info;
    size_t bytesWritten;

    /* Get current file size. */
    result = fs_info(pFS, "/testdir/test_new.txt", FS_IGNORE_MOUNTS, &info);
    if (result != FS_SUCCESS) {
        printf("%s: Cannot get file info for append test.\n", pTest->name);
        return FS_ERROR;
    }
    originalSize = (size_t)info.size;

    /* Open file in append mode. */
    result = fs_file_open(pFS, "/testdir/test_new.txt", FS_WRITE | FS_APPEND | FS_IGNORE_MOUNTS, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file in append mode.\n", pTest->name);
        return FS_ERROR;
    }

    /* Write additional content. */
    result = fs_file_write(pFile, appendContent, strlen(appendContent), &bytesWritten);
    if (result != FS_SUCCESS || bytesWritten != strlen(appendContent)) {
        printf("%s: Failed to write in append mode.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    fs_file_close(pFile);

    /* Verify total size. */
    result = fs_info(pFS, "/testdir/test_new.txt", FS_IGNORE_MOUNTS, &info);
    if (result != FS_SUCCESS || (unsigned int)info.size != originalSize + strlen(appendContent)) {
        printf("%s: Appended file has incorrect size (got=%u, expected=%u).\n", pTest->name, (unsigned int)info.size, (unsigned int)(originalSize + strlen(appendContent)));
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END mem_write_append */

/* BEG mem_write_exclusive */
int fs_test_mem_write_exclusive(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs* pFS = pTestState->pFS;
    fs_result result;
    fs_file* pFile;
    const char* content = "Exclusive content";
    size_t bytesWritten;

    /* Try to create exclusive file that already exists - should fail. */
    result = fs_file_open(pFS, "/testdir/test_new.txt", FS_WRITE | FS_EXCLUSIVE | FS_IGNORE_MOUNTS, &pFile);
    if (result == FS_SUCCESS) {
        fs_file_close(pFile);
        printf("%s: Exclusive open of existing file should have failed.\n", pTest->name);
        return FS_ERROR;
    }

    /* Create new file with exclusive flag - should succeed. */
    result = fs_file_open(pFS, "/testdir/exclusive_test.txt", FS_WRITE | FS_EXCLUSIVE | FS_IGNORE_MOUNTS, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Exclusive creation of new file failed.\n", pTest->name);
        return FS_ERROR;
    }
    result = fs_file_write(pFile, content, strlen(content), &bytesWritten);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to write to exclusive file.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    fs_file_close(pFile);

    return FS_SUCCESS;
}
/* END mem_write_exclusive */

/* BEG mem_write_truncate */
int fs_test_mem_write_truncate(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs* pFS = pTestState->pFS;
    fs_result result;
    fs_file* pFile;
    fs_int64 cursor;
    const char* newContent = "Truncated and rewritten";
    size_t bytesWritten;
    fs_file_info info;

    /* Open existing file with truncate flag. */
    result = fs_file_open(pFS, "/testdir/test_new.txt", FS_WRITE | FS_TRUNCATE | FS_IGNORE_MOUNTS, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file with truncate flag.\n", pTest->name);
        return FS_ERROR;
    }

    /* File should be empty now. */
    result = fs_file_tell(pFile, &cursor);
    if (result != FS_SUCCESS || cursor != 0) {
        printf("%s: File cursor should be at 0 after truncate.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    /* Write new content. */
    result = fs_file_write(pFile, newContent, strlen(newContent), &bytesWritten);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to write after truncate.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    fs_file_close(pFile);

    /* Verify final size. */
    result = fs_info(pFS, "/testdir/test_new.txt", FS_IGNORE_MOUNTS, &info);
    if (result != FS_SUCCESS || info.size != strlen(newContent)) {
        printf("%s: Truncated file has incorrect size.\n", pTest->name);
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END mem_write_truncate */

/* BEG mem_write_seek */
int fs_test_mem_write_seek(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs* pFS = pTestState->pFS;
    fs_result result;
    fs_file* pFile;
    size_t bytesWritten;

    /* Create a file for seek testing. */
    result = fs_test_open_and_write_file(pTest, pFS, "/testdir/seek_test.txt", FS_WRITE | FS_IGNORE_MOUNTS, "0123456789", 10);
    if (result != FS_SUCCESS) {
        return FS_ERROR;
    }

    /* Open for read/write and test seeking. */
    result = fs_file_open(pFS, "/testdir/seek_test.txt", FS_WRITE | FS_IGNORE_MOUNTS, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file for seek test.\n", pTest->name);
        return FS_ERROR;
    }

    /* Seek to position 5 and write. */
    result = fs_file_seek(pFile, 5, FS_SEEK_SET);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to seek to position 5.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    result = fs_file_write(pFile, "ABC", 3, &bytesWritten);
    if (result != FS_SUCCESS || bytesWritten != 3) {
        printf("%s: Failed to write at seek position.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    fs_file_close(pFile);

    /* Verify the result by reading back. */
    result = fs_test_open_and_read_file(pTest, pFS, "/testdir/seek_test.txt", FS_READ | FS_IGNORE_MOUNTS, "01234ABC89", 10);
    if (result != FS_SUCCESS) {
        return FS_ERROR;
    }

    /* Test invalid seek (negative position). */
    result = fs_file_open(pFS, "/testdir/seek_test.txt", FS_READ | FS_IGNORE_MOUNTS, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file for seek test.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_file_seek(pFile, -10, FS_SEEK_SET);
    if (result == FS_SUCCESS) {
        printf("%s: Seeking to negative position should have failed.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    fs_file_close(pFile);

    /* Test edge case: Seek beyond file end and write (sparse file behavior). */
    result = fs_file_open(pFS, "/testdir/sparse_test.txt", FS_WRITE | FS_IGNORE_MOUNTS, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create file for sparse test.\n", pTest->name);
        return FS_ERROR;
    }

    /* Seek to position 100 (beyond end of empty file). */
    result = fs_file_seek(pFile, 100, FS_SEEK_SET);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to seek beyond file end.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    /* Write at position 100 - should create a sparse-like effect. */
    result = fs_file_write(pFile, "test", 4, &bytesWritten);
    if (result != FS_SUCCESS || bytesWritten != 4) {
        printf("%s: Failed to write at seek position 100.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }
    fs_file_close(pFile);

    /* Verify file size is now 104. */
    {
        fs_file_info info;
        result = fs_info(pFS, "/testdir/sparse_test.txt", FS_IGNORE_MOUNTS, &info);
        if (result != FS_SUCCESS || info.size != 104) {
            printf("%s: File size should be 104 after sparse write, got %u.\n", pTest->name, (unsigned int)info.size);
            return FS_ERROR;
        }
    }

    return FS_SUCCESS;
}
/* END mem_write_seek */

/* BEG mem_write_truncate2 */
int fs_test_mem_write_truncate2(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs* pFS = pTestState->pFS;
    fs_result result;
    fs_file* pFile;
    fs_file_info info;

    /* Open the seek test file and truncate it using fs_file_truncate(). */
    result = fs_file_open(pFS, "/testdir/seek_test.txt", FS_WRITE | FS_IGNORE_MOUNTS, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file for truncate test.\n", pTest->name);
        return FS_ERROR;
    }

    /* Seek to position 5 and truncate there. */
    result = fs_file_seek(pFile, 5, FS_SEEK_SET);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to seek for truncate test.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    result = fs_file_truncate(pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to truncate file.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    fs_file_close(pFile);

    /* Verify truncated size. */
    result = fs_info(pFS, "/testdir/seek_test.txt", FS_IGNORE_MOUNTS, &info);
    if (result != FS_SUCCESS || info.size != 5) {
        printf("%s: Truncated file has incorrect size (got=%u, expected=5).\n", pTest->name, (unsigned int)info.size);
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END mem_write_truncate2 */

/* BEG mem_write_flush */
int fs_test_mem_write_flush(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs* pFS = pTestState->pFS;
    fs_result result;
    fs_file* pFile;
    size_t bytesWritten;

    /* For memory backend, flush is essentially a no-op, but we should test it doesn't fail. */
    result = fs_file_open(pFS, "/testdir/flush_test.txt", FS_WRITE | FS_IGNORE_MOUNTS, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file for flush test.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_file_write(pFile, "test", 4, &bytesWritten);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to write for flush test.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    result = fs_file_flush(pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to flush file.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    fs_file_close(pFile);

    return FS_SUCCESS;
}
/* END mem_write_flush */

/* BEG mem_read */
int fs_test_mem_read(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs* pFS = pTestState->pFS;
    fs_result result;

    /* Read back the truncated content from the seek test. */
    result = fs_test_open_and_read_file(pTest, pFS, "/testdir/seek_test.txt", FS_READ | FS_IGNORE_MOUNTS, "01234", 5);
    if (result != FS_SUCCESS) {
        return FS_ERROR;
    }

    /* Test reading the overwrite test file. */
    result = fs_test_open_and_read_file(pTest, pFS, "/testdir/overwrite_test.txt", FS_READ | FS_IGNORE_MOUNTS, "Overwritten content!", 20);
    if (result != FS_SUCCESS) {
        return FS_ERROR;
    }

    /* Test reading the exclusive test file. */
    result = fs_test_open_and_read_file(pTest, pFS, "/testdir/exclusive_test.txt", FS_READ | FS_IGNORE_MOUNTS, "Exclusive content", 17);
    if (result != FS_SUCCESS) {
        return FS_ERROR;
    }

    /* Test error condition: Try to open directory as file - should fail. */
    {
        fs_file* pFile;
        result = fs_file_open(pFS, "/testdir", FS_READ | FS_IGNORE_MOUNTS, &pFile);
        if (result == FS_SUCCESS) {
            fs_file_close(pFile);
            printf("%s: Opening directory as file should have failed.\n", pTest->name);
            return FS_ERROR;
        }
    }

    /* Test edge case: Zero-byte file operations. */
    {
        fs_file* pFile;
        fs_file_info info;
        char buffer[10];
        size_t bytesRead;
        
        /* Create empty file. */
        result = fs_file_open(pFS, "/testdir/empty.txt", FS_WRITE | FS_IGNORE_MOUNTS, &pFile);
        if (result != FS_SUCCESS) {
            printf("%s: Failed to create empty file.\n", pTest->name);
            return FS_ERROR;
        }
        fs_file_close(pFile);

        /* Verify empty file has size 0. */
        result = fs_info(pFS, "/testdir/empty.txt", FS_IGNORE_MOUNTS, &info);
        if (result != FS_SUCCESS || info.size != 0) {
            printf("%s: Empty file should have size 0, got %u.\n", pTest->name, (unsigned int)info.size);
            return FS_ERROR;
        }

        /* Read from empty file. */
        result = fs_file_open(pFS, "/testdir/empty.txt", FS_READ | FS_IGNORE_MOUNTS, &pFile);
        if (result != FS_SUCCESS) {
            printf("%s: Failed to open empty file for reading.\n", pTest->name);
            return FS_ERROR;
        }

        result = fs_file_read(pFile, buffer, sizeof(buffer), &bytesRead);
        if (result != FS_AT_END || bytesRead != 0) {
            printf("%s: Reading from empty file should return FS_AT_END with 0 bytes.\n", pTest->name);
            fs_file_close(pFile);
            return FS_ERROR;
        }
        fs_file_close(pFile);
    }

    return FS_SUCCESS;
}
/* END mem_read */

/* BEG mem_read_readonly */
int fs_test_mem_read_readonly(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs* pFS = pTestState->pFS;
    fs_result result;
    fs_file* pFile;
    size_t bytesWritten;

    /* Open file in read-only mode. */
    result = fs_file_open(pFS, "/testdir/test_new.txt", FS_READ | FS_IGNORE_MOUNTS, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file in read-only mode.\n", pTest->name);
        return FS_ERROR;
    }

    /* Try to write - should fail. */
    result = fs_file_write(pFile, "test", 4, &bytesWritten);
    if (result == FS_SUCCESS) {
        printf("%s: Writing to read-only file should have failed.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    fs_file_close(pFile);

    return FS_SUCCESS;
}
/* END mem_read_readonly */

/* BEG mem_read_noexist */
int fs_test_mem_read_noexist(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs* pFS = pTestState->pFS;
    fs_result result;
    fs_file* pFile;

    /* Try to open non-existent file. */
    result = fs_file_open(pFS, "/testdir/nonexistent.txt", FS_READ | FS_IGNORE_MOUNTS, &pFile);
    if (result == FS_SUCCESS) {
        fs_file_close(pFile);
        printf("%s: Opening non-existent file should have failed.\n", pTest->name);
        return FS_ERROR;
    }

    /* Check that the error is the expected one. */
    if (result != FS_DOES_NOT_EXIST) {
        printf("%s: Expected FS_DOES_NOT_EXIST but got %d.\n", pTest->name, result);
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END mem_read_noexist */

/* BEG mem_duplicate */
int fs_test_mem_duplicate(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs* pFS = pTestState->pFS;
    fs_result result;
    fs_file* pFile;
    fs_file* pDuplicate;
    char buffer1[32], buffer2[32];
    size_t bytesRead1, bytesRead2;

    /* Open a file. */
    result = fs_file_open(pFS, "/testdir/test_new.txt", FS_READ | FS_IGNORE_MOUNTS, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file for duplication test.\n", pTest->name);
        return FS_ERROR;
    }

    /* Duplicate the file handle. */
    result = fs_file_duplicate(pFile, &pDuplicate);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to duplicate file handle.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    /* Read from both handles. */
    
    result = fs_file_read(pFile, buffer1, sizeof(buffer1), &bytesRead1);
    if (result != FS_SUCCESS && result != FS_AT_END) {
        printf("%s: Failed to read from original file handle.\n", pTest->name);
        fs_file_close(pFile);
        fs_file_close(pDuplicate);
        return FS_ERROR;
    }

    /* Reset cursor on duplicate and read. */
    result = fs_file_seek(pDuplicate, 0, FS_SEEK_SET);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to seek duplicate handle.\n", pTest->name);
        fs_file_close(pFile);
        fs_file_close(pDuplicate);
        return FS_ERROR;
    }

    result = fs_file_read(pDuplicate, buffer2, sizeof(buffer2), &bytesRead2);
    if (result != FS_SUCCESS && result != FS_AT_END) {
        printf("%s: Failed to read from duplicate file handle.\n", pTest->name);
        fs_file_close(pFile);
        fs_file_close(pDuplicate);
        return FS_ERROR;
    }

    /* Compare the reads. */
    if (bytesRead1 != bytesRead2 || memcmp(buffer1, buffer2, bytesRead1) != 0) {
        printf("%s: Original and duplicate file handles returned different content.\n", pTest->name);
        fs_file_close(pFile);
        fs_file_close(pDuplicate);
        return FS_ERROR;
    }

    fs_file_close(pFile);
    fs_file_close(pDuplicate);

    /* Test edge case: Multiple file handles to same file. */
    {
        fs_file* pFile1;
        fs_file* pFile2;
        char readBuffer[10];
        size_t bytesRead;
        size_t bytesWritten;
        
        result = fs_file_open(pFS, "/testdir/multihandle.txt", FS_WRITE | FS_IGNORE_MOUNTS, &pFile1);
        if (result != FS_SUCCESS) {
            printf("%s: Failed to open file for multi-handle test.\n", pTest->name);
            return FS_ERROR;
        }

        result = fs_file_open(pFS, "/testdir/multihandle.txt", FS_READ | FS_IGNORE_MOUNTS, &pFile2);
        if (result != FS_SUCCESS) {
            printf("%s: Failed to open second handle to same file.\n", pTest->name);
            fs_file_close(pFile1);
            return FS_ERROR;
        }

        /* Write with first handle. */
        result = fs_file_write(pFile1, "handle1", 7, &bytesWritten);
        if (result != FS_SUCCESS) {
            printf("%s: Failed to write with first handle.\n", pTest->name);
            fs_file_close(pFile1);
            fs_file_close(pFile2);
            return FS_ERROR;
        }

        /* Read with second handle should see the data. */
        result = fs_file_read(pFile2, readBuffer, 7, &bytesRead);
        if (result != FS_SUCCESS || bytesRead != 7 || memcmp(readBuffer, "handle1", 7) != 0) {
            printf("%s: Second handle should see data written by first handle.\n", pTest->name);
            fs_file_close(pFile1);
            fs_file_close(pFile2);
            return FS_ERROR;
        }

        fs_file_close(pFile1);
        fs_file_close(pFile2);
    }

    return FS_SUCCESS;
}
/* END mem_duplicate */

/* BEG mem_iteration */
int fs_test_mem_iteration(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs* pFS = pTestState->pFS;
    fs_result result;
    fs_iterator* pIterator;
    int fileCount = 0;
    int dirCount = 0;
    int expectedFiles = 0;
    fs_file_info info;
    char fullPath[512];
    const char* expectedFileNames[] = {
        "/testdir/test_new.txt",
        "/testdir/overwrite_test.txt", 
        "/testdir/exclusive_test.txt",
        "/testdir/seek_test.txt",
        "/testdir/flush_test.txt"
    };
    int i;
    
    /* Count expected files by checking what exists */
    for (i = 0; i < 5; i += 1) {
        result = fs_info(pFS, expectedFileNames[i], FS_IGNORE_MOUNTS, &info);
        if (result == FS_SUCCESS && !info.directory) {
            expectedFiles += 1;
        }
    }

    /* Iterate through /testdir. */
    pIterator = fs_first(pFS, "/testdir", FS_IGNORE_MOUNTS);
    if (pIterator == NULL) {
        printf("%s: Failed to start directory iteration.\n", pTest->name);
        return FS_ERROR;
    }

    while (pIterator != NULL) {
        if (pIterator->info.directory) {
            dirCount += 1;
        } else {
            fileCount += 1;
        }
        
        /* Verify we can get info for each item. */
        fs_snprintf(fullPath, sizeof(fullPath), "/testdir/%s", pIterator->pName);
        
        result = fs_info(pFS, fullPath, FS_IGNORE_MOUNTS, &info);
        if (result != FS_SUCCESS) {
            printf("%s: Failed to get info for iterated item %s.\n", pTest->name, pIterator->pName);
            return FS_ERROR;
        }

        if (info.directory != pIterator->info.directory) {
            printf("%s: Iterator and info disagree on directory status for %s.\n", pTest->name, pIterator->pName);
            return FS_ERROR;
        }

        pIterator = fs_next(pIterator);
    }

    /* We should have found at least 1 directory and some files. */
    if (dirCount < 1) {
        printf("%s: Expected at least 1 directory, found %d.\n", pTest->name, dirCount);
        return FS_ERROR;
    }

    if (fileCount < expectedFiles) {
        printf("%s: Expected at least %d files, found %d.\n", pTest->name, expectedFiles, fileCount);
        return FS_ERROR;
    }

    /* Test error condition: Try to iterate a file as directory. */
    /* Create a temporary file for this test. */
    {
        fs_file* pFile;
        result = fs_file_open(pFS, "/testdir/temp_file_for_iteration_test.txt", FS_WRITE | FS_IGNORE_MOUNTS, &pFile);
        if (result == FS_SUCCESS) {
            fs_file_close(pFile);
            
            /* Now try to iterate this file as a directory. */
            pIterator = fs_first(pFS, "/testdir/temp_file_for_iteration_test.txt", FS_IGNORE_MOUNTS);
            if (pIterator != NULL) {
                printf("%s: Iterating a file as directory should have failed.\n", pTest->name);
                return FS_ERROR;
            }
            
            /* Clean up the temp file. */
            fs_remove(pFS, "/testdir/temp_file_for_iteration_test.txt", FS_WRITE | FS_IGNORE_MOUNTS);
        }
    }

    return FS_SUCCESS;
}
/* END mem_iteration */

/* BEG mem_rename */
int fs_test_mem_rename(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs* pFS = pTestState->pFS;
    fs_result result;
    fs_file_info info;

    /* Rename a file. */
    result = fs_rename(pFS, "/testdir/test_new.txt", "/testdir/renamed_file.txt", FS_WRITE | FS_IGNORE_MOUNTS);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to rename file.\n", pTest->name);
        return FS_ERROR;
    }

    /* Verify old name doesn't exist. */
    result = fs_info(pFS, "/testdir/test_new.txt", FS_IGNORE_MOUNTS, &info);
    if (result == FS_SUCCESS) {
        printf("%s: Old file name still exists after rename.\n", pTest->name);
        return FS_ERROR;
    }

    /* Verify new name exists. */
    result = fs_info(pFS, "/testdir/renamed_file.txt", FS_IGNORE_MOUNTS, &info);
    if (result != FS_SUCCESS) {
        printf("%s: New file name doesn't exist after rename.\n", pTest->name);
        return FS_ERROR;
    }

    /* Rename a directory. */
    result = fs_rename(pFS, "/testdir/subdir2", "/testdir/renamed_dir", FS_WRITE | FS_IGNORE_MOUNTS);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to rename directory.\n", pTest->name);
        return FS_ERROR;
    }

    /* Verify directory rename worked. */
    result = fs_info(pFS, "/testdir/renamed_dir", FS_IGNORE_MOUNTS, &info);
    if (result != FS_SUCCESS || !info.directory) {
        printf("%s: Renamed directory doesn't exist or is not a directory.\n", pTest->name);
        return FS_ERROR;
    }

    /* Test error condition: Try to rename root directory - should fail. */
    result = fs_rename(pFS, "/", "/newroot", FS_WRITE | FS_IGNORE_MOUNTS);
    if (result == FS_SUCCESS) {
        printf("%s: Renaming root directory should have failed.\n", pTest->name);
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END mem_rename */

/* BEG mem_remove */
int fs_test_mem_remove(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs* pFS = pTestState->pFS;
    fs_result result;
    fs_file_info info;

    /* Remove a file. */
    result = fs_remove(pFS, "/testdir/flush_test.txt", FS_WRITE | FS_IGNORE_MOUNTS);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to remove file.\n", pTest->name);
        return FS_ERROR;
    }

    /* Verify file is gone. */
    result = fs_info(pFS, "/testdir/flush_test.txt", FS_IGNORE_MOUNTS, &info);
    if (result == FS_SUCCESS) {
        printf("%s: File still exists after removal.\n", pTest->name);
        return FS_ERROR;
    }

    /* Try to remove non-empty directory - should fail. */
    result = fs_remove(pFS, "/testdir/subdir1", FS_WRITE | FS_IGNORE_MOUNTS);
    if (result == FS_SUCCESS) {
        printf("%s: Removing non-empty directory should have failed.\n", pTest->name);
        return FS_ERROR;
    }

    /* Remove nested directory first. */
    result = fs_remove(pFS, "/testdir/subdir1/nested", FS_WRITE | FS_IGNORE_MOUNTS);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to remove nested directory.\n", pTest->name);
        return FS_ERROR;
    }

    /* Now remove the parent directory. */
    result = fs_remove(pFS, "/testdir/subdir1", FS_WRITE | FS_IGNORE_MOUNTS);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to remove empty directory.\n", pTest->name);
        return FS_ERROR;
    }

    /* Remove the renamed empty directory. */
    result = fs_remove(pFS, "/testdir/renamed_dir", FS_WRITE | FS_IGNORE_MOUNTS);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to remove renamed directory.\n", pTest->name);
        return FS_ERROR;
    }

    /* Test error condition: Try to remove root directory - should fail. */
    result = fs_remove(pFS, "/", FS_WRITE | FS_IGNORE_MOUNTS);
    if (result == FS_SUCCESS) {
        printf("%s: Removing root directory should have failed.\n", pTest->name);
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END mem_remove */

/* BEG mem_edge_cases */
int fs_test_mem_edge_cases(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    
    /* This test function previously contained various edge case tests */
    /* that have been moved to their more appropriate test functions. */
    /* Keeping this function for now to maintain test structure. */
    (void)pTestState;
    
    return FS_SUCCESS;
}
/* END mem_edge_cases */

/* BEG mem_stress_test */
int fs_test_mem_stress_test(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs* pFS = pTestState->pFS;
    fs_result result;
    char filename[64];
    char content[32];
    int i;

    /* Create many files in one directory. */
    for (i = 0; i < 50; i += 1) {
        fs_snprintf(filename, sizeof(filename), "/testdir/stress_%03d.txt", i);
        fs_snprintf(content, sizeof(content), "File number %d content", i);
        
        result = fs_test_open_and_write_file(pTest, pFS, filename, FS_WRITE | FS_IGNORE_MOUNTS, content, strlen(content));
        if (result != FS_SUCCESS) {
            printf("%s: Failed to create stress test file %d.\n", pTest->name, i);
            return FS_ERROR;
        }
    }

    /* Verify all files exist and have correct content. */
    for (i = 0; i < 50; i += 1) {
        fs_snprintf(filename, sizeof(filename), "/testdir/stress_%03d.txt", i);
        fs_snprintf(content, sizeof(content), "File number %d content", i);
        
        result = fs_test_open_and_read_file(pTest, pFS, filename, FS_READ | FS_IGNORE_MOUNTS, content, strlen(content));
        if (result != FS_SUCCESS) {
            printf("%s: Failed to verify stress test file %d.\n", pTest->name, i);
            return FS_ERROR;
        }
    }

    /* Create deep directory structure. */
    {
        char deepPath[256] = "/testdir";
        for (i = 0; i < 10; i += 1) {
            fs_snprintf(deepPath, sizeof(deepPath), "%s/level%d", deepPath, i);
            
            result = fs_mkdir(pFS, deepPath, FS_WRITE | FS_IGNORE_MOUNTS);
            if (result != FS_SUCCESS) {
                printf("%s: Failed to create deep directory level %d.\n", pTest->name, i);
                return FS_ERROR;
            }
        }

        /* Create a file in the deepest directory. */
        strncat(deepPath, "/deep_file.txt", sizeof(deepPath) - strlen(deepPath) - 1);
        result = fs_test_open_and_write_file(pTest, pFS, deepPath, FS_WRITE | FS_IGNORE_MOUNTS, "deep content", 12);
        if (result != FS_SUCCESS) {
            printf("%s: Failed to create file in deep directory.\n", pTest->name);
            return FS_ERROR;
        }

        /* Verify the deep file. */
        result = fs_test_open_and_read_file(pTest, pFS, deepPath, FS_READ | FS_IGNORE_MOUNTS, "deep content", 12);
        if (result != FS_SUCCESS) {
            printf("%s: Failed to read file from deep directory.\n", pTest->name);
            return FS_ERROR;
        }
    }

    return FS_SUCCESS;
}
/* END mem_stress_test */

/* BEG mem_uninit */
int fs_test_mem_uninit(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs* pFS = pTestState->pFS;

    if (pFS != NULL) {
        fs_uninit(pFS);
        pTestState->pFS = NULL;
    }

    return FS_SUCCESS;
}
/* END mem_uninit */

/* Helper function to set up source directory structure for serialization test */
static fs_result fs_test_serialization_set_up_src(fs_test* pTest, fs* pFS)
{
    fs_result result;
    
    /* Create the source directory structure */
    result = fs_mkdir(pFS, "/root/src", FS_WRITE);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create /root/src directory.\n", pTest->name);
        return FS_ERROR;
    }
    
    result = fs_mkdir(pFS, "/root/src/subdir", FS_WRITE);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create /root/src/subdir directory.\n", pTest->name);
        return FS_ERROR;
    }
    
    return FS_SUCCESS;
}

/* BEG serialization */
int fs_test_serialization(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_config fsConfig;
    fs* pFS;
    fs_memory_stream serializedData;
    const char* pContentA = "Content A";
    const char* pContentB = "Content B";
    const char* pContentC = "Content C";

    /* Initialize fs_mem backend. */
    fsConfig = fs_config_init(pTestState->pBackend, NULL, NULL);

    result = fs_init(&fsConfig, &pFS);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to initialize fs_mem file system.\n", pTest->name);
        return FS_ERROR;
    }

    pTestState->pFS = pFS;

    /* Create a temp directory. */
    result = fs_mktmp("fs_", pTestState->pTempDir, sizeof(pTestState->pTempDir), FS_MKTMP_DIR);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create temp directory for serialization test.\n", pTest->name);
        fs_uninit(pFS);
        pTestState->pFS = NULL;
        return FS_ERROR;
    }

    /* Set up the mount point. */
    result = fs_mount(pFS, pTestState->pTempDir, "/root", FS_READ | FS_WRITE);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to mount temp directory.\n", pTest->name);
        fs_uninit(pFS);
        pTestState->pFS = NULL;
        return FS_ERROR;
    }

    result = fs_test_serialization_set_up_src(pTest, pFS);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to set up source files for serialization test.\n", pTest->name);
        fs_uninit(pFS);
        pTestState->pFS = NULL;
        return FS_ERROR;
    }


    /* Serialize the /root/src directory. */
    result = fs_test_open_and_write_file(pTest, pFS, "/root/src/a.txt", FS_WRITE, pContentA, strlen(pContentA));
    if (result != FS_SUCCESS) {
        printf("%s: Failed to write a.txt for serialization test.\n", pTest->name);
        fs_uninit(pFS);
        pTestState->pFS = NULL;
        return FS_ERROR;
    }

    result = fs_test_open_and_write_file(pTest, pFS, "/root/src/subdir/b.txt", FS_WRITE, pContentB, strlen(pContentB));
    if (result != FS_SUCCESS) {
        printf("%s: Failed to write b.txt for serialization test.\n", pTest->name);
        fs_uninit(pFS);
        pTestState->pFS = NULL;
        return FS_ERROR;
    }

    result = fs_test_open_and_write_file(pTest, pFS, "/root/src/subdir/c.txt", FS_WRITE, pContentC, strlen(pContentC));
    if (result != FS_SUCCESS) {
        printf("%s: Failed to write c.txt for serialization test.\n", pTest->name);
        fs_uninit(pFS);
        pTestState->pFS = NULL;
        return FS_ERROR;
    }

    /* Initialize the memory stream for writing */
    result = fs_memory_stream_init_write(NULL, &serializedData);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to initialize memory stream for serialization.\n", pTest->name);
        fs_uninit(pFS);
        pTestState->pFS = NULL;
        return FS_ERROR;
    }

    result = fs_serialize(pFS, "/root/src", 0, (fs_stream*)&serializedData);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to serialize /root/src directory.\n", pTest->name);
        fs_memory_stream_uninit(&serializedData);
        fs_uninit(pFS);
        pTestState->pFS = NULL;
        return FS_ERROR;
    }

    /* Now deserialize into the /root/dst directory. */
    result = fs_stream_seek((fs_stream*)&serializedData, 0, FS_SEEK_SET);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to reset stream position before deserialization.\n", pTest->name);
        fs_memory_stream_uninit(&serializedData);
        fs_uninit(pFS);
        pTestState->pFS = NULL;
        return FS_ERROR;
    }
    
    result = fs_deserialize(pFS, "/root/dst", 0, (fs_stream*)&serializedData);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to deserialize into /root/dst directory.\n", pTest->name);
        fs_memory_stream_uninit(&serializedData);
        fs_uninit(pFS);
        pTestState->pFS = NULL;
        return FS_ERROR;
    }

    /* Now we need to verify the data. The content within /root/dst should exactly match that in /root/src. */
    
    /* Verify the directory structure and file contents match between src and dst */
    
    /* First, verify that all files in /root/src exist in /root/dst with same content */
    result = fs_test_open_and_read_file(pTest, pFS, "/root/dst/a.txt", FS_READ, pContentA, strlen(pContentA));
    if (result != FS_SUCCESS) {
        printf("%s: Failed to verify a.txt content in dst directory.\n", pTest->name);
        fs_memory_stream_uninit(&serializedData);
        fs_uninit(pFS);
        pTestState->pFS = NULL;
        return FS_ERROR;
    }
    
    result = fs_test_open_and_read_file(pTest, pFS, "/root/dst/subdir/b.txt", FS_READ, pContentB, strlen(pContentB));
    if (result != FS_SUCCESS) {
        printf("%s: Failed to verify b.txt content in dst directory.\n", pTest->name);
        fs_memory_stream_uninit(&serializedData);
        fs_uninit(pFS);
        pTestState->pFS = NULL;
        return FS_ERROR;
    }
    
    result = fs_test_open_and_read_file(pTest, pFS, "/root/dst/subdir/c.txt", FS_READ, pContentC, strlen(pContentC));
    if (result != FS_SUCCESS) {
        printf("%s: Failed to verify c.txt content in dst directory.\n", pTest->name);
        fs_memory_stream_uninit(&serializedData);
        fs_uninit(pFS);
        pTestState->pFS = NULL;
        return FS_ERROR;
    }
    
    /* Verify directory structure by iterating through both directories and comparing */
    {
        fs_iterator* pSrcIterator;
        fs_iterator* pDstIterator;
        int srcCount = 0, dstCount = 0;
        
        /* Count items in src directory */
        pSrcIterator = fs_first(pFS, "/root/src", FS_READ);
        if (pSrcIterator == NULL) {
            printf("%s: Failed to iterate /root/src directory.\n", pTest->name);
            fs_memory_stream_uninit(&serializedData);
            fs_uninit(pFS);
            pTestState->pFS = NULL;
            return FS_ERROR;
        }
        
        while (pSrcIterator != NULL) {
            srcCount++;
            pSrcIterator = fs_next(pSrcIterator);
        }
        
        /* Count items in dst directory */
        pDstIterator = fs_first(pFS, "/root/dst", FS_READ);
        if (pDstIterator == NULL) {
            printf("%s: Failed to iterate /root/dst directory.\n", pTest->name);
            fs_memory_stream_uninit(&serializedData);
            fs_uninit(pFS);
            pTestState->pFS = NULL;
            return FS_ERROR;
        }
        
        while (pDstIterator != NULL) {
            dstCount++;
            pDstIterator = fs_next(pDstIterator);
        }
        
        if (srcCount != dstCount) {
            printf("%s: Directory item count mismatch - src has %d items, dst has %d items.\n", pTest->name, srcCount, dstCount);
            fs_memory_stream_uninit(&serializedData);
            fs_uninit(pFS);
            pTestState->pFS = NULL;
            return FS_ERROR;
        }
        
        /* Verify subdir structure too */
        srcCount = 0;
        dstCount = 0;
        
        pSrcIterator = fs_first(pFS, "/root/src/subdir", FS_READ);
        if (pSrcIterator == NULL) {
            printf("%s: Failed to iterate /root/src/subdir directory.\n", pTest->name);
            fs_memory_stream_uninit(&serializedData);
            fs_uninit(pFS);
            pTestState->pFS = NULL;
            return FS_ERROR;
        }
        
        while (pSrcIterator != NULL) {
            srcCount++;
            pSrcIterator = fs_next(pSrcIterator);
        }
        
        pDstIterator = fs_first(pFS, "/root/dst/subdir", FS_READ);
        if (pDstIterator == NULL) {
            printf("%s: Failed to iterate /root/dst/subdir directory.\n", pTest->name);
            fs_memory_stream_uninit(&serializedData);
            fs_uninit(pFS);
            pTestState->pFS = NULL;
            return FS_ERROR;
        }
        
        while (pDstIterator != NULL) {
            dstCount++;
            pDstIterator = fs_next(pDstIterator);
        }
        
        if (srcCount != dstCount) {
            printf("%s: Subdir item count mismatch - src/subdir has %d items, dst/subdir has %d items.\n", pTest->name, srcCount, dstCount);
            fs_memory_stream_uninit(&serializedData);
            fs_uninit(pFS);
            pTestState->pFS = NULL;
            return FS_ERROR;
        }
    }
    
    /* Clean up the serialized data */
    fs_memory_stream_uninit(&serializedData);

    /* Clean up the file system. */
    fs_test_system_remove_directory(pTest, pTestState->pTempDir);

    return FS_SUCCESS;
}
/* END serialization */


int main(int argc, char** argv)
{
    int result;
    const fs_backend* pBackend;

    /* Tests. */
    fs_test test_root;
    fs_test test_path;
    fs_test test_path_iteration;                    /* Tests path breakup logic. This is critical for some internal logic in the library. */
    fs_test test_path_normalize;                    /* Tests path normalization, like resolving ".." and "." segments. Again, this is used extensively for path validation and therefore needs proper testing. */
    fs_test test_path_trim_base;
    fs_test test_system;
    fs_test test_system_sysdir;                     /* Standard directory tests need to come first because we'll be writing out our test files to a temp folder. */
    fs_test test_system_init;
    fs_test test_system_mktmp;                      /* The output from this test will be used for subsequent tests. Tests will output into the temp directory created here. */
    fs_test test_system_mkdir;
    fs_test test_system_write;
    fs_test test_system_write_new;                  /* Tests creation of a new file. */
    fs_test test_system_write_overwrite;            /* Tests FS_WRITE (overwrite mode). */
    fs_test test_system_write_append;               /* Tests FS_WRITE | FS_APPEND. */
    fs_test test_system_write_exclusive;            /* Tests FS_WRITE | FS_EXCLUSIVE. */
    fs_test test_system_write_truncate;             /* Tests FS_WRITE | FS_TRUNCATE. */
    fs_test test_system_write_truncate2;            /* Tests fs_file_truncate(). */
    fs_test test_system_write_seek;                 /* Tests seeking while writing. */
    fs_test test_system_write_flush;                /* Tests fs_file_flush(). */
    fs_test test_system_read;                       /* Tests FS_READ. Also acts as the parent test for other reading related tests. */
    fs_test test_system_read_readonly;              /* Tests that writing to a read-only file fails. */
    fs_test test_system_read_noexist;               /* Tests that reading a non-existent file fails cleanly. */
    fs_test test_system_duplicate;                  /* Tests fs_file_duplicate(). */
    fs_test test_system_rename;                     /* Tests fs_rename(). Make sure this is done before the remove test. */
    fs_test test_system_remove;                     /* Tests fs_remove(). This will delete all of the test files we created earlier. Therefore it should be the last test, before uninitialization. */
    fs_test test_system_uninit;                     /* Needs to be last since this is where the fs_uninit() function is called. */
    fs_test test_mounts;                            /* The top-level test for mounts. This will set up the `fs` object and the folder and file structure in preparation for subsequent tests. */
    fs_test test_mounts_write;                      /* Tests writing to mounts. */
    fs_test test_mounts_read;                       /* Tests reading from mounts. */
    fs_test test_mounts_mkdir;                      /* Tests creating directories with mounts. */
    fs_test test_mounts_rename;                     /* Tests renaming files with mounts. */
    fs_test test_mounts_remove;                     /* Tests removing files with mounts. */
    fs_test test_mounts_iteration;                  /* Tests iterating directories with mounts. */
    fs_test test_unmount;                           /* This needs to be the last mount test. */
    fs_test test_archives;                          /* The top-level test for archives. This will set up the `fs` object and the folder and file structure in preparation for subsequent tests. */
    fs_test test_archives_opaque;                   /* Tests that opening files inside an archive in opaque mode fails as expected. */
    fs_test test_archives_verbose;                  /* Tests that opening files inside an archive with an explicit (verbose) path works as expected. */
    fs_test test_archives_transparent;              /* Tests that opening files inside an archive with a transparent path works as expected. */
    fs_test test_archives_mount;                    /* Tests that mounting an archive works as expected. */
    fs_test test_archives_iteration;                /* Tests iteration with archives. */
    fs_test test_archives_iteration_opaque;         /* Tests iteration with archives in opaque mode. */
    fs_test test_archives_iteration_verbose;        /* Tests iteration with archives in verbose mode. */
    fs_test test_archives_iteration_transparent;    /* Tests iteration with archives in transparent mode. */
    fs_test test_archives_recursive;                /* Tests archives inside archives. */
    fs_test test_archives_uninit;                   /* This needs to be the last archive test. */
    fs_test test_mem;                               /* The top-level test for memory backend. This will set up the fs_mem object in preparation for subsequent tests. */
    fs_test test_mem_init;                          /* Initializes the memory backend. */
    fs_test test_mem_mkdir;                         /* Tests creating directories with memory backend. */
    fs_test test_mem_write;                         /* Tests writing to memory backend. */
    fs_test test_mem_write_new;                     /* Tests creation of a new file in memory. */
    fs_test test_mem_write_overwrite;               /* Tests FS_WRITE (overwrite mode) in memory. */
    fs_test test_mem_write_append;                  /* Tests FS_WRITE | FS_APPEND in memory. */
    fs_test test_mem_write_exclusive;               /* Tests FS_WRITE | FS_EXCLUSIVE in memory. */
    fs_test test_mem_write_truncate;                /* Tests FS_WRITE | FS_TRUNCATE in memory. */
    fs_test test_mem_write_seek;                    /* Tests seeking while writing in memory. */
    fs_test test_mem_write_truncate2;               /* Tests fs_file_truncate() in memory. */
    fs_test test_mem_write_flush;                   /* Tests fs_file_flush() in memory. */
    fs_test test_mem_read;                          /* Tests FS_READ in memory. Also acts as the parent test for other reading related tests. */
    fs_test test_mem_read_readonly;                 /* Tests that writing to a read-only file fails in memory. */
    fs_test test_mem_read_noexist;                  /* Tests that reading a non-existent file fails cleanly in memory. */
    fs_test test_mem_duplicate;                     /* Tests fs_file_duplicate() in memory. */
    fs_test test_mem_iteration;                     /* Tests directory iteration in memory. */
    fs_test test_mem_rename;                        /* Tests fs_rename() in memory. Make sure this is done before the remove test. */
    fs_test test_mem_remove;                        /* Tests fs_remove() in memory. This will delete test files. */
    fs_test test_mem_stress_test;                   /* Tests stress scenarios like many files and deep directories in memory. */
    fs_test test_mem_uninit;                        /* Needs to be last since this is where the fs_uninit() function is called for memory backend. */
    fs_test test_serialization;

    /* Test states. */
    fs_test_state test_system_state;
    fs_test_state test_mounts_state;
    fs_test_state test_archives_state;
    fs_test_state test_mem_state;
    fs_test_state test_serialization_state;

    /* Grab the backend so we can pass it into the test state. */
    pBackend = fs_test_get_backend(argc, argv);

    /* Print the backend name so we can easily see which backend is being used. */
    printf("Backend: %s\n", fs_test_get_backend_name(pBackend));

    /* Make sure the test states are initialized before running anything. */
    test_system_state        = fs_test_state_init(pBackend);
    test_mem_state           = fs_test_state_init(FS_MEM);  /* Memory backend uses its own backend type. */
    test_mounts_state        = fs_test_state_init(pBackend);
    test_archives_state      = fs_test_state_init(pBackend);
    test_serialization_state = fs_test_state_init(pBackend);

    /*
    Note that the order of tests is important here because we will use the output from previous tests
    as the input to subsequent tests.
    */

    /* Root. Only used for execution. */
    fs_test_init(&test_root, NULL, NULL, NULL, NULL);

    /* Paths. */
    fs_test_init(&test_path,                           "Path",                           NULL,                                   NULL,                 &test_root);
    fs_test_init(&test_path_iteration,                 "Path Iteration",                 fs_test_path_iteration,                 NULL,                 &test_path);
    fs_test_init(&test_path_normalize,                 "Path Normalize",                 fs_test_path_normalize,                 NULL,                 &test_path);
    fs_test_init(&test_path_trim_base,                 "Path Trim Base",                 fs_test_path_trim_base,                 NULL,                 &test_path);

    /*
    Default System IO.

    This only tests basic file operations. It does not test mounts or archives.
    */
    fs_test_init(&test_system,                         "System IO",                      NULL,                                   &test_system_state,   &test_root);
    fs_test_init(&test_system_sysdir,                  "System Directories",             fs_test_system_sysdir,                  &test_system_state,   &test_system);
    fs_test_init(&test_system_init,                    "Initialization",                 fs_test_system_init,                    &test_system_state,   &test_system);
    fs_test_init(&test_system_mktmp,                   "Temporaries",                    fs_test_system_mktmp,                   &test_system_state,   &test_system);
    fs_test_init(&test_system_mkdir,                   "Make Directory",                 fs_test_system_mkdir,                   &test_system_state,   &test_system);
    fs_test_init(&test_system_write,                   "Write",                          NULL,                                   &test_system_state,   &test_system);
    fs_test_init(&test_system_write_new,               "Write New",                      fs_test_system_write_new,               &test_system_state,   &test_system_write);
    fs_test_init(&test_system_write_overwrite,         "Write Overwrite",                fs_test_system_write_overwrite,         &test_system_state,   &test_system_write);
    fs_test_init(&test_system_write_append,            "Write Append",                   fs_test_system_write_append,            &test_system_state,   &test_system_write);
    fs_test_init(&test_system_write_exclusive,         "Write Exclusive",                fs_test_system_write_exclusive,         &test_system_state,   &test_system_write);
    fs_test_init(&test_system_write_truncate,          "Write Truncate",                 fs_test_system_write_truncate,          &test_system_state,   &test_system_write);
    fs_test_init(&test_system_write_seek,              "Write Seek",                     fs_test_system_write_seek,              &test_system_state,   &test_system_write);
    fs_test_init(&test_system_write_truncate2,         "fs_file_truncate()",             fs_test_system_write_truncate2,         &test_system_state,   &test_system_write);
    fs_test_init(&test_system_write_flush,             "Write Flush",                    fs_test_system_write_flush,             &test_system_state,   &test_system_write);
    fs_test_init(&test_system_read,                    "Read",                           fs_test_system_read,                    &test_system_state,   &test_system);
    fs_test_init(&test_system_read_readonly,           "Read Read-Only",                 fs_test_system_read_readonly,           &test_system_state,   &test_system_read);
    fs_test_init(&test_system_read_noexist,            "Read Non-Existent",              fs_test_system_read_noexist,            &test_system_state,   &test_system_read);
    fs_test_init(&test_system_duplicate,               "Duplicate",                      fs_test_system_duplicate,               &test_system_state,   &test_system);
    fs_test_init(&test_system_rename,                  "Rename",                         fs_test_system_rename,                  &test_system_state,   &test_system);
    fs_test_init(&test_system_remove,                  "Remove",                         fs_test_system_remove,                  &test_system_state,   &test_system);
    fs_test_init(&test_system_uninit,                  "Uninitialization",               fs_test_system_uninit,                  &test_system_state,   &test_system);

    /*
    Mounts.
    */
    fs_test_init(&test_mounts,                         "Mounts",                         fs_test_mounts,                         &test_mounts_state,   &test_root);
    fs_test_init(&test_mounts_write,                   "Mounts Write",                   fs_test_mounts_write,                   &test_mounts_state,   &test_mounts);
    fs_test_init(&test_mounts_read,                    "Mounts Read",                    fs_test_mounts_read,                    &test_mounts_state,   &test_mounts);
    fs_test_init(&test_mounts_mkdir,                   "Mounts Make Directory",          fs_test_mounts_mkdir,                   &test_mounts_state,   &test_mounts);
    fs_test_init(&test_mounts_rename,                  "Mounts Rename",                  fs_test_mounts_rename,                  &test_mounts_state,   &test_mounts);
    fs_test_init(&test_mounts_remove,                  "Mounts Remove",                  fs_test_mounts_remove,                  &test_mounts_state,   &test_mounts);
    fs_test_init(&test_mounts_iteration,               "Mounts Iteration",               fs_test_mounts_iteration,               &test_mounts_state,   &test_mounts);
    fs_test_init(&test_unmount,                        "Unmount",                        fs_test_unmount,                        &test_mounts_state,   &test_mounts);

    /*
    Archives.
    */
    fs_test_init(&test_archives,                       "Archives",                       fs_test_archives,                       &test_archives_state, &test_root);
    fs_test_init(&test_archives_opaque,                "Archives Opaque",                fs_test_archives_opaque,                &test_archives_state, &test_archives);
    fs_test_init(&test_archives_verbose,               "Archives Verbose",               fs_test_archives_verbose,               &test_archives_state, &test_archives);
    fs_test_init(&test_archives_transparent,           "Archives Transparent",           fs_test_archives_transparent,           &test_archives_state, &test_archives);
    fs_test_init(&test_archives_mount,                 "Archives Mounted",               fs_test_archives_mount,                 &test_archives_state, &test_archives);
    fs_test_init(&test_archives_iteration,             "Archives Iteration",             NULL,                                   &test_archives_state, &test_archives);
    fs_test_init(&test_archives_iteration_opaque,      "Archives Iteration Opaque",      fs_test_archives_iteration_opaque,      &test_archives_state, &test_archives_iteration);
    fs_test_init(&test_archives_iteration_verbose,     "Archives Iteration Verbose",     fs_test_archives_iteration_verbose,     &test_archives_state, &test_archives_iteration);
    fs_test_init(&test_archives_iteration_transparent, "Archives Iteration Transparent", fs_test_archives_iteration_transparent, &test_archives_state, &test_archives_iteration);
    fs_test_init(&test_archives_recursive,             "Archives Recursive",             fs_test_archives_recursive,             &test_archives_state, &test_archives);
    fs_test_init(&test_archives_uninit,                "Archives Uninitialization",      fs_test_archives_uninit,                &test_archives_state, &test_archives);

    /*
    Memory Backend Tests.

    This tests the in-memory file system backend independently from the system backend.
    */
    fs_test_init(&test_mem,                            "Memory Backend",                 NULL,                                   &test_mem_state,       &test_root);
    fs_test_init(&test_mem_init,                       "Memory Initialization",          fs_test_mem_init,                       &test_mem_state,       &test_mem);
    fs_test_init(&test_mem_mkdir,                      "Memory Make Directory",          fs_test_mem_mkdir,                      &test_mem_state,       &test_mem);
    fs_test_init(&test_mem_write,                      "Memory Write",                   fs_test_mem_write,                      &test_mem_state,       &test_mem);
    fs_test_init(&test_mem_write_new,                  "Memory Write New",               fs_test_mem_write_new,                  &test_mem_state,       &test_mem_write);
    fs_test_init(&test_mem_write_overwrite,            "Memory Write Overwrite",         fs_test_mem_write_overwrite,            &test_mem_state,       &test_mem_write);
    fs_test_init(&test_mem_write_append,               "Memory Write Append",            fs_test_mem_write_append,               &test_mem_state,       &test_mem_write);
    fs_test_init(&test_mem_write_exclusive,            "Memory Write Exclusive",         fs_test_mem_write_exclusive,            &test_mem_state,       &test_mem_write);
    fs_test_init(&test_mem_write_truncate,             "Memory Write Truncate",          fs_test_mem_write_truncate,             &test_mem_state,       &test_mem_write);
    fs_test_init(&test_mem_write_seek,                 "Memory Write Seek",              fs_test_mem_write_seek,                 &test_mem_state,       &test_mem_write);
    fs_test_init(&test_mem_write_truncate2,            "Memory fs_file_truncate()",      fs_test_mem_write_truncate2,            &test_mem_state,       &test_mem_write);
    fs_test_init(&test_mem_write_flush,                "Memory Write Flush",             fs_test_mem_write_flush,                &test_mem_state,       &test_mem_write);
    fs_test_init(&test_mem_read,                       "Memory Read",                    fs_test_mem_read,                       &test_mem_state,       &test_mem);
    fs_test_init(&test_mem_read_readonly,              "Memory Read Read-Only",          fs_test_mem_read_readonly,              &test_mem_state,       &test_mem_read);
    fs_test_init(&test_mem_read_noexist,               "Memory Read Non-Existent",       fs_test_mem_read_noexist,               &test_mem_state,       &test_mem_read);
    fs_test_init(&test_mem_duplicate,                  "Memory Duplicate",               fs_test_mem_duplicate,                  &test_mem_state,       &test_mem);
    fs_test_init(&test_mem_iteration,                  "Memory Iteration",               fs_test_mem_iteration,                  &test_mem_state,       &test_mem);
    fs_test_init(&test_mem_rename,                     "Memory Rename",                  fs_test_mem_rename,                     &test_mem_state,       &test_mem);
    fs_test_init(&test_mem_remove,                     "Memory Remove",                  fs_test_mem_remove,                     &test_mem_state,       &test_mem);
    fs_test_init(&test_mem_stress_test,                "Memory Stress Test",             fs_test_mem_stress_test,                &test_mem_state,       &test_mem);
    fs_test_init(&test_mem_uninit,                     "Memory Uninitialization",        fs_test_mem_uninit,                     &test_mem_state,       &test_mem);

    /*
    Serialization Tests.
    */
    fs_test_init(&test_serialization,                  "Serialization",                  fs_test_serialization,                  &test_serialization_state, &test_root);


    result = fs_test_run(&test_root);

    /* Print the test summary. */
    printf("\n");
    fs_test_print_summary(&test_root);

    if (result == FS_SUCCESS) {
        return 0;
    } else {
        return 1;
    }
}
