#include "../fs.c"

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

static int fs_test_reconstruct_path_forward(const fs_path_iterator* pIterator, size_t iteratorCount, char pPath[1024])
{
    size_t i;
    size_t len = 0;

    pPath[0] = '\0';

    for (i = 0; i < iteratorCount; i++) {
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

static int fs_test_reconstruct_path_reverse(const fs_path_iterator* pIterator, size_t iteratorCount, char pPath[1024])
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
    char pPathReconstructedForward[1024];
    char pPathReconstructedReverse[1024];
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
    char pNormalizedPath[1024];
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





/* BEG test_state */
typedef struct
{
    const fs_backend* pBackend;
    fs* pFS;
    char pTempDir[1024];
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
    char path[1024];
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
    char pTempFile[1024];

    if (pTestState->pFS == NULL) {
        printf("%s: File system not initialized. Aborting test.\n", pTest->name);
        return FS_ERROR;
    }

    /* Start with creating a temporary directory. If this works, the output from this will be where we output our test files going forward in future tests. */
    result = fs_mktmp(".fs_", pTestState->pTempDir, sizeof(pTestState->pTempDir), FS_MKTMP_DIR);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create temporary directory.\n", pTest->name);
        return FS_ERROR;
    }

    printf("%s: [DIR]  %s\n", pTest->name, pTestState->pTempDir);


    /* Now for a file. We just discard with this straight away. */
    result = fs_mktmp(".fs_", pTempFile, sizeof(pTempFile), FS_MKTMP_FILE);
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
    char pDirPath[1024];

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
    fs_file* pFile;
    fs_file_info fileInfo;
    char pFilePath[1024];
    const char data[4] = {1, 2, 3, 4};
    char dataRead[4];
    size_t bytesWritten;

    if (pTestState->pFS == NULL) {
        printf("%s: File system not initialized. Aborting test.\n", pTest->name);
        return FS_ERROR;
    }

    fs_path_append(pFilePath, sizeof(pFilePath), pTestState->pTempDir, (size_t)-1, "a", (size_t)-1);

    result = fs_file_open(pTestState->pFS, pFilePath, FS_WRITE | FS_TRUNCATE | FS_IGNORE_MOUNTS, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create new file.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_file_write(pFile, data, sizeof(data), &bytesWritten);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to write to new file.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    fs_file_close(pFile);

    if (bytesWritten != sizeof(data)) {
        printf("%s: ERROR: Expecting %d bytes written, but got %d.\n", pTest->name, (int)sizeof(data), (int)bytesWritten);
        return FS_ERROR;
    }


    /* Now we need to open the file and verify. */
    result = fs_file_open(pTestState->pFS, pFilePath, FS_READ, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file for reading.\n", pTest->name);
        return FS_ERROR;
    }

    /* The file should be exactly sizeof(data) bytes. */
    result = fs_file_get_info(pFile, &fileInfo);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to get file info.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    if (fileInfo.size != sizeof(data)) {
        printf("%s: ERROR: Expecting file size to be %d bytes, but got %u.\n", pTest->name, (int)sizeof(data), (unsigned int)fileInfo.size);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    result = fs_file_read(pFile, dataRead, sizeof(dataRead), &bytesWritten);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to read from file.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    if (bytesWritten != sizeof(dataRead)) {
        printf("%s: ERROR: Expecting %d bytes read, but got %d.\n", pTest->name, (int)sizeof(dataRead), (int)bytesWritten);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    fs_file_close(pFile);

    if (memcmp(dataRead, data, sizeof(dataRead)) != 0) {
        printf("%s: ERROR: Data read does not match data written.\n", pTest->name);
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
/* END system_write_new */

/* BEG system_write_overwrite */
static int fs_test_system_write_overwrite_internal(fs_test* pTest, char newData[4])
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    fs_file* pFile;
    fs_file_info fileInfo;
    char pFilePath[1024];
    char dataRead[4];
    size_t bytesRead;
    size_t newDataSize = 4;

    fs_path_append(pFilePath, sizeof(pFilePath), pTestState->pTempDir, (size_t)-1, "a", (size_t)-1);

    result = fs_file_open(pTestState->pFS, pFilePath, FS_WRITE | FS_IGNORE_MOUNTS, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file for overwriting.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_file_write(pFile, newData, newDataSize, NULL);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to write to file.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    fs_file_close(pFile);

    /* Now we need to open the file and verify. */
    result = fs_file_open(pTestState->pFS, pFilePath, FS_READ, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file for reading.\n", pTest->name);
        return FS_ERROR;
    }

    /* The file should be exactly sizeof(data) bytes. */
    result = fs_file_get_info(pFile, &fileInfo);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to get file info.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    if (fileInfo.size != newDataSize) {
        printf("%s: ERROR: Expecting file size to be %d bytes, but got %u.\n", pTest->name, (int)newDataSize, (unsigned int)fileInfo.size);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    result = fs_file_read(pFile, dataRead, sizeof(dataRead), &bytesRead);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to read from file.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    if (bytesRead != sizeof(dataRead)) {
        printf("%s: ERROR: Expecting %d bytes read, but got %d.\n", pTest->name, (int)sizeof(dataRead), (int)bytesRead);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    fs_file_close(pFile);

    if (memcmp(dataRead, newData, sizeof(dataRead)) != 0) {
        printf("%s: ERROR: Data read does not match data written.\n", pTest->name);
        return FS_ERROR;
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
    char pFilePath[1024];
    const char dataToAppend[4] = {5, 6, 7, 8};
    char dataExpected[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    char dataRead[8];
    size_t bytesWritten;

    if (pTestState->pFS == NULL) {
        printf("%s: File system not initialized. Aborting test.\n", pTest->name);
        return FS_ERROR;
    }

    fs_path_append(pFilePath, sizeof(pFilePath), pTestState->pTempDir, (size_t)-1, "a", (size_t)-1);

    result = fs_file_open(pTestState->pFS, pFilePath, FS_WRITE | FS_APPEND | FS_IGNORE_MOUNTS, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to create new file.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_file_write(pFile, dataToAppend, sizeof(dataToAppend), &bytesWritten);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to write to new file.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    fs_file_close(pFile);

    if (bytesWritten != sizeof(dataToAppend)) {
        printf("%s: ERROR: Expecting %d bytes written, but got %d.\n", pTest->name, (int)sizeof(dataToAppend), (int)bytesWritten);
        return FS_ERROR;
    }


    /* Now we need to open the file and verify. */
    result = fs_file_open(pTestState->pFS, pFilePath, FS_READ, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file for reading.\n", pTest->name);
        return FS_ERROR;
    }

    /* The file should be exactly sizeof(data) bytes. */
    result = fs_file_get_info(pFile, &fileInfo);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to get file info.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    if (fileInfo.size != sizeof(dataExpected)) {
        printf("%s: ERROR: Expecting file size to be %d bytes, but got %u.\n", pTest->name, (int)sizeof(dataExpected), (unsigned int)fileInfo.size);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    result = fs_file_read(pFile, dataRead, sizeof(dataRead), &bytesWritten);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to read from file.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    if (bytesWritten != sizeof(dataRead)) {
        printf("%s: ERROR: Expecting %d bytes read, but got %d.\n", pTest->name, (int)sizeof(dataRead), (int)bytesWritten);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    fs_file_close(pFile);

    if (memcmp(dataRead, dataExpected, sizeof(dataRead)) != 0) {
        printf("%s: ERROR: Data read does not match data written.\n", pTest->name);
        return FS_ERROR;
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
    char pFilePathA[1024];
    char pFilePathB[1024];

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
    fs_file_info fileInfo;
    char pFilePath[1024];
    size_t bytesWritten;
    size_t bytesRead;
    char data[4] = {5, 6, 7, 8};
    char dataRead[8];
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
    result = fs_file_open(pTestState->pFS, pFilePath, FS_READ, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open file for reading.\n", pTest->name);
        return FS_ERROR;
    }

    result = fs_file_get_info(pFile, &fileInfo);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to get file info.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    if (fileInfo.size != 8) {
        printf("%s: ERROR: Unexpected file size. Expected 8, got %u.\n", pTest->name, (unsigned int)fileInfo.size);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    result = fs_file_read(pFile, dataRead, sizeof(dataRead), &bytesRead);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to read from file.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    if (bytesRead != sizeof(dataRead)) {
        printf("%s: ERROR: Expecting %d bytes read, but got %d.\n", pTest->name, (int)sizeof(dataRead), (int)bytesRead);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    if (memcmp(dataRead, dataExpected, sizeof(dataExpected)) != 0) {
        printf("%s: ERROR: Data read from file does not match expected data.\n", pTest->name);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    fs_file_close(pFile);


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
    char pFilePath[1024];
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
    char pFilePath[1024];

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
    char pFilePath[1024];
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
    char pFilePath[1024];
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
    char pFilePath[1024];
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
    char pFilePathA[1024];
    char pFilePathC[1024];
    char pFilePathD[1024];
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
    
    for (pIterator = fs_first(pTestState->pFS, pDirPath, FS_IGNORE_MOUNTS); pIterator != NULL; pIterator = fs_next(pIterator)) {
        char pSubPath[1024];
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
    char pRootPath[128];
    char pDir1Path[128];
    char pDir2Path[128];
    char pDir3Path[128];

    fsConfig = fs_config_init(pTestState->pBackend, NULL, NULL);

    result = fs_init(&fsConfig, &pTestState->pFS);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to initialize file system.\n", pTest->name);
        return FS_ERROR;
    }

    /* We will do all of our mounting tests in a temp folder, so create that now. */
    result = fs_mktmp(".fs_mounts_", pTestState->pTempDir, sizeof(pTestState->pTempDir), FS_MKTMP_DIR);
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
    fs_result result;
    fs_file* pFile;

    result = fs_file_open(pTestState->pFS, pFilePath, FS_WRITE | FS_TRUNCATE | FS_NO_CREATE_DIRS, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open %s for writing.\n", pTest->name, pFilePath);
        return FS_ERROR;
    }

    /*
    We want to write out some data so we can correctly identify the data later when testing reads. The
    actual writing of data should have been tested earlier so no need to verify that.
    */
    result = fs_file_write(pFile, pData, dataSize, NULL);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to write to %s.\n", pTest->name, pFilePath);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    fs_file_close(pFile);

    return FS_SUCCESS;
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
    char pFilePath1[128];
    char pFilePath2[128];
    char pFilePath3[128];
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
    fs_result result;
    fs_file* pFile;
    char pActualData[16];
    size_t bytesRead;

    result = fs_file_open(pTestState->pFS, pFilePath, FS_READ, &pFile);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to open %s for reading.\n", pTest->name, pFilePath);
        return FS_ERROR;
    }

    /*
    We want to write out some data so we can correctly identify the data later when testing reads. The
    actual writing of data should have been tested earlier so no need to verify that.
    */
    result = fs_file_read(pFile, pActualData, expectedDataSize, &bytesRead);
    if (result != FS_SUCCESS) {
        printf("%s: Failed to read from %s.\n", pTest->name, pFilePath);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    /* Compare the data to ensure we read the correct file. */
    if (bytesRead != expectedDataSize || memcmp(pActualData, pExpectedData, expectedDataSize) != 0) {
        printf("%s: Data mismatch for %s.\n", pTest->name, pFilePath);
        fs_file_close(pFile);
        return FS_ERROR;
    }

    fs_file_close(pFile);

    return FS_SUCCESS;
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

/* BEG unmount */
int fs_test_unmount(fs_test* pTest)
{
    fs_test_state* pTestState = (fs_test_state*)pTest->pUserData;
    fs_result result;
    char pDir1Path[1024];
    char pDir2Path[1024];
    char pDir3Path[1024];

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


int main(int argc, char** argv)
{
    int result;
    const fs_backend* pBackend;

    /* Tests. */
    fs_test test_root;
    fs_test test_path;
    fs_test test_path_iteration;            /* Tests path breakup logic. This is critical for some internal logic in the library. */
    fs_test test_path_normalize;            /* Tests path normalization, like resolving ".." and "." segments. Again, this is used extensively for path validation and therefore needs proper testing. */
    fs_test test_system;
    fs_test test_system_sysdir;             /* Standard directory tests need to come first because we'll be writing out our test files to a temp folder. */
    fs_test test_system_init;
    fs_test test_system_mktmp;              /* The output from this test will be used for subsequent tests. Tests will output into the temp directory created here. */
    fs_test test_system_mkdir;
    fs_test test_system_write;
    fs_test test_system_write_new;          /* Tests creation of a new file. */
    fs_test test_system_write_overwrite;    /* Tests FS_WRITE (overwrite mode). */
    fs_test test_system_write_append;       /* Tests FS_WRITE | FS_APPEND. */
    fs_test test_system_write_exclusive;    /* Tests FS_WRITE | FS_EXCLUSIVE. */
    fs_test test_system_write_truncate;     /* Tests FS_WRITE | FS_TRUNCATE. */
    fs_test test_system_write_truncate2;    /* Tests fs_file_truncate(). */
    fs_test test_system_write_seek;         /* Tests seeking while writing. */
    fs_test test_system_write_flush;        /* Tests fs_file_flush(). */
    fs_test test_system_read;               /* Tests FS_READ. Also acts as the parent test for other reading related tests. */
    fs_test test_system_read_readonly;      /* Tests that writing to a read-only file fails. */
    fs_test test_system_read_noexist;       /* Tests that reading a non-existent file fails cleanly. */
    fs_test test_system_duplicate;          /* Tests fs_file_duplicate(). */
    fs_test test_system_rename;             /* Tests fs_rename(). Make sure this is done before the remove test. */
    fs_test test_system_remove;             /* Tests fs_remove(). This will delete all of the test files we created earlier. Therefore it should be the last test, before uninitialization. */
    fs_test test_system_uninit;             /* Needs to be last since this is where the fs_uninit() function is called. */
    fs_test test_mounts;                    /* The top-level test for mounts. This will set up the `fs` object and the folder and file structure in preparation for subsequent tests. */
    fs_test test_mounts_write;              /* Tests writing to mounts. */
    fs_test test_mounts_read;               /* Tests reading from mounts. */
    fs_test test_mounts_mkdir;              /* Tests creating directories with mounts. */
    fs_test test_unmount;                   /* This needs to be the last mount test.*/

    /* Test states. */
    fs_test_state test_system_state;
    fs_test_state test_mounts_state;

    /* Grab the backend so we can pass it into the test state. */
    pBackend = fs_test_get_backend(argc, argv);

    /* Print the backend name so we can easily see which backend is being used. */
    printf("Backend: %s\n", fs_test_get_backend_name(pBackend));

    /* Make sure the test states are initialized before running anything. */
    test_system_state = fs_test_state_init(pBackend);
    test_mounts_state = fs_test_state_init(pBackend);


    /*
    Note that the order of tests is important here because we will use the output from previous tests
    as the input to subsequent tests.
    */

    /* Root. Only used for execution. */
    fs_test_init(&test_root, NULL, NULL, NULL, NULL);

    /* Paths. */
    fs_test_init(&test_path,           "Path",           NULL,                   NULL, &test_root);
    fs_test_init(&test_path_iteration, "Path Iteration", fs_test_path_iteration, NULL, &test_path);
    fs_test_init(&test_path_normalize, "Path Normalize", fs_test_path_normalize, NULL, &test_path);

    /*
    Default System IO.

    This only tests basic file operations. It does not test mounts or archives.
    */
    fs_test_init(&test_system,                 "System IO",          NULL,                           &test_system_state, &test_root);
    fs_test_init(&test_system_sysdir,          "System Directories", fs_test_system_sysdir,          &test_system_state, &test_system);
    fs_test_init(&test_system_init,            "Initialization",     fs_test_system_init,            &test_system_state, &test_system);
    fs_test_init(&test_system_mktmp,           "Temporaries",        fs_test_system_mktmp,           &test_system_state, &test_system);
    fs_test_init(&test_system_mkdir,           "Make Directory",     fs_test_system_mkdir,           &test_system_state, &test_system);
    fs_test_init(&test_system_write,           "Write",              NULL,                           &test_system_state, &test_system);
    fs_test_init(&test_system_write_new,       "Write New",          fs_test_system_write_new,       &test_system_state, &test_system_write);
    fs_test_init(&test_system_write_overwrite, "Write Overwrite",    fs_test_system_write_overwrite, &test_system_state, &test_system_write);
    fs_test_init(&test_system_write_append,    "Write Append",       fs_test_system_write_append,    &test_system_state, &test_system_write);
    fs_test_init(&test_system_write_exclusive, "Write Exclusive",    fs_test_system_write_exclusive, &test_system_state, &test_system_write);
    fs_test_init(&test_system_write_truncate,  "Write Truncate",     fs_test_system_write_truncate,  &test_system_state, &test_system_write);
    fs_test_init(&test_system_write_seek,      "Write Seek",         fs_test_system_write_seek,      &test_system_state, &test_system_write);
    fs_test_init(&test_system_write_truncate2, "fs_file_truncate()", fs_test_system_write_truncate2, &test_system_state, &test_system_write);
    fs_test_init(&test_system_write_flush,     "Write Flush",        fs_test_system_write_flush,     &test_system_state, &test_system_write);
    fs_test_init(&test_system_read,            "Read",               fs_test_system_read,            &test_system_state, &test_system);
    fs_test_init(&test_system_read_readonly,   "Read Read-Only",     fs_test_system_read_readonly,   &test_system_state, &test_system_read);
    fs_test_init(&test_system_read_noexist,    "Read Non-Existent",  fs_test_system_read_noexist,    &test_system_state, &test_system_read);
    fs_test_init(&test_system_duplicate,       "Duplicate",          fs_test_system_duplicate,       &test_system_state, &test_system);
    fs_test_init(&test_system_rename,          "Rename",             fs_test_system_rename,          &test_system_state, &test_system);
    fs_test_init(&test_system_remove,          "Remove",             fs_test_system_remove,          &test_system_state, &test_system);
    fs_test_init(&test_system_uninit,          "Uninitialization",   fs_test_system_uninit,          &test_system_state, &test_system);

    /*
    Mounts.
    */
    fs_test_init(&test_mounts,       "Mounts",                fs_test_mounts,       &test_mounts_state, &test_root);
    fs_test_init(&test_mounts_write, "Mounts Write",          fs_test_mounts_write, &test_mounts_state, &test_mounts);
    fs_test_init(&test_mounts_read,  "Mounts Read",           fs_test_mounts_read,  &test_mounts_state, &test_mounts);
    fs_test_init(&test_mounts_mkdir, "Mounts Make Directory", fs_test_mounts_mkdir, &test_mounts_state, &test_mounts);
    fs_test_init(&test_unmount,      "Unmount",               fs_test_unmount,      &test_mounts_state, &test_mounts);

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
