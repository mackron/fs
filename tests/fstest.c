#include "../fs.h"
#include "../extras/backends/zip/fs_zip.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

FS_API int fs_strncpy(char* dst, const char* src, size_t count);    /* <-- This is not forward declared in the header. It exists in the .c file though. */


static void fstest_print_result_v(const char* pPattern, int result, va_list args)
{
    const char* pResultStr = (result == 0) ? "PASS" : "FAIL";
    char pBuffer[1024];
    char pSpaces[1024];

    memset(pSpaces, ' ', sizeof(pSpaces));
    vsnprintf(pBuffer, 1024, pPattern, args);

    printf("%s%s%.*s%s\033[0m\n", ((result == 0) ? "\033[32m" : "\033[31m"), pBuffer, 80 - 4 - (int)strlen(pBuffer), pSpaces, pResultStr);
}

static void fstest_print_result_f(const char* pPattern, int result, ...)
{
    va_list args;
    va_start(args, result);
    {
        fstest_print_result_v(pPattern, result, args);
    }
    va_end(args);
}




static int fstest_breakup_path_forward(const char* pPath, size_t pathLen, fs_path_iterator pIterator[32], size_t* pCount)
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

static int fstest_breakup_path_reverse(const char* pPath, size_t pathLen, fs_path_iterator pIterator[32], size_t* pCount)
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

static int fstest_reconstruct_path_forward(const fs_path_iterator* pIterator, size_t iteratorCount, char pPath[1024])
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

static int fstest_reconstruct_path_reverse(const fs_path_iterator* pIterator, size_t iteratorCount, char pPath[1024])
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

static int fstest_path(const char* pPath)
{
    fs_path_iterator segmentsForward[32];
    fs_path_iterator segmentsReverse[32];
    size_t segmentsForwardCount;
    size_t segmentsReverseCount;
    char pPathReconstructedForward[1024];
    char pPathReconstructedReverse[1024];
    int forwardResult = 0;
    int reverseResult = 0;

    printf("Path: \"%s\"\n", pPath);

    fstest_breakup_path_forward(pPath, (size_t)-1, segmentsForward, &segmentsForwardCount);
    fstest_breakup_path_reverse(pPath, (size_t)-1, segmentsReverse, &segmentsReverseCount);

    fstest_reconstruct_path_forward(segmentsForward, segmentsForwardCount, pPathReconstructedForward);
    fstest_reconstruct_path_reverse(segmentsReverse, segmentsReverseCount, pPathReconstructedReverse);

    if (strcmp(pPath, pPathReconstructedForward) != 0) {
        forwardResult = 1;
    }
    if (strcmp(pPath, pPathReconstructedReverse) != 0) {
        reverseResult = 1;
    }

    fstest_print_result_f("  Forward: \"%s\"", forwardResult, pPathReconstructedForward);
    fstest_print_result_f("  Reverse: \"%s\"", reverseResult, pPathReconstructedReverse);

    if (forwardResult == 0 && reverseResult == 0) {
        return 0;
    } else {
        return 1;
    }
}

static int fstest_path_normalize(const char* pPath, const char* pExpected)
{
    char pNormalizedPath[1024];
    int result;

    printf("Path: \"%s\" = \"%s\"\n", pPath, (pExpected == NULL) ? "ERROR" : pExpected);
    result = fs_path_normalize(pNormalizedPath, sizeof(pNormalizedPath), pPath, FS_NULL_TERMINATED);
    if (result < 0) {
        if (pExpected != NULL) {
            fstest_print_result_f("  Normalized: %s", 1, "ERROR");
            return 1;
        } else {
            fstest_print_result_f("  Normalized: %s", 0, "ERROR");
            return 0;
        }
    }

    result = strcmp(pNormalizedPath, pExpected) != 0;
    fstest_print_result_f("  Normalized: \"%s\"", result, pNormalizedPath);

    /* Just for placing a breakpoint for debugging. */
    if (result == 1) {
        return 1;
    }

    return result;
}

static int fstest_paths()
{
    int result = 0;

    result |= fstest_path("/");
    result |= fstest_path("");
    result |= fstest_path("/abc");
    result |= fstest_path("/abc/");
    result |= fstest_path("abc/");
    result |= fstest_path("/abc/def/ghi");
    result |= fstest_path("/abc/def/ghi/");
    result |= fstest_path("abc/def/ghi/");
    result |= fstest_path("C:");
    result |= fstest_path("C:/");
    result |= fstest_path("C:/abc");
    result |= fstest_path("C:/abc/");
    result |= fstest_path("C:/abc/def/ghi");
    result |= fstest_path("C:/abc/def/ghi/");
    result |= fstest_path("//localhost");
    result |= fstest_path("//localhost/abc");
    result |= fstest_path("//localhost//abc");
    result |= fstest_path("~");
    result |= fstest_path("~/Documents");

    printf("\n");
    printf("Path Normalization\n");
    result |= fstest_path_normalize("", "");
    result |= fstest_path_normalize("/", "/");
    result |= fstest_path_normalize("/abc/def/ghi", "/abc/def/ghi");
    result |= fstest_path_normalize("/..", NULL);   /* Expecting error. */
    result |= fstest_path_normalize("..", "..");
    result |= fstest_path_normalize("abc/../def", "def");
    result |= fstest_path_normalize("abc/./def", "abc/def");
    result |= fstest_path_normalize("../abc/def", "../abc/def");
    result |= fstest_path_normalize("abc/def/..", "abc");
    result |= fstest_path_normalize("abc/../../def", "../def");
    result |= fstest_path_normalize("/abc/../../def", NULL);   /* Expecting error. */
    result |= fstest_path_normalize("abc/def/", "abc/def");
    result |= fstest_path_normalize("/abc/def/", "/abc/def");

    printf("\n");

    if (result == 0) {
        return 0;
    } else {
        return 1;
    }
}



static int fstest_default_io()
{
    /* TODO: Implement me. */

    return 0;
}

static int fstest_archive_io_file(fs* pFS, const char* pFilePath, const char* pOutputDirectory, int openMode)
{
    fs_result result;
    fs_file_info fileInfo;
    fs_file* pFileIn;
    fs_file* pFileOut;
    char pOutputFilePath[1024];
    fs_result readResult = FS_SUCCESS;
    fs_result writeResult = FS_SUCCESS;
    fs_uint64 totalBytesRead;

    result = fs_info(pFS, pFilePath, FS_READ | openMode, &fileInfo);
    fstest_print_result_f("  Info      %s", (result != FS_SUCCESS), pFilePath);
    if (result != 0) {
        return 1;
    }

    result = fs_file_open(pFS, pFilePath, FS_READ | openMode, &pFileIn);
    fstest_print_result_f("  Open      %s", (result != FS_SUCCESS), pFilePath);
    if (result != 0) {
        return 1;
    }

    result = fs_file_get_info(pFileIn, &fileInfo);
    fstest_print_result_f("  File Info %s", (result != FS_SUCCESS), pFilePath);
    if (result != 0) {
        fs_file_close(pFileIn);
        return 1;
    }

    snprintf(pOutputFilePath, sizeof(pOutputFilePath), "%s/%s", pOutputDirectory, fs_path_file_name(pFilePath, (size_t)-1));
    result = fs_file_open(pFS, pOutputFilePath, FS_WRITE | FS_TRUNCATE, &pFileOut);
    fstest_print_result_f("  Open      %s", (result != FS_SUCCESS), pOutputFilePath);
    if (result != 0) {
        fs_file_close(pFileIn);
        return 1;
    }

    totalBytesRead = 0;
    for (;;) {
        char chunk[4096];
        size_t bytesRead = 0;

        result = fs_file_read(pFileIn, chunk, sizeof(chunk), &bytesRead);
        if (result != FS_SUCCESS) {
            if (result == FS_AT_END && bytesRead == 0) {
                readResult = FS_SUCCESS;    /* Don't present an error for an EOF condition. */
            } else {
                readResult = result;
            }

            break;
        }

        if (bytesRead == 0) {
            break;
        }

        totalBytesRead += bytesRead;
        
        result = fs_file_write(pFileOut, chunk, bytesRead, NULL);
        if (result != FS_SUCCESS) {
            writeResult = result;
            break;
        }
    }

    fstest_print_result_f("  Read      %s",   (readResult  != FS_SUCCESS), pFilePath);
    fstest_print_result_f("  Write     %s",   (writeResult != FS_SUCCESS), pOutputFilePath);
    fstest_print_result_f("  Bytes     %llu", (totalBytesRead != fileInfo.size), totalBytesRead);


    fs_file_close(pFileIn);
    fs_file_close(pFileOut);
    
    return 0;
}

static int fstest_archive_io()
{
    int result;
    fs* pFS;
    fs_config fsConfig;
    fs_archive_type pArchiveTypes[] =
    {
        {FS_ZIP, "zip"},
        {FS_ZIP, "pac"}
    };

    printf("Archive I/O\n");

    fsConfig = fs_config_init_default();
    fsConfig.pArchiveTypes    = pArchiveTypes;
    fsConfig.archiveTypeCount = sizeof(pArchiveTypes) / sizeof(pArchiveTypes[0]);

    result = fs_init(&fsConfig, &pFS) != FS_SUCCESS;
    fstest_print_result_f("  FS_STDIO Initialization", (result != FS_SUCCESS));
    if (result != FS_SUCCESS) {
        return 1;
    }

    /* Test archives in archives. */
    {
        fs_file* pFile;
        
        result = fs_file_open(pFS, "testvectors/testvectors2.zip/testvectors.zip/miniaudio.h", FS_READ | FS_VERBOSE, &pFile);
        if (result != FS_SUCCESS) {
            printf("Failed to open file.\n");
            return 1;
        }
    }

    //fs_register_archive_type(pFS, FS_ZIP, "zip");
    //fs_register_archive_type(pFS, FS_ZIP, "pac");

    fs_mount_write(pFS, "testvectors/extracted", NULL, FS_MOUNT_PRIORITY_HIGHEST);

    //fs_mount(pFS, "test", NULL, FS_MOUNT_PRIORITY_HIGHEST);
    //fs_mount(pFS, "blah", NULL, FS_MOUNT_PRIORITY_LOWEST);

    result |= fstest_archive_io_file(pFS, "testvectors/testvectors.zip/miniaudio.h", "", FS_VERBOSE);
    result |= fstest_archive_io_file(pFS, "testvectors/miniaudio.h",                 "", FS_TRANSPARENT);
    result |= fstest_archive_io_file(pFS, "testvectors/testvectors.zip/miniaudio.h", "", FS_TRANSPARENT); /* Files opened in transparent mode must still support verbose paths. */

#if 1
    /* Mounted tests. TODO: Improve these. Make a separate test. */
    if (fs_mount(pFS, "testvectors", NULL, FS_MOUNT_PRIORITY_HIGHEST) != FS_SUCCESS) { printf("FAILED TO MOUNT 'testvectors'\n"); }
    {
        result |= fstest_archive_io_file(pFS, "testvectors.zip/miniaudio.h", "", FS_VERBOSE);
    }
    fs_unmount(pFS, "testvectors");

    if (fs_mount(pFS, "testvectors/testvectors.zip", NULL, FS_MOUNT_PRIORITY_HIGHEST) != FS_SUCCESS) { printf("FAILED TO MOUNT 'testvectors/testvectors.zip'\n"); }
    {
        result |= fstest_archive_io_file(pFS, "miniaudio.h", "", FS_VERBOSE);
    }
    fs_unmount(pFS, "testvectors/testvectors.zip");
#endif

    fs_uninit(pFS);
    return result;
}

static int fstest_write_io()
{
    int result;
    fs* pFS;
    fs_config fsConfig;

    printf("Write I/O\n");

    fsConfig = fs_config_init_default();

    result = fs_init(&fsConfig, &pFS) != FS_SUCCESS;
    fstest_print_result_f("  FS_STDIO Initialization", (result != FS_SUCCESS));
    if (result != FS_SUCCESS) {
        return 1;
    }

    fs_mount_write(pFS, "testvectors/write",               NULL,            FS_MOUNT_PRIORITY_HIGHEST);
    fs_mount_write(pFS, "testvectors/write/config",        "config",        FS_MOUNT_PRIORITY_HIGHEST);
    fs_mount_write(pFS, "testvectors/write/config/editor", "config/editor", FS_MOUNT_PRIORITY_HIGHEST);

    {
        fs_file* pFile;
        const char* pContent = "Hello, World!";

        result = fs_file_open(pFS, "config/editor/editor.cfg", FS_WRITE | FS_TRUNCATE, &pFile) != FS_SUCCESS;
        if (result != 0) {
            printf("Failed to open file for writing.\n");
            return 1;
        }

        result = fs_file_write(pFile, pContent, strlen(pContent), NULL) != FS_SUCCESS;
        if (result != 0) {
            printf("Failed to write to file.\n");
            return 1;
        }

        fs_file_close(pFile);
    }

    return 0;
}

static int fstest_iteration()
{
    int result;
    fs* pFS;
    fs_config fsConfig;
    fs_archive_type pArchiveTypes[] =
    {
        {FS_ZIP, "zip"}
    };

    printf("Iteration\n");

    /* Default iteration. */
    printf("  Default\n");
    {
        fs_iterator* pIterator;

        for (pIterator = fs_first(NULL, "", 0); pIterator != NULL; pIterator = fs_next(pIterator)) {
            printf("    %s\n", pIterator->pName);
        }
    }


    fsConfig = fs_config_init_default();
    fsConfig.pArchiveTypes = pArchiveTypes;
    fsConfig.archiveTypeCount = sizeof(pArchiveTypes) / sizeof(pArchiveTypes[0]);

    result = fs_init(&fsConfig, &pFS);

    /* Iteration with registered archives. */
    printf("  With Archives\n");
    result = fs_init(&fsConfig, &pFS) != FS_SUCCESS;
    fstest_print_result_f("    FS_STDIO Initialization", (result != FS_SUCCESS));
    if (result != FS_SUCCESS) {
        return 1;
    }

    fs_mount(pFS, "testvectors", NULL, FS_MOUNT_PRIORITY_HIGHEST);

    {
        fs_iterator* pIterator;

        for (pIterator = fs_first(pFS, "iteration", FS_TRANSPARENT); pIterator != NULL; pIterator = fs_next(pIterator)) {
            printf("    %s\n", pIterator->pName);
        }
    }

    return 0;

}

static int fstest_io()
{
    int result = 0;

    result |= fstest_default_io();
    result |= fstest_archive_io();
    result |= fstest_write_io();
    result |= fstest_iteration();

    if (result == 0) {
        return 0;
    } else {
        return 1;
    }
}


int main(int argc, char** argv)
{
    fstest_paths();
    //fstest_io();

    (void)argc;
    (void)argv;

    return 0;
}