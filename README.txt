SchnitzelCraft0 - what is it?
-----------------------------
Well, I have another server written in python called "SchnitzelCraft", but this
is the original server that was named that. It was written (horribly) in C,
and only works on Windows. Yet, I like it the most because of some of its
quirks that I never added to other MC Servers I wrote. For instance, Mobs,
Physics, two-layer maps and the bedrock water chute.

So, I've started hacking on and slowly refactoring the original source code.
Eventually, the bugs in map handling, blocking sockets, and windows dependency
will all be dealt with, and it will be something you could use seriously.

Building
--------
You need MinGW GCC in your path. Then build.bat *should* work.

Running
-------
Until I get round to fixing it, all configuration options are defined as
constants in the source code.
Therefore, to run it, all you need to do is run schnitzelcraft0.exe