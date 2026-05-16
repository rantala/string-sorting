cmake_minimum_required(VERSION 3.16)
project(sortstring C CXX)

include(CheckIncludeFile)

option(ENABLE_GCC_ANALYZER "Enable GCC -fanalyzer static analysis on internal sources" OFF)
if(ENABLE_GCC_ANALYZER)
	if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU" OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		message(FATAL_ERROR "ENABLE_GCC_ANALYZER requires GCC for both C and C++ "
			"(got C=${CMAKE_C_COMPILER_ID}, CXX=${CMAKE_CXX_COMPILER_ID}).")
	endif()
	if(CMAKE_C_COMPILER_VERSION VERSION_LESS 13 OR CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13)
		message(FATAL_ERROR "ENABLE_GCC_ANALYZER requires GCC >= 13 (got "
			"C=${CMAKE_C_COMPILER_VERSION}, CXX=${CMAKE_CXX_COMPILER_VERSION}).")
	endif()
endif()

option(ENABLE_CLANG_ANALYZER "Enable clang-tidy clang-analyzer-* checks on internal sources" OFF)
if(ENABLE_CLANG_ANALYZER)
	find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy)
	if(NOT CLANG_TIDY_EXECUTABLE)
		message(FATAL_ERROR "ENABLE_CLANG_ANALYZER requires clang-tidy on PATH.")
	endif()
	set(CLANG_TIDY_COMMAND
		${CLANG_TIDY_EXECUTABLE}
		-checks=-*,clang-analyzer-*
		-header-filter=^${CMAKE_SOURCE_DIR}/src/.*)
endif()

set(MARCH "native" CACHE STRING
	"Value passed to -march=. Override to target a different ISA level.")

check_include_file(sys/sdt.h HAVE_SYS_SDT_H)

find_package(OpenMP REQUIRED)

add_compile_options(-Wall -Wextra -march=${MARCH})

string(APPEND CMAKE_C_FLAGS_RELEASE          " -g")
string(APPEND CMAKE_CXX_FLAGS_RELEASE        " -g")

string(APPEND CMAKE_C_FLAGS_DEBUG   " -O1 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2")
string(APPEND CMAKE_CXX_FLAGS_DEBUG " -O1 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2")

set(INTERNAL_SRCS
	src/funnelsort_bfs_128way.cpp
	src/funnelsort_bfs_16way.cpp
	src/funnelsort_bfs_32way.cpp
	src/funnelsort_bfs_64way.cpp
	src/funnelsort_bfs_8way.cpp
	src/funnelsort_dfs_128way.cpp
	src/funnelsort_dfs_16way.cpp
	src/funnelsort_dfs_32way.cpp
	src/funnelsort_dfs_64way.cpp
	src/funnelsort_dfs_8way.cpp
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
	src/routines.cpp
	src/util/timing.cpp
	src/util/cpus_allowed.cpp
	src/util/vmainfo.cpp)

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

set_source_files_properties(external/adaptive.c  PROPERTIES COMPILE_FLAGS -Wno-sign-compare)
set_source_files_properties(external/quicksort.c PROPERTIES COMPILE_FLAGS -Wno-sign-compare)

add_library(sortstring_internal          OBJECT ${INTERNAL_SRCS})
add_library(sortstring_internal_unittest OBJECT ${INTERNAL_SRCS})
target_compile_definitions(sortstring_internal_unittest PRIVATE UNIT_TEST)

foreach(tgt sortstring_internal sortstring_internal_unittest)
	target_include_directories(${tgt} PUBLIC
		${CMAKE_SOURCE_DIR}/src
		${CMAKE_SOURCE_DIR}/src/util)
	target_compile_definitions(${tgt} PUBLIC restrict=__restrict__)
	if(HAVE_SYS_SDT_H)
		target_compile_definitions(${tgt} PUBLIC HAVE_SYS_SDT_H=1)
	endif()
	target_link_libraries(${tgt} PUBLIC OpenMP::OpenMP_C OpenMP::OpenMP_CXX)
	set_target_properties(${tgt} PROPERTIES
		C_STANDARD 99   C_STANDARD_REQUIRED YES
		CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
endforeach()

if(ENABLE_GCC_ANALYZER)
	target_compile_options(sortstring_internal          PRIVATE -fanalyzer)
	target_compile_options(sortstring_internal_unittest PRIVATE -fanalyzer)
endif()

if(ENABLE_CLANG_ANALYZER)
	set_target_properties(sortstring_internal sortstring_internal_unittest PROPERTIES
		C_CLANG_TIDY   "${CLANG_TIDY_COMMAND}"
		CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
endif()

add_executable(sortstring src/sortstring.cpp ${EXTERNAL_SRCS})
target_link_libraries(sortstring PRIVATE sortstring_internal rt)

add_executable(unit-test unit-test/main.cpp ${EXTERNAL_SRCS})
target_link_libraries(unit-test PRIVATE sortstring_internal_unittest rt)
target_compile_definitions(unit-test PRIVATE UNIT_TEST)

set_target_properties(sortstring unit-test PROPERTIES
	C_STANDARD 99   C_STANDARD_REQUIRED YES
	CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
