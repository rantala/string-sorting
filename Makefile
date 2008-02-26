PROGRAM=sortstring${PROG_SUFFIX}
COMMONSRC=other/adaptive.c\
	  other/benqsort.c\
	  other/burstsortA.c\
	  other/burstsortL.c\
	  other/forward16.c\
	  other/forward8.c\
	  other/mbmradix.c\
	  other/mkqsort.c\
	  other/msd.c\
	  other/multikey.c\
	  other/utils.c\
	  other/cradix.c
SRCS=sortstring.cpp \
     clock.cpp \
     msd0.cpp \
     msd1.cpp \
     msd2.cpp \
     msd3.cpp \
     msd_ci.cpp \
     msd_dyn_vector.cpp \
     msd_dyn_block.cpp \
     msd_a.cpp

     #$(COMMONSRC)

default: $(PROGRAM)

CXX=g++ -O0 -Drestrict=__restrict__ -g -I. -Wall -Wextra
CXX=g++ -O2 -Drestrict=__restrict__ -march=core2 -DNDEBUG -DBOOST_DISABLE_ASSERTS -I. -Wextra -Wall -g
CXX=icc -O2 -xP -restrict -ipo -DNDEBUG -DBOOST_DISABLE_ASSERTS -I. -Kc++ -g

${PROGRAM}: $(SRCS) Makefile
	$(CXX) $(CXXOPTS) $(SRCS) -o $(PROGRAM)
	@echo "Finished."

clean:
	-rm -f $(PROGRAM)
