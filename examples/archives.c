/*
Shows basic usage of how to make use of archives with the library.

This examples depends on the ZIP backend which is located in the "extras" folder.

Like the hello_world example, this will load a file and print it to the console. The difference is
that this example will support the ability to load files from a ZIP archive.

When loading from an archive, you need to specify the backend and map it to a file extension. The
extension is what the library uses to determine which backend to use when opening a file.

To see this in action, consider specifying a file path like "archive.zip/file.txt". This will
ensure the file is loaded from the ZIP archive.
*/
#include "../fs.h"
#include "../extras/backends/zip/fs_zip.h"   /* <-- This is where FS_ZIP is declared. */

#include <stdio.h>

int main(int argc, char** argv)
{
    fs_result result;
    fs* pFS;
    fs_config fsConfig;
    fs_file* pFile;
    char buffer[4096];
    size_t bytesRead;
    fs_archive_type pArchiveTypes[] =
    {
        {FS_ZIP, "zip"}
    };

    if (argc < 2) {
        printf("Usage: archives <file>\n");
        return 1;
    }

    fsConfig = fs_config_init(FS_STDIO, NULL, NULL);
    fsConfig.pArchiveTypes    = pArchiveTypes;
    fsConfig.archiveTypeCount = sizeof(pArchiveTypes) / sizeof(pArchiveTypes[0]);

    result = fs_init(&fsConfig, &pFS);
    if (result != FS_SUCCESS) {
        printf("Failed to initialize file system: %d\n", result);
        return 1;
    }

    result = fs_file_open(pFS, argv[1], FS_READ, &pFile);
    if (result != FS_SUCCESS) {
        fs_uninit(pFS);
        printf("Failed to open file: %d\n", result);
        return 1;
    }

    while (fs_file_read(pFile, buffer, sizeof(buffer), &bytesRead) == FS_SUCCESS) {
        printf("%.*s", (int)bytesRead, buffer);
    }

    fs_file_close(pFile);
    fs_uninit(pFS);

    return 0;
}
