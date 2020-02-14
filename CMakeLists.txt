cmake_minimum_required(VERSION 3.1)
set(CMAKE_CXX_STANDARD 11)

project(sortstring)
include_directories(src src/util)

link_libraries(rt)

set(INTERNAL_SRCS
	src/msd_a.cpp
	src/msd_a2.cpp
	src/msd_lsd.cpp
	src/msd_ce.cpp
	src/msd_ci.cpp
	src/msd_dyn_block.cpp
	src/msd_dyn_vector.cpp
	src/burstsort.cpp
	src/burstsort2.cpp
	src/burstsort_mkq.cpp
	src/multikey_simd.cpp
	src/multikey_dynamic.cpp
	src/multikey_block.cpp
	src/multikey_multipivot.cpp
	src/multikey_cache.cpp
	src/mergesort.cpp
	src/mergesort_unstable.cpp
	src/mergesort_losertree.cpp
	src/mergesort_lcp.cpp
	src/funnelsort.cpp
	src/routines.c
	src/sortstring.c
	src/util/timing.c
	src/util/cpus_allowed.c
	src/util/vmainfo.c)

set(EXTERNAL_SRCS
	external/lcp-quicksort.cpp
	external/mbmradix.c
	external/quicksort.c
	external/mkqsort.c
	external/forward8.c
	external/cradix.c
	external/cradix-improved.c
	external/msd.c
	external/multikey.c
	external/burstsortL.c
	external/utils.c
	external/adaptive.c
	external/burstsortA.c
	external/forward16.c
	external/parallel_string_radix_sort.cpp)

set_source_files_properties(external/adaptive.c PROPERTIES COMPILE_FLAGS -Wno-sign-compare)
set_source_files_properties(external/quicksort.c PROPERTIES COMPILE_FLAGS -Wno-sign-compare)

add_executable(sortstring
	${INTERNAL_SRCS}
	${EXTERNAL_SRCS})

add_definitions(-msse2)
add_definitions(-Drestrict=__restrict__)
set(CMAKE_CXX_FLAGS_RELEASE        "-fopenmp -g -DNDEBUG -march=native ${CMAKE_CXX_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-fopenmp -g -DNDEBUG -march=native ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
set(CMAKE_C_FLAGS_RELEASE          "-fopenmp -g -DNDEBUG -march=native ${CMAKE_C_FLAGS_RELEASE}")
set(CMAKE_C_FLAGS_RELWITHDEBINFO   "-fopenmp -g -DNDEBUG -march=native ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
set(CMAKE_CXX_FLAGS                "-Wall -Wextra ${CMAKE_CXX_FLAGS}")
set(CMAKE_C_FLAGS                  "-Wall -Wextra -std=c99 ${CMAKE_C_FLAGS}")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O1 -g -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2")
set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}   -O1 -g -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2")
