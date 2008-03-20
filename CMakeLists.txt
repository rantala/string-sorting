project(sortstring)
find_package(Boost REQUIRED)

set(INTERNAL_SRCS
	src/clock.cpp
	src/msd_a.cpp
	src/msd_ce.cpp
	src/msd_ci.cpp
	src/msd_dyn_block.cpp
	src/msd_dyn_vector.cpp
	src/burstsort.cpp
	src/burstsort_mkq.cpp
	src/sortstring.cpp)

set(EXTERNAL_SRCS
	external/mbmradix.c
	external/quicksort.c
	external/mkqsort.c
	external/forward8.c
	external/cradix.c
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

add_definitions(-Drestrict=__restrict__)

if(USE_ICC)
	set(CMAKE_CXX_COMPILER "icpc")
	set(CMAKE_C_COMPILER "icc")
	set(CMAKE_CXX_FLAGS_RELEASE        "-g -DNDEBUG -DBOOST_DISABLE_ASSERTS -ipo -xT -O2")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -DNDEBUG -DBOOST_DISABLE_ASSERTS -ipo -xT -O2")
	set(CMAKE_C_FLAGS_RELEASE          "-g -DNDEBUG -ipo -xT -O2")
	set(CMAKE_C_FLAGS_RELWITHDEBINFO   "-g -DNDEBUG -ipo -xT -O2")
else(USE_ICC)
	set(CMAKE_CXX_FLAGS_RELEASE        "-g -DNDEBUG -DBOOST_DISABLE_ASSERTS -march=core2 ${CMAKE_CXX_FLAGS_RELEASE}")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -DNDEBUG -DBOOST_DISABLE_ASSERTS -march=core2 ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
	set(CMAKE_C_FLAGS_RELEASE          "-g -DNDEBUG -march=core2 ${CMAKE_C_FLAGS_RELEASE}")
	set(CMAKE_C_FLAGS_RELWITHDEBINFO   "-g -DNDEBUG -march=core2 ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
endif(USE_ICC)