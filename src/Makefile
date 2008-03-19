PROGRAM=sortstring${PROG_SUFFIX}
EXTERNAL_SRC=../external/adaptive.c\
	     ../external/quicksort.c\
	     ../external/burstsortA.c\
	     ../external/burstsortL.c\
	     ../external/forward16.c\
	     ../external/forward8.c\
	     ../external/mbmradix.c\
	     ../external/mkqsort.c\
	     ../external/msd.c\
	     ../external/multikey.c\
	     ../external/utils.c\
	     ../external/cradix.c
SRCS=sortstring.cpp \
     clock.cpp \
     msd_ce.cpp \
     msd_ci.cpp \
     msd_dyn_vector.cpp \
     msd_dyn_block.cpp \
     msd_a.cpp \
     $(EXTERNAL_SRC)

default: $(PROGRAM)

CXX=g++ -O0 -Drestrict=__restrict__ -g -I. -Wall -Wextra
CXX=g++ -O2 -Drestrict=__restrict__ -march=core2 -DNDEBUG -DBOOST_DISABLE_ASSERTS -I. -Wextra -Wall -g
CXX=icc -O2 -xP -restrict -ipo -DNDEBUG -DBOOST_DISABLE_ASSERTS -I. -Kc++ -g -wd69

${PROGRAM}: $(SRCS) Makefile
	$(CXX) $(CXXOPTS) $(SRCS) -o $(PROGRAM)
	@echo "Finished."

clean:
	-rm -f $(PROGRAM)
