# What is the purpose of this repository?

This repository contains source code that implements the extandable output (or hash) function [**KangarooTwelve**][k12] (or **K12**).
Its purpose is to offer optimized implementations of K12 and nothing else.

The code comes from the [**eXtended Keccak Code Package**][xkcp] (or **XKCP**), after much trimming to keep only what is needed for K12.
It is still structured like the XKCP in two layers. The lower layer implements the permutation Keccak-_p_[1600, 12] and possibly parallel versions thereof, whereas the higher layer implements the sponge construction and the K12 tree hash mode.
Also, some sources have been merged to reduce the file count.

* For the higher layer, we kept only the code needed for K12.
* For the lower layer, we removed all the functions that are not needed for K12. The lower layer therefore implements a subset of the SnP and PlSnP interfaces.

For Keccak or Xoodoo-based functions other than K12 only, it is recommended to use the XKCP itself instead and not to mix both this repository and the XKCP.


# How can I build this K12 code?

This repository uses the same build system as that of the XKCP.
To build, the following tools are needed:

* *GCC*
* *GNU make*
* *xsltproc*

The different targets are defined in [`Makefile.build`](Makefile.build). This file is expanded into a regular makefile using *xsltproc*. To use it, simply type, e.g.,

> `make generic64/K12Tests`

to build K12Tests generically optimized for 64-bit platforms. The name before the slash indicates the platform, while the part after the slash is the executable to build. As another example, the static (resp. dynamic) library is built by typing `make generic64/libK12.a` (resp. `.so`) or similarly with `generic64` replaced with the appropriate platform name.  An alternate C compiler can be specified via the `CC` environment variable.

Instead of building an executable with *GCC*, one can choose to select the files needed and make a package. For this, simply append `.pack` to the target name, e.g.,

> `make generic64/K12Tests.pack`

This creates a `.tar.gz` archive with all the necessary files to build the given target.

The list of targets can be found at the end of [`Makefile.build`](Makefile.build) or by running `make` without parameters.

For Microsoft Visual Studio support and other details, please refer to the [XKCP][xkcp].

[k12]: https://keccak.team/kangarootwelve.html
[xkcp]: https://github.com/XKCP/XKCP
