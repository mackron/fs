/*
This backend can be used to create a filesystem where the root directory of the new fs object is
a subdirectory of another fs object. Trying to access anything outside of this root directory
will result in an error.

Everything is done in terms of the owner FS object, meaning the owner must be kept alive for the
life of the sub object. It also means it will inherit the owner's registered archive types.

The way this works is very simple. Whenever you try to open a file, the path is checked to see if
it's attempting to access anything outside of the root directory. If it is, an error is returned.
Otherwise, the root directory is prepended to the path and the operation is passed on to the owner
FS object.

To use this backend, you need to create a fs_sub_config object and fill in the pOwnerFS and
pRootDir fields. Then pass this object into `fs_init()`:

    fs_sub_config subConfig;
    subConfig.pOwnerFS = pOwnerFS;
    subConfig.pRootDir = "subdir";

    fs_config fsConfig = fs_config_init(FS_SUB, &subConfig, NULL);

    fs* pSubFS;
    fs_init(&fsConfig, &pSubFS);

You must ensure you destroy your sub object before destroying the owner FS object.
*/
#ifndef fs_sub_h
#define fs_sub_h

#if defined(__cplusplus)
extern "C" {
#endif

/* BEG fs_sub.h */
typedef struct fs_sub_config
{
    fs* pOwnerFS;
    const char* pRootDir;
} fs_sub_config;

extern const fs_backend* FS_SUB;
/* END fs_sub.h */

#if defined(__cplusplus)
}
#endif
#endif  /* fs_sub_h */
