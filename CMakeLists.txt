cmake_minimum_required(VERSION 2.4)

project(sortstring)
find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
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
	src/util/timing.c)

set(EXTERNAL_SRCS
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
	external/forward16.c)

add_executable(sortstring
	${INTERNAL_SRCS}
	${EXTERNAL_SRCS})

option(USE_ICC "Use the Intel C/C++ compiler" OFF)
option(USE_SUNCC "Use the Sun Studio C/C++ compiler" OFF)

if(USE_ICC)
	add_definitions(-restrict)
	set(CMAKE_CXX_COMPILER "icpc")
	set(CMAKE_C_COMPILER "icc")
	set(CMAKE_CXX_FLAGS_RELEASE        "-g -DNDEBUG -DBOOST_DISABLE_ASSERTS -ipo -xT -O2")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -DNDEBUG -DBOOST_DISABLE_ASSERTS -ipo -xT -O2")
	set(CMAKE_C_FLAGS_RELEASE          "-g -DNDEBUG -ipo -xT -O2")
	set(CMAKE_C_FLAGS_RELWITHDEBINFO   "-g -DNDEBUG -ipo -xT -O2")
elseif(USE_SUNCC)
	# Beware: SunCC is unable to compile this software.
	add_definitions(-Drestrict="")
	add_definitions(-D__PRETTY_FUNCTION__="' '")
	add_definitions(-D__builtin_expect="")
	set(CMAKE_CXX_COMPILER "CC")
	set(CMAKE_C_COMPILER "cc")
	set(CMAKE_CXX_FLAGS_RELEASE        "-g -DNDEBUG -DBOOST_DISABLE_ASSERTS ${CMAKE_CXX_FLAGS_RELEASE}")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -DNDEBUG -DBOOST_DISABLE_ASSERTS ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
	set(CMAKE_C_FLAGS_RELEASE          "-g -DNDEBUG ${CMAKE_C_FLAGS_RELEASE}")
	set(CMAKE_C_FLAGS_RELWITHDEBINFO   "-g -DNDEBUG ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
else(USE_ICC)
	add_definitions(-msse2)
	add_definitions(-Drestrict=__restrict__)
	set(CMAKE_CXX_FLAGS_RELEASE        "-fopenmp -g -DNDEBUG -DBOOST_DISABLE_ASSERTS -march=native ${CMAKE_CXX_FLAGS_RELEASE}")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-fopenmp -g -DNDEBUG -DBOOST_DISABLE_ASSERTS -march=native ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
	set(CMAKE_C_FLAGS_RELEASE          "-fopenmp -g -DNDEBUG -march=native ${CMAKE_C_FLAGS_RELEASE}")
	set(CMAKE_C_FLAGS_RELWITHDEBINFO   "-fopenmp -g -DNDEBUG -march=native ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
	set(CMAKE_CXX_FLAGS                "-Wall -Wextra ${CMAKE_CXX_FLAGS}")
	set(CMAKE_C_FLAGS                  "-Wall -Wextra -std=c99 ${CMAKE_C_FLAGS}")
endif(USE_ICC)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2")
set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}   -O0 -g -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2")
