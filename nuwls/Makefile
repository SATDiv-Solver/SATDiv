COMPILER = g++
CFLAGS = -O3 -mcmodel=large -std=c++17
PRE_CFLAGS = ${CFLAGS} -c

OBJS = build.o heuristic.o pms.o deci.o

.PHONY: all clean
all: ${OBJS}

build.o: build.cc basis_pms.h
	${COMPILER} ${PRE_CFLAGS} build.cc
heuristic.o: heuristic.cc basis_pms.h
	${COMPILER} ${PRE_CFLAGS} heuristic.cc
pms.o: pms.cc basis_pms.h
	${COMPILER} ${PRE_CFLAGS} pms.cc
deci.o: deci.cc basis_pms.h deci.h
	${COMPILER} ${PRE_CFLAGS} deci.cc

clean:
	@rm -f ${OBJS}
