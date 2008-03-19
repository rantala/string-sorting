project(sortstring)
find_package(Boost REQUIRED)

set(INTERNAL_SRCS
	src/clock.cpp
	src/msd_a.cpp
	src/msd_ce.cpp
	src/msd_ci.cpp
	src/msd_dyn_block.cpp
	src/msd_dyn_vector.cpp
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

if(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Drestrict=__restrict__ -msse2")
endif(CMAKE_COMPILER_IS_GNUCXX)

set(CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE} -DBOOST_DISABLE_ASSERTS -march=core2")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DBOOST_DISABLE_ASSERTS -march=core2")
set(CMAKE_C_FLAGS_RELEASE        "${CMAKE_C_FLAGS_RELEASE} -march=core2")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -march=core2")
