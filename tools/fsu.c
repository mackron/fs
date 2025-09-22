#include "../fs.h"
#include "../extras/backends/zip/fs_zip.h"
#include "../extras/backends/pak/fs_pak.h"
#include "../extras/backends/mem/fs_mem.h"

#include <stdio.h>
#include <string.h>

void print_help(void)
{
    printf("Usage: fsu [operation] [args]\n");
    printf("\n");
    printf("unpack <input file> <output path>\n");
    printf("  Unpacks the contents of an archive to the specified output path.\n");
    printf("\n");
    printf("pack <input directory>\n");
    printf("  Reads the contents of the specified directory and packs it into an\n");
    printf("  archive which can later be unpacked with the 'unpack' command. Outputs\n");
    printf("  to stdout.\n");
}

fs_result unpack_iterator(fs* pFS, fs* pArchive, fs_iterator* pIterator, const char* pFolderPath)
{
    fs_result result;
    
    while (pIterator != NULL) {
        /* TODO: Make this more robust by dynamically allocating on the heap as necessary. */
        char pFullPath[4096];
        fs_path_append(pFullPath, sizeof(pFullPath), pFolderPath, FS_NULL_TERMINATED, pIterator->pName, pIterator->nameLen);

        if (pIterator->info.directory) {
            printf("Directory: %s\n", pFullPath);

            fs_mkdir(pFS, pFullPath, 0);

            result = unpack_iterator(pFS, pArchive, fs_first(pArchive, pFullPath, FS_OPAQUE), pFullPath);
            if (result != FS_SUCCESS) {
                printf("Failed to unpack directory %s with code %d\n", pIterator->pName, result);
                return result;
            }
        } else {
            fs_file* pFileI;
            fs_file* pFileO;

            printf("File: %s\n", pFullPath);

            /* TODO: Don't think we need this check if we're unpacking to a temp folder. Remove this when we've got temp folders implemented. */
            if (fs_info(pFS, pFullPath, FS_OPAQUE, NULL) == FS_SUCCESS) {
                printf("File %s already exists. Aborting.\n", pFullPath);
                return FS_SUCCESS;
            }

            result = fs_file_open(pFS, pFullPath, FS_WRITE | FS_OPAQUE, &pFileO);
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

int unpack(int argc, char** argv)
{
    const char* pInputPath;
    const char* pOutputPath;
    fs_result result;
    fs_config fsConfig;
    fs* pFS;
    fs_file* pArchiveFile;
    fs_config archiveConfig;
    fs* pArchive;
    size_t iBackend;
    const fs_backend* pBackends[2];

    /* List backends in priority order. */
    pBackends[0] = FS_ZIP;
    pBackends[1] = FS_PAK;

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

    fsConfig = fs_config_init_default();

    result = fs_init(&fsConfig, &pFS);
    if (result != 0) {
        printf("Failed to initialize FS object with code %d\n", result);
        return 1;
    }

    /* The first thing to do is open the file of the archive itself. */
    result = fs_file_open(pFS, pInputPath, FS_READ | FS_OPAQUE | FS_IGNORE_MOUNTS, &pArchiveFile);
    if (result != FS_SUCCESS) {
        printf("Failed to open archive file \"%s\": %s\n", pInputPath, fs_result_description(result));
        fs_uninit(pFS);
        return 1;
    }

    /*
    We'll now use trial and error to find a suitable backend. We'll just use the first one that works. Not
    the most robust way of doing it, but it works for my needs.

    We'll use a special case here for our serialized format. We'll try seeking to the end and read the
    tail to see if we can find the signature, and if so just assume we're dealing with a serialized archive.
    */
    pArchive = NULL;

    result = fs_file_seek(pArchiveFile, -24, FS_SEEK_END);
    if (result == FS_SUCCESS) {
        char sig[8];
        size_t bytesRead = 0;

        result = fs_file_read(pArchiveFile, sig, sizeof(sig), &bytesRead);
        if (result == FS_SUCCESS && bytesRead == sizeof(sig)) {
            if (memcmp(sig, "FSSRLZ1\0", sizeof(sig)) == 0) {
                /*
                Looks like a serialized archive. In this case our pArchive fs will use the FS_MEM backend and we'll
                deserialize into it before extracting. Inefficient, but does not matter for my purposes.
                */
                archiveConfig = fs_config_init(FS_MEM, NULL, NULL);

                result = fs_init(&archiveConfig, &pArchive);
                if (result != FS_SUCCESS) {
                    printf("Failed to initialize memory archive for serialized data: %s\n", fs_result_description(result));
                    fs_file_close(pArchiveFile);
                    fs_uninit(pFS);
                    return 1;
                }

                result = fs_deserialize(pArchive, NULL, FS_IGNORE_MOUNTS, fs_file_get_stream(pArchiveFile));
                if (result != FS_SUCCESS) {
                    printf("Failed to deserialize archive with code %s\n", fs_result_description(result));
                    fs_uninit(pArchive);
                    fs_file_close(pArchiveFile);
                    fs_uninit(pFS);
                    return 1;
                }
            } else {
                /* Not a serialized archive. */
            }
        } else {
            /* Failed to read the signature. Assume it's not a serialized archived. */
        }
    } else {
        /* Seeking failed. Assume it's not a serialized archived. */
    }

    /* If at this point we don't have an archive it means it did not come from fs_serialize(). We'll see it's a regular archive. */
    if (pArchive == NULL) {
        /* Make sure we seek back to the start of the file before attempting to open from regular backends */
        fs_file_seek(pArchiveFile, 0, FS_SEEK_SET);

        for (iBackend = 0; iBackend < sizeof(pBackends) / sizeof(pBackends[0]); iBackend++) {
            archiveConfig = fs_config_init(pBackends[iBackend], NULL, fs_file_get_stream(pArchiveFile));

            result = fs_init(&archiveConfig, &pArchive);
            if (result == FS_SUCCESS) {
                break;
            }
        }
    }
    
    if (pArchive == NULL) {
        printf("Failed to find a suitable backend for archive \"%s\"\n", pInputPath);
        fs_file_close(pArchiveFile);
        fs_uninit(pFS);
        return 1;
    }



    /* Make sure the output directory exists. */
    result = fs_mkdir(pFS, pOutputPath, FS_IGNORE_MOUNTS);
    if (result != FS_SUCCESS && result != FS_ALREADY_EXISTS) {
        printf("Failed to create output directory \"%s\" with code %d\n", pOutputPath, result);
        fs_uninit(pFS);
        return 1;
    }

    /* TODO: Extract to a temp folder and then move to the output target. */
    /*fs_mount(pFS, pTempPath, "NULL", FS_WRITE);*/
    fs_mount(pFS, pOutputPath, NULL, FS_WRITE);


    result = unpack_iterator(pFS, pArchive, fs_first(pArchive, "/", FS_OPAQUE), "");
    if (result != FS_SUCCESS) {
        printf("Failed to unpack archive with code %d\n", result);
        fs_uninit(pArchive);
        fs_uninit(pFS);
        return 1;
    }

    /* TODO: Use fs_rename() to move the files. */
    /* TODO: Delete the temp folder. */


    fs_uninit(pArchive);
    fs_file_close(pArchiveFile);
    fs_uninit(pFS);

    return 0;
}

int pack(int argc, char** argv)
{
    fs_result result;
    const char* pDirectoryPath;
    fs_file* pOutputFile;

    if (argc < 2) {
        printf("No input directory.\n");
        return 1;
    }

    pDirectoryPath = argv[1];

    if (argc > 2) {
        result = fs_file_open(NULL, argv[2], FS_WRITE | FS_TRUNCATE, &pOutputFile);
        if (result != FS_SUCCESS) {
            printf("Failed to open output file \"%s\": %s\n", argv[2], fs_result_description(result));
            return 1;
        }
    } else {
        result = fs_file_open(NULL, FS_STDOUT, FS_WRITE, &pOutputFile);
        if (result != FS_SUCCESS) {
            printf("Failed to open stdout: %s\n", fs_result_description(result));
            return 1;
        }
    }

    result = fs_serialize(NULL, pDirectoryPath, FS_OPAQUE | FS_IGNORE_MOUNTS, fs_file_get_stream(pOutputFile));
    if (result != FS_SUCCESS) {
        printf("Failed to serialize directory \"%s\": %s\n", pDirectoryPath, fs_result_description(result));
        fs_file_close(pOutputFile);
        return 1;
    }

    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        print_help();
        return 1;
    }

    /*  */ if (strcmp(argv[1], "unpack") == 0) {
        return unpack(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "pack") == 0) {
        return pack(argc - 1, argv + 1);
    } else {
        print_help();
        return 1;
    }


    return 0;
}
