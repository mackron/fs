This is a cross-platform library which abstracts access to the regular file system and archives
such as ZIP files. It's written in C, has no external dependencies, and is your choice of either
public domain or [MIT No Attribution](https://github.com/aws/mit-0).


About
=====
This library supports the ability to open files from the native file system, in addition to
archives such as ZIP files, through a consistent API. The idea is that a `fs` object can represent
any kind of file system, including, but not restricted to, the native file system, and archives.
You can implement and plug in custom file system backends, examples of which can be found in the
[extras/backends](extras/backends) folder.

There are multiple ways of working with an archive, the most common way of which is to associate a
backend with a file extension. When this association is established, which you do during the
initialization of your `fs` object, the `fs` object will be able to seamlessly read from those
archives. By default, archives will be scanned automatically and handled transparently, but this
can be inefficient so there are options when opening a file to require that archives be explicitly
listed in the path such as "data/archive.zip/file.txt", or to even disable loading from archives
entirely.

There is no notion of a "current" or "working" directory. Instead, when you want to open a file
using a relative path, it searches through a list of base directories that you specify. The first
search path in this list that contains a file of the specified relative path will be where the file
is loaded from. Adding a search/base path is referred to as mounting. You can mount both
directories and archives. When mounting an archive it will be handled totally transparently with no
special handling required on your part.

There is a lot of flexibility to the mounting system. An obvious use case is for a game where you
want to support mods, with each mod having a different priority. You could, as an example, set up
your mounts like this:

```c
fs_mount(pFS, "/game/install/path/gamedata/base", "gamedata", 0); // Base game. Lowest priority.
fs_mount(pFS, "/game/install/path/gamedata/mod1", "gamedata", 0); // Mod #1. Middle priority.
fs_mount(pFS, "/game/install/path/gamedata/mod2", "gamedata", 0); // Mod #2. Highest priority.
```

In this example you would then load a game asset like this:

```c
fs_file* pFile;
fs_file_open(pFS, "gamedata/texture.png", FS_READ, &pFile); // Start the path with "gamedata" to search in the "gamedata" mounts.
```

The "mod2" search directory will have the highest priority. If "texture.png" is not found in that
mod's data directory, "mod1" will have it's directory searched. If it can't be found there it will
fall back to the base game. The example above has the mods data directory in the same folder as the
game's installation directory, but you can refer to any actual path you like. You could even have
it so your mods are installed in a separate folder if you'd like mods to be decoupled from your
game's installation:

```c
fs_mount(pFS, "/game/install/path/gamedata/base", "gamedata", 0);
fs_mount(pFS, "/mods/install/path/mod1",          "gamedata", 0);
```

The examples above use a mount point called "gamedata", but you can use an empty directory as well:

```c
fs_mount(pFS, "/game/install/path/gamedata/base", "", 0);
```

You can also mount an archive straight from it's path, so long as the `fs` object has been
configured with support for the relevant backend:

```c
fs_mount(pFS, "/game/install/path/gamedata/base.zip", "gamedata", 0);
```

You can also mount a directory within an archive:

```c
fs_mount(pFS, "/game/install/path/gamedata.zip/base", "gamedata", 0);
```

You can also mount an `fs` directly which might be useful if, for example, you have an in-memory
archive that you want to link up to your main `fs` object:

```c
fs_mount_fs(pFS, pInMemoryArchiveFS, "gamedata", 0);  // The second paramter is just another `fs` object.
```

Other libraries such as PhysicsFS disable the ability to use "." and ".." in file paths, the idea
being that it prevents you from navigating outside the mount point. This library on the other hand
explicitly supports the ability to navigate above and around the mount point, but you have the
ability to disable it.

The first way is to use a leading "/" for the mount point:

```c
fs_mount(pFS, "/game/install/path/gamedata/base", "/gamedata", 0);
```

With this example we are mounting to "/gamedata" instead of "gamedata". The leading slash tells the
library to consider that the root and to not allow navigation outside of that. The other way to do
it is on a per-file basis via `fs_file_open()`:

```c
fs_file_open(pFS, "gamedata/../texture.png", FS_READ | FS_NO_ABOVE_ROOT_NAVIGATION, &pFile);
```

Here the `FS_NO_ABOVE_ROOT_NAVIGATION` flag will disallow the ability to navigate above the level
of the mount point, exactly as if the mount point was "/gamedata" instead of "gamedata".

If you want to be exactly like PhysicsFS and disable the "." and ".." path segments entirely, you
can do so with `FS_NO_SPECIAL_DIRS`:

```c
fs_file_open(pFS, pSomeInsecureFilePathFromUser, FS_READ | FS_NO_SPECIAL_DIRS, &pFile);
```

In addition to mounting search paths for reading, you can do the same for writing. Writing mounts
are separate from the mounts used when opening a file for reading:

```c
fs_mount_write(pFS, "/home/user/.config/mygame",            "config", 0);
fs_mount_write(pFS, "/home/user/.local/share/mygame/saves", "saves",  0);
```

Here we've mounted two write directories. When you open a file for writing, it looks at the start
of the file path and checks if it matches and of the mount points. So with the mounts above, we can
save a config file like so:

```c
fs_file_open(pFS, "config/game.cfg", FS_WRITE, &pFile); // Will save to the "config" mount point because the path starts with "config".
```

And then when we want to write a save game, you can use the "saves" mount point:

```c
fs_file_open(pFS, "saves/save1.cfg", FS_WRITE, &pFile); // Will save to the "saves" mount point because the path starts with "saves".
```

Writing into an archive is not supported through the main API like this. Therefore, attempting
to mount an archive for writing will fail.

This kind of mounting system allows you to isolate all of your platform-specific directory
structures to the mounting part of your code, and then everything else can be cross-platform code.

In addition to the aforementioned functionality, the library includes all of the standard
functionality you would expect for file IO, such as file enumeration, stat-ing (referred to as
"info" in this library), creating directories and deleting and renaming files.


Usage
=====
See fs.h for documentation. Examples can be found in the "examples" folder.


Building
========
To build the library, just add the necessary source files to your source tree. The main library is
contained within fs.c. Archive backends are each contained in their own separate file. Stock
archive backends can be found in the "extras" folder. These will have a .h file which you should
include after fs.h:

```c
#include "fs.h"
#include "extras/backends/zip/fs_zip.h"
```

See [archives](examples/archives.c) for an example on how to use archives.

You can also use CMake, but support for that is very basic.


License
=======
Your choice of either public domain or [MIT No Attribution](https://github.com/aws/mit-0).
