#include "../fs.h"
#include "../extras/backends/zip/fs_zip.h"
#include "../extras/backends/pak/fs_pak.h"

#include <stdio.h>
#include <string.h>

void print_help(void)
{
    printf("Usage: fsu [operation] [args]\n");
    printf("\n");
    printf("  extract <input file> <output path>\n");
}

fs_result extract_iterator(fs* pFS, fs* pArchive, fs_iterator* pIterator, const char* pFolderPath)
{
    fs_result result;
    
    while (pIterator != NULL) {
        /* TODO: Make this more robust by dynamically allocating on the heap as necessary. */
        char pFullPath[4096];
        fs_path_append(pFullPath, sizeof(pFullPath), pFolderPath, FS_NULL_TERMINATED, pIterator->pName, pIterator->nameLen);

        if (pIterator->info.directory) {
            printf("Directory: %s\n", pFullPath);

            fs_mkdir(pFS, pFullPath, 0);

            result = extract_iterator(pFS, pArchive, fs_first(pArchive, pFullPath, FS_OPAQUE), pFullPath);
            if (result != FS_SUCCESS) {
                printf("Failed to extract directory %s with code %d\n", pIterator->pName, result);
                return result;
            }
        } else {
            fs_file* pFileI;
            fs_file* pFileO;

            printf("File: %s\n", pFullPath);

            /* TODO: Don't think we need this check if we're extracting to a temp folder. Remove this when we've got temp folders implemented. */
            if (fs_info(pFS, pFullPath, FS_OPAQUE, NULL) == FS_SUCCESS) {
                printf("File %s already exists. Aborting.\n", pFullPath);
                return FS_SUCCESS;
            }

            result = fs_file_open(pFS, pFullPath, FS_TRUNCATE | FS_OPAQUE, &pFileO);
            if (result != FS_SUCCESS) {
                printf("Failed to open output file \"%s\" with code %d\n", pFullPath, result);
                return result;
            }


            result = fs_file_open(pArchive, pFullPath, FS_READ | FS_OPAQUE, &pFileI);
            if (result != FS_SUCCESS) {
                printf("Failed to open archived file \"%s\" with code %d\n", pFullPath, result);
                fs_file_close(pFileO);
                return result;
            }

            /* Now we just keep reading in chunk until we run out of data. */
            for (;;) {
                char chunk[4096];
                size_t bytesRead = 0;

                result = fs_file_read(pFileI, chunk, sizeof(chunk), &bytesRead);
                if (result != FS_SUCCESS) {
                    break;
                }

                if (bytesRead == 0) {
                    break;
                }

                result = fs_file_write(pFileO, chunk, bytesRead, NULL);
                if (result != FS_SUCCESS) {
                    break;
                }
            }

            fs_file_close(pFileI);
            fs_file_close(pFileO);

            if (result != FS_SUCCESS && result != FS_AT_END) {
                printf("Failed to read file \"%s\" with code %d\n", pFullPath, result);
                return result;
            }
        }
        
        pIterator = fs_next(pIterator);
    }

    return FS_SUCCESS;
}

int extract(int argc, char** argv)
{
    const char* pInputPath;
    const char* pOutputPath;
    fs_result result;
    fs* pFS;
    fs_config fsConfig;
    fs_archive_type pArchiveTypes[2];
    fs* pArchive;

    if (argc < 2) {
        printf("No input file.\n");
        return 1;
    }

    pInputPath = argv[1];
    
    if (argc > 2) {
        pOutputPath = argv[2];
    } else {
        pOutputPath = ".";
    }

    pArchiveTypes[0] = fs_archive_type_init(FS_ZIP, "zip");
    pArchiveTypes[1] = fs_archive_type_init(FS_PAK, "pak");

    fsConfig = fs_config_init_default();
    fsConfig.pArchiveTypes = pArchiveTypes;
    fsConfig.archiveTypeCount = sizeof(pArchiveTypes) / sizeof(pArchiveTypes[0]);

    result = fs_init(&fsConfig, &pFS);
    if (result != 0) {
        printf("Failed to initialize FS object with code %d\n", result);
        return 1;
    }

    /* Make sure the output directory exists. */
    result = fs_mkdir(pFS, pOutputPath, FS_IGNORE_MOUNTS);
    if (result != FS_SUCCESS) {
        printf("Failed to create output directory \"%s\" with code %d\n", pOutputPath, result);
        fs_uninit(pFS);
        return 1;
    }

    /* TODO: Extract to a temp folder and then move to the output target. */
    /*fs_mount(pFS, pTempPath, "NULL", FS_WRITE);*/
    fs_mount(pFS, pOutputPath, NULL, FS_WRITE);

    

    result = fs_open_archive(pFS, pInputPath, FS_READ | FS_OPAQUE, &pArchive);
    if (result != FS_SUCCESS) {
        printf("Failed to open archive \"%s\" with code %d\n", pInputPath, result);
        fs_uninit(pFS);
        return 1;
    }

    

    result = extract_iterator(pFS, pArchive, fs_first(pArchive, "/", FS_OPAQUE), "");
    if (result != FS_SUCCESS) {
        printf("Failed to extract archive with code %d\n", result);
        fs_uninit(pArchive);
        fs_uninit(pFS);
        return 1;
    }

    /* TODO: Use fs_rename() to move the files. Note that this does not look at mounts so we'll need to look at pTempPath and pOutputPath explicitly. */
    /* TODO: Delete the temp folder. */


    fs_close_archive(pArchive);
    fs_uninit(pFS);

    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        print_help();
        return 1;
    }

    if (strcmp(argv[1], "extract") == 0) {
        return extract(argc - 1, argv + 1);
    } else {
        print_help();
        return 1;
    }


    return 0;
}
