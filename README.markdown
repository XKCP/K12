# What is KangarooTwelve ?

[**KangarooTwelve**][k12] is a family of two (**KT128** and **KT256**) fast and secure extendable-output functions (XOF), the generalization of hash functions to arbitrary output lengths.
Derived from Keccak, they aim at higher speeds than FIPS 202's SHA-3 and SHAKE functions, while retaining their flexibility and basis of security.

On high-end platforms, they can exploit a high degree of parallelism, whether using multiple cores or the single-instruction multiple-data (SIMD) instruction set of modern processors.
On Intel's Haswell and Skylake architectures, KT128 tops at less than 1.5 cycles/byte for long messages on a single core, and at 0.51 cycles/byte on the SkylakeX and Cascade Lake architectures.
On the latest Apple A14 and M1 processors, KangarooTwelve can take advantage of the ARMv8-A's SHA-3 dedicated instructions and KT128 delivers 0.75 cycles/byte for long messages on a single core.
On low-end platforms, as well as for short messages, KT128 also benefits from about a factor two speed-up compared to the fastest FIPS 202 instance SHAKE128.

More details can be found in our [ACNS paper][eprint] (KT128 only) and in the [RFC draft](ietf).

# What can I find here?

This repository contains source code that implements the extendable output (or hash) function **KT128** and **KT256**.
Its purpose is to offer optimized implementations of the KangarooTwelve and nothing else.

The code comes from the [**eXtended Keccak Code Package**][xkcp] (or **XKCP**), after much trimming to keep only what is needed for KT.
It is still structured like the XKCP in two layers. The lower layer implements the permutation Keccak-_p_[1600, 12] and possibly parallel versions thereof, whereas the higher layer implements the sponge construction and the tree hash mode.
Also, some sources have been merged to reduce the file count.

* For the higher layer, we kept only the code needed for KT.
* For the lower layer, we removed all the functions that are not needed for KT. The lower layer therefore implements a subset of the SnP and PlSnP interfaces.

For Keccak or Xoodoo-based functions other than KT128 and KT256, it is recommended to use the XKCP itself instead and not to mix both this repository and the XKCP.


# Is there a tool to compute the hash of a file?

Not in this repository, but Jack O'Connor's [`kangarootwelve_xkcp.rs` repository](https://github.com/oconnor663/kangarootwelve_xkcp.rs) contains Rust bindings to this code and a `k12sum` utility.
Pre-built binaries can be found [there](https://github.com/oconnor663/kangarootwelve_xkcp.rs/releases).


# How can I build this code?

This repository uses the same build system as that of the XKCP.
To build, the following tools are needed:

* *GCC*
* *GNU make*
* *xsltproc*

The different targets are defined in [`Makefile.build`](Makefile.build). This file is expanded into a regular makefile using *xsltproc*. To use it, simply type, e.g.,

```
make generic64/K12Tests
```

to build K12Tests generically optimized for 64-bit platforms. The name before the slash indicates the platform, while the part after the slash is the executable to build. As another example, the static (resp. dynamic) library is built by typing `make generic64/libK12.a` (resp. `.so`) or similarly with `generic64` replaced with the appropriate platform name.  An alternate C compiler can be specified via the `CC` environment variable.

Instead of building an executable with *GCC*, one can choose to select the files needed and make a package. For this, simply append `.pack` to the target name, e.g.,

```
make generic64/K12Tests.pack
```

This creates a `.tar.gz` archive with all the necessary files to build the given target.

The list of targets can be found at the end of [`Makefile.build`](Makefile.build) or by running `make` without parameters.

## Microsoft Visual Studio support

KangarooTwelve can be compiled with Microsoft Visual Studio (MSVC). The XKCP build system offers support for the creation of project files. To get a project file for a given target, simply append `.vcxproj` to the target name, e.g.,

```
make generic64noAsm/K12Tests.vcxproj
```

The targets `generic32` and `generic64noAsm` can be used with MSVC, but not `generic64` as it contains assembly implementations in the GCC syntax, which at this point cannot be used with MSVC.
Please refer to the documention of [XKCP][xkcp] for more details on the limitations of the support of MSVC.

[k12]: https://keccak.team/kangarootwelve.html
[xkcp]: https://github.com/XKCP/XKCP
[eprint]: https://eprint.iacr.org/2016/770.pdf
[ietf]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-kangarootwelve/


# Acknowledgments

We wish to thank:

- Andy Polyakov for his expertise with the ARMv8-A+SHA3 code, and in particular for his core routine from [CRYPTOGAMS](https://github.com/dot-asm/cryptogams)
- Duc Tri Nguyen for his benchmark on the Apple M1
- Jack O'Connor for bug fixes and more importantly for his [Rust bindings](https://github.com/oconnor663/kangarootwelve_xkcp.rs)
- Kent Ross for his contributions to this code and its quality
- Hadi El Yakhni for adding KT256
