# Imports

There are two different types of import in the language right now: quoted and named.

```tea
import math
```

The named import

This one loads a module named math, that is 'baked' inside the language.

```tea
import "file.tea" as file
```

While this other one imports a file named `file.tea` as `file`, from a relative path.

It could be redundant to have two different syntaxes for imports, but we need to distinguish between a relative and a logical path in some way.

now, thinking about all the possible files that can be imported now or soon
- `*.tea` normal Teascript scripting files;
- `*.tbc` compiled Teascript bytecode;
- `*.dll/*.so/*.lib` compiled C libraries;
- natives libraries directly inside the VM;
- folders it searches for a init.tea or a init.tbc (even a init.dll/.so ?);

std modules like `time` `os` or `date` use import name instead of import "name", same for installed packages. this because they can be accessed from everywhere.
