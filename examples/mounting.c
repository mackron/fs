/*
Basic demonstration of how to mount directories.

Mounting a directory essentially adds it as a search path when opening files. You can mount
multiple directories to the same mount point, in which case it's prioritized by the order in
which they were mounted.

In this example we will mount the following folders to the mount point "mnt":

    "testvectors/mounting/src1"
    "testvectors/mounting/src2"

The latter will take precedence over the former, so if a file exists in both directories, the
one in "src2" will be loaded. You can swap these around to see the difference.

This example uses "mnt" as the mount point, but you can use anything you like, including an empty
string. You just need to remember to use the same mount point when specifying the path of the file
to open.

This examples omits some cleanup for the sake of brevity.
*/
#include "../fs.h"
#include <stdio.h>

int main(int argc, char** argv)
{
    fs_result result;
    fs* pFS;
    fs_file* pFile;
    char buffer[4096];
    size_t bytesRead;

    result = fs_init(NULL, &pFS);
    if (result != FS_SUCCESS) {
        printf("Failed to initialize file system: %d\n", result);
        return 1;
    }

    result = fs_mount(pFS, "testvectors/mounting/src1", "mnt", FS_READ);
    if (result != FS_SUCCESS) {
        printf("Failed to mount directory: %d\n", result);
        return 1;
    }

    result = fs_mount(pFS, "testvectors/mounting/src2", "mnt", FS_READ);
    if (result != FS_SUCCESS) {
        printf("Failed to mount directory: %d\n", result);
        return 1;
    }

    result = fs_file_open(pFS, "mnt/hello", FS_READ, &pFile);
    if (result != FS_SUCCESS) {
        printf("Failed to open file: %d\n", result);
        return 1;
    }

    while (fs_file_read(pFile, buffer, sizeof(buffer), &bytesRead) == FS_SUCCESS) {
        printf("%.*s", (int)bytesRead, buffer);
    }

    fs_file_close(pFile);
    fs_uninit(pFS);

    (void)argc;
    (void)argv;

    return 0;
}
