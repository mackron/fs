/*
Shows the most basic of basic usage of the library.

This will simply load a file from the default file system and print it to the console.

At the most basic level, a `fs` object is not required which is what this example demonstrates.
*/
#include "../fs.h"
#include <stdio.h>

int main(int argc, char** argv)
{
    fs_result result;
    fs_file* pFile;
    char buffer[4096];
    size_t bytesRead;

    if (argc < 2) {
        printf("Usage: hello_world <file>\n");
        return 1;
    }

    result = fs_file_open(NULL, argv[1], FS_READ, &pFile);
    if (result != FS_SUCCESS) {
        printf("Failed to open file: %d\n", result);
        return 1;
    }

    while (fs_file_read(pFile, buffer, sizeof(buffer), &bytesRead) == FS_SUCCESS) {
        printf("%.*s", (int)bytesRead, buffer);
    }

    fs_file_close(pFile);

    return 0;
}
