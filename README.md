This is a cross-platform library which abstracts access to the regular file system and archives
such as ZIP files. It's written in C, has no external dependencies, and is your choice of either
public domain or [MIT No Attribution](https://github.com/aws/mit-0).


Building
========
To build the library, just add the necessary source files to your source tree. The main library is
contained within a single .c file. Archive backends are each contained in their own separate file.
Stock archive backends can be found in the "extras" folder.

You can also use CMake, but support for that is very basic.


Usage
=====
Below is an overview of the core functionality of the library. See the .h file for more complete
documentation.

Basic Usage
-----------
The main object in the library is the `fs` object. Below is the most basic way to initialize a `fs`
object:

```c
fs_result result;
fs* pFS;

result = fs_init(NULL, &pFS);
if (result != FS_SUCCESS) {
	// Failed to initialize.
}
```

The above code will initialize a `fs` object representing the system's regular file system. It uses
stdio under the hood. Once this is set up you can load files:

```c
fs_file* pFile;

result = fs_file_open(pFS, "file.txt", FS_READ, &pFile);
if (result != FS_SUCCESS) {
	// Failed to open file.
}
```

Reading content from the file is very standard:

```c
size_t bytesRead;

result = fs_file_read(pFS, pBuffer, bytesToRead, &bytesRead);
if (result != FS_SUCCESS) {
	// Failed to read file. You can use FS_AT_END to check if reading failed to being at EOF.
}
```

In the code above, the number of bytes actually read is output to a variable. You can use this to
determine if you've reached the end of the file.

To do more advanced stuff, such as opening from archives, you'll need to configure the `fs` object
with a config, which you pass into `fs_init()`:

```c
#include "extras/fs_zip.h" // <-- This is where FS_ZIP is declared.

...

fs_archive_type pArchiveTypes[] =
{
    {FS_ZIP, "zip"},
    {FS_ZIP, "pac"}
};

fs_config fsConfig = fs_config_init(FS_STDIO, NULL, NULL);
fsConfig.pArchiveTypes    = pArchiveTypes;
fsConfig.archiveTypeCount = sizeof(pArchiveTypes) / sizeof(pArchiveTypes[0]);

fs_init(&fsConfig, &pFS);
```

In the code above we are registering support for ZIP archives (`FS_ZIP`). Whenever a file with a
"zip" or "pac" extension is found, the library will be able to access the archive. The library will
determine whether or not a file is an archive based on it's extension. If the extension does not
match, it'll assume it's not an archive and will skip it. Below is an example of one way you can
read from an archive:

```c
result = fs_file_open(pFS, "archive.zip/file-inside-archive.txt", FS_READ, &pFile);
if (result != FS_SUCCESS) {
	// Failed to open file.
}
```

In the example above, we've explicitly specified the name of the archive in the file path. The
library also supports the ability to handle archives transparently, meaning you don't need to
explicitly specify the archive. The code below will also work:

```c
fs_file_open(pFS, "file-inside-archive.txt", FS_READ, &pFile);
```

Transparently handling archives like this has overhead because the library needs to scan the file
system and check every archive it finds. To avoid this, you can explicitly disable this feature:

```c
fs_file_open(pFS, "archive.zip/file-inside-archive.txt", FS_READ | FS_VERBOSE, &pFile);
```

In the code above, the `FS_VERBOSE` flag will require you to pass in a verbose file path, meaning
you need to explicitly specify the archive in the path. You can take this one step further by
disabling access to archives in this manner altogether via `FS_OPAQUE`:

```c
result = fs_file_open(pFS, "archive.zip/file-inside-archive.txt", FS_READ | FS_OPAQUE, &pFile);
if (result != FS_SUCCESS) {
	// This example will always fail.
}
```

In the example above, `FS_OPAQUE` is telling the library to treat archives as if they're totally
opaque and that the files within cannot be accessed.

Up to this point the handling of archives has been done automatically via `fs_file_open()`, however
the library allows you to manage archives manually. To do this you just initialize a `fs` object to
represent the archive:

```c
// Open the archive file itself first.
fs_file* pArchiveFile;

result = fs_file_open(pFS, "archive.zip", FS_READ, &pArchiveFile);
if (result != FS_SUCCESS) {
	// Failed to open archive file.
}


// Once we have the archive file we can create the `fs` object representing the archive.
fs* pArchive;
fs_config archiveConfig;

archiveConfig = fs_config_init(FS_ZIP, NULL, fs_file_get_stream(pArchiveFile));

result = fs_init(&archiveConfig, &pArchive);
if (result != FS_SUCCESS) {
	// Failed to initialize archive.
}

...

// During teardown, make sure the archive `fs` object is uninitialized before the stream.
fs_uninit(pArchive);
fs_file_close(pArchiveFile);
```

To initialize an `fs` object for an archive you need a stream to provide the raw archive data to
the backend. Conveniently, the `fs_file` object itself is a stream. In the example above we're just
opening a file from a different `fs` object (usually one representing the default file system) to
gain access to a stream. The stream does not need to be a `fs_file`. You can implement your own
`fs_stream` object, and a `fs_memory_stream` is included as stock with the library for when you
want to store the contents of an archive in-memory. Once you have the `fs` object for the archive
you can use it just like any other:

```c
result = fs_file_open(pArchive, "file-inside-archive.txt", FS_READ, &pFile);
if (result != FS_SUCCESS) {
	// Failed to open file.
}
```

In addition to the above, you can use `fs_open_archive()` to open an archive from a file:

```c
fs* pArchive;

result = fs_open_archive(pFS, "archive.zip", FS_READ, &pArchive);
```

When opening an archive like this, it will inherit the archive types from the parent `fs` object
and will therefore support archives within archives. Use caution when doing this because if both
archives are compressed you will get a big performance hit. Only the inner-most archive should be
compressed.


Mounting
--------
There is no notion of a "current directory" in this library. By default, relative paths will be
relative to whatever the backend deems appropriate. In practice, this means the "current" directory
for the default system backend, and the root directory for archives. There is still control over
how to load files from a relative path, however: mounting.

You can mount a physical directory to virtual path, similar in concept to Unix operating systems.
The difference, however, is that you can mount multiple directories to the same mount point. There
are separate mount points for reading and writing. Below is an example for mounting for reading:

```c
fs_mount(pFS, "/some/actual/path", NULL, FS_MOUNT_PRIORITY_HIGHEST);
```

In the example above, `NULL` is equivalent to an empty path. If, for example, you have a file with
the path "/some/actual/path/file.txt", you can open it like the following:

```c
fs_file_open(pFS, "file.txt", FS_READ, &pFile);
```

You don't need to specify the "/some/actual/path" part because it's handled by the mount. If you
specify a virtual path, you can do something like the following:

```c
fs_mount(pFS, "/some/actual/path", "assets", FS_MOUNT_PRIORITY_HIGHEST);
```

In this case, loading files that are physically located in "/some/actual/path" would need to be
prexied with "assets":

```c
fs_file_open(pFS, "assets/file.txt", FS_READ, &pFile);
```

Archives can also be mounted:

```c
fs_mount(pFS, "/game/data/base/assets.zip", "assets", FS_MOUNT_PRIORITY_HIGHEST);
```

You can mount multiple paths to the same mount point:

```c
fs_mount(pFS, "/game/data/base.zip", "assets", FS_MOUNT_PRIORITY_HIGHEST);
fs_mount(pFS, "/game/data/mod1.zip", "assets", FS_MOUNT_PRIORITY_HIGHEST);
fs_mount(pFS, "/game/data/mod2.zip", "assets", FS_MOUNT_PRIORITY_HIGHEST);
```

In the example above, the "base.zip" archive is mounted first. Then "mod1.zip" is mounted, which
takes higher priority over "base.zip". Then "mod2.zip" is mounted which takes higher priority
again. With this set up, any file that is loaded from the "assets" mount point will first be loaded
from "mod2.zip", and if it doesn't exist there, "mod1.zip", and if not there, finally "base.zip".
You could use this set up to support simple modding prioritization in a game, for example.

When opening a file, mounts always take priority over the backend's default search path. If the
file cannot be opened from any mounts, it will attempt to open the file from the backend's default
search path. When opening in transparent mode with `FS_TRANSPARENT` (default), it will first try
opening the file as if it were not in an archive. If that fails, it will look inside archives.

You can also mount directories for writing:

```c
fs_mount_write(pFS, "/home/user/.config/mygame", "config", FS_MOUNT_PRIORITY_HIGHEST);
```

You can then open a file for writing like so:

```c
fs_file_open(pFS, "config/game.cfg", FS_WRITE, &pFile);
```

When opening a file in write mode, the prefix is what determines which write mount point to use.
You can therefore have multiple write mounts:

```c
fs_mount_write(pFS, "/home/user/.config/mygame",            "config", FS_MOUNT_PRIORITY_HIGHEST);
fs_mount_write(pFS, "/home/user/.local/share/mygame/saves", "saves",  FS_MOUNT_PRIORITY_HIGHEST);
```

Now you can write out different types of files, with the prefix being used to determine where it'll
be saved:

```c
fs_file_open(pFS, "config/game.cfg", FS_WRITE, &pFile);	// Prefixed with "config", so will use the "config" mount point.
fs_file_open(pFs, "saves/save0.sav", FS_WRITE, &pFile);	// Prefixed with "saves", so will use the "saves" mount point.
```

Note that writing directly into an archive is not supported by this API. To write into an archive,
the backend itself must support writing, and you will need to manually initialize a `fs` object for
the archive an write into it directly.


Enumeration
-----------
You can enumerate over the contents of a directory like the following:

```c
for (fs_iterator* pIterator = fs_first(pFS, "directory/to/enumerate", FS_NULL_TERMINATED, 0); pIterator != NULL; pIterator = fs_next(pIterator)) {
	printf("Name: %s\n",   pIterator->pName);
	printf("Size: %llu\n", pIterator->info.size);
}
```

If you want to terminate iteration early, use `fs_free_iterator()` to free the iterator object.
`fs_next()` will free the iterator for you when it reaches the end.

Like when opening a file, you can specify `FS_OPAQUE`, `FS_VERBOSE` or `FS_TRANSPARENT` (default)
in `fs_first()` to control which files are enumerated. Enumerated files will be consistent with
what would be opened when using the same option with `fs_file_open()`.

Internally, `fs_first()` will gather all of the enumerated files. This means you should expect
`fs_first()` to be slow compared to `fs_next()`.

Enumerated entries will be sorted by name in terms of `strcmp()`.

Enumeration is not recursive. If you want to enumerate recursively you can inspect the `directory`
member of the `info` member in `fs_iterator`.

License
=======
Your choice of either public domain or [MIT No Attribution](https://github.com/aws/mit-0).
