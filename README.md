This is a cross-platform library that provides a unified API for accessing the native file system
and archives such as ZIP. It's written in C (compilable as C++), has no external dependencies, and
is your choice of either public domain or [MIT No Attribution](https://github.com/aws/mit-0).


About
=====
See the documentation at the top of [fs.h](fs.h) for details on how to use the library.

This library supports the ability to open files from the both the native file system and virtual
file systems through a unified API. File systems are implemented as backends, examples of which can
be found in the [extras/backends](extras/backends) folder. Custom backends can be implemented and
plugged in without needing to modify the library.

One common use of a backend, and the motivation for the creation of this library in the first
place, is to support archive formats like ZIP. You can plug in archive backends and associate them
with a file extension to enable seamless and transparent handling of archives. For example, you can
associate the ZIP backend with the "zip" extension which will then let you load a file like this:

```c
fs_file_open(pFS, "myapp/archive.zip/file.txt", FS_READ, &pFile);
```

The example above explicitly lists the "archive.zip" archive in the path, but you need not specify
that if you don't want to. The following will also work:

```c
fs_file_open(pFS, "myapp/file.txt", FS_READ, &pFile);
```

Here the archive is handled totally transparently. There are many options for opening a file, some
of which are more efficient than others. See the documentation in [fs.h](fs.h) for details.

When opening a file, the library searches through a list of mount points that you set up
beforehand. Below is an example for setting up a simple mount point that will be used when opening
a file for reading:

```c
fs_mount(pFS, "/home/user/.local/share/myapp", NULL, FS_READ);
```

With this setup, opening a file in read-only mode will open the file relative to the path specified
in `fs_mount()`. You can also specify a virtual path for a mount point:

```c
fs_mount(pFS, "/home/user/.local/share/myapp", "data", FS_READ);
```

With the mount above you would need to prefix your path with "data" when opening a file:

```c
fs_file_open(pFS, "data/file.txt", FS_READ, &pFile);
```

Since the path starts with "data", the paths mounted to the virtual path "data" will be searched
when opening the file.

You can also mount multiple paths to the same virtual path, in which case the later will take
precedence. Below is an example you might see in a game:

```c
fs_mount(pFS, "/usr/share/mygame/gamedata/base",              "gamedata", FS_READ); // Base game. Lowest priority.
fs_mount(pFS, "/home/user/.local/share/mygame/gamedata/mod1", "gamedata", FS_READ); // Mod #1. Middle priority.
fs_mount(pFS, "/home/user/.local/share/mygame/gamedata/mod2", "gamedata", FS_READ); // Mod #2. Highest priority.
```

You could then load an asset like this:

```c
fs_file_open(pFS, "gamedata/texture.png", FS_READ, &pFile);
```

Here there "mod2" directory would be searched first because it has the highest priority. If the
file cannot be opened from there it'll fall back to "mod1", and then as a last resort it'll fall
back to the base game.

Internally there are a separate set of mounts for reading and writing. To set up a mount point for
opening files in write mode, you need to specify the `FS_WRITE` option:

```c
fs_mount(pFS, "/home/user/.config/mygame", "config", FS_WRITE);
```

To open a file for writing, you need only prefix the path with the mount's virtual path, exactly
like read mode:

```c
fs_file_open(pFS, "config/game.cfg", FS_WRITE, &pFile);
```

You can set up read and write mount points to the same virtual path:

```c
fs_mount(pFS, "/usr/share/mygame/config",              "config", FS_READ);
fs_mount(pFS, "/home/user/.local/share/mygame/config", "config", FS_READ | FS_WRITE);
```

When opening a file for reading, it'll first try searching the second mount point, and if it's not
found will fall back to the first. When opening in write mode, it will only ever use the second
mount point as the output directory because that's the only one set up with `FS_WRITE`. With this
setup, the first mount point is essentially protected from modification.

There's more you can do with mounting, such as mounting an archive or `fs` object to a virtual
path, helpers for mounting system directories, and preventing navigation outside the mount point.
See the documentation at the top of [fs.h](fs.h) for more details.

In addition to the aforementioned functionality, the library includes all of the standard
functionality you would expect for file IO, such as file enumeration, stat-ing (referred to as
"info" in this library), creating directories and deleting and renaming files.


Usage
=====
See [fs.h](fs.h) for documentation. Examples can be found in the [examples](examples) folder. Backends can
be found in the [extras/backends](extras/backends) folder.


Building
========
To build the library, just add the necessary source files to your source tree. The main library is
contained within fs.c. Archive backends are each contained in their own separate file. Stock
archive backends can be found in the "extras" folder. These will have a .h file which you should
include after fs.h:

```c
#include "fs.h"
#include "extras/backends/zip/fs_zip.h"
#include "extras/backends/pak/fs_pak.h"
```

You need only include backends that you care about. See examples/archives.c for an example on how
to use archives.

You can also use CMake, but support for that is very basic.


License
=======
Your choice of either public domain or [MIT No Attribution](https://github.com/aws/mit-0).
