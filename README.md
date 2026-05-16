A collection of string sorting algorithm implementations
========================================================

This collection features several string sorting algorithm implementations, that
have been tuned to take better advantage of modern hardware. Classic
implementations tend to optimize instruction counts, but when sorting large
collections of strings, we also need to focus on memory issues. All algorithms
are implemented using C and C++.

Technical details:
  * All of the implementations sort the strings by raw byte values. This
    means that they are mainly intended for research use.
  * Includes several variants of known and efficient (string) sorting
    algorithms, such as MSD radix sort, burstsort and multi-key-quicksort.
  * Emphasis on reducing cache misses and memory stalls.
  * Includes the tools to create a HTML report, that can be
    used to compare the provided implementations. The report includes details
    such as TLB, L1 and L2 cache misses, run times and memory peak usage.
  * Supports Linux huge pages. For more information, see below.


License
-------

MIT.

Exception: The directory `external` contains files, that are included for
reference purposes, that may or may not be compatible with the MIT license.


Copyright
---------

Copyright © 2007-2012 by Tommi Rantala <tt.rantala@gmail.com>

The directory `external` contains files, that are included for reference
purposes, and are copyright by their respective authors.


Requirements
------------

  * C++17
  * CMake >= 3.16
  * OpenMP
  * Ninja (optional; the default Make generator also works)


Compilation
-----------

Default compilation with GCC:

    $ git clone https://github.com/rantala/string-sorting.git
    $ cd string-sorting
    $ cmake -B build -G Ninja && ninja -C build
    $ ./build/sortstring

Use a separate debug build for easier debugging:

    $ cmake -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug && ninja -C build-debug


GCC static analyzer
-------------------

The build can be configured to run the GCC static analyzer (`-fanalyzer`) on
the project's own sources. Third-party code under `external/` is excluded.

    $ cmake -B build-gcc-analyzer -G Ninja -DENABLE_GCC_ANALYZER=ON && ninja -C build-gcc-analyzer

The option requires GCC >= 13 for both C and C++; configuration fails fast
with any other compiler or older version. Analyzer diagnostics are emitted as
build warnings; the build still succeeds. Expect significantly longer compile
times when the option is enabled.


Clang static analyzer
---------------------

The build can also be configured to run the Clang static analyzer via
`clang-tidy`, scoped to the `clang-analyzer-*` check group. Third-party code
under `external/` is excluded.

    $ cmake -B build-clang-analyzer -G Ninja -DENABLE_CLANG_ANALYZER=ON && ninja -C build-clang-analyzer

The option requires `clang-tidy` on `PATH` (Ubuntu: `apt install clang-tidy`);
configuration fails fast if it is not found. Diagnostics appear inline like
compiler warnings and the build still succeeds. The configured C/C++ compiler
does not need to be Clang. `ENABLE_GCC_ANALYZER` and `ENABLE_CLANG_ANALYZER`
can be combined when building with GCC >= 13.


Target architecture
-------------------

The compiler `-march=` value can be overridden via the `MARCH` cache
variable (default `native`):

    $ cmake -B build-v3 -G Ninja -DMARCH=x86-64-v3 && ninja -C build-v3

Useful for targeting a portable ISA level or restricting the instructions
emitted by the compiler.


Huge pages
----------

The default page size on many computer architectures is 4 kilobytes. When
working with large data sets, this means that the input is spread to thousands
of memory pages. Unfortunately random access in thousands of pages can be slow
(see e.g. http://en.wikipedia.org/wiki/Translation_lookaside_buffer).

To alleviate this exact problem, many architectures have support for larger
page size. For example modern x86 has support for 2/4 megabyte "huge pages".
With such large pages, even large data sets fit into a much smaller amount of
memory pages.

In this program, support for huge pages is enabled using either --hugetlb-text
or --hugetlb-ptrs, or both. The former option places the input data (i.e. the
actual strings from the given file) into huge pages, and the latter option
places the string pointer array into huge pages. Using huge pages in Linux
requires CPU support, and properly adjusted kernel settings.

The external library libhugetlbfs (https://github.com/libhugetlbfs/libhugetlbfs)
can be used to replace all calls to malloc to use huge pages. If this library is
used, the aforementioned options are not needed.


HTML report creation
--------------------

Requirements:
  * OProfile for most measurements, probably also requires root privileges.
     - The default settings use Intel Core 2 specific events. When profiling on
       other platforms, you will most likely need to modify the scripts in the
       report/ directory.
  * /usr/bin/memusage for measuring the memory peak usage. This is a GNU libc
    utility.
