COMPILER = g++
CFLAGS = -O3 -mcmodel=large -std=c++17 -Wall -Wno-sign-compare -Wno-parentheses -Idbg-macro -Iclipp/include -IMaple
PRE_CFLAGS = ${CFLAGS} -c

TARGET = SATDiv

SRC_DIR = src
BIN_DIR = bin
LIB_DIR = lib

DSAT = DiversitySAT
DSAT_TARGET = ${SRC_DIR}/${DSAT}.o
DSAT_CPP_FILE = ${SRC_DIR}/${DSAT}.cpp
DSAT_H_FILE = ${SRC_DIR}/${DSAT}.h ${SRC_DIR}/Argument.h ${SRC_DIR}/cnfinfo.h ${SRC_DIR}/MyBitSet.h
DSAT_SOURCE_FILES = ${DSAT_H_FILE} ${DSAT_CPP_FILE}

MAPLE_OBJS = Maple/core/Solver.or Maple/simp/SimpSolver.or

TARGET_FILES = 	${DSAT_TARGET} ${MAPLE_OBJS}

UPDATE = update
CLEAN = clean
CLEANUP = cleanup
.PHONY: all ${UPDATE} ${CLEAN} ${CLEANUP}

MAIN_SOURCE_FILE = ${SRC_DIR}/main.cpp

all: ${TARGET} ${UPDATE}

${DSAT_TARGET}: ${DSAT_SOURCE_FILES}
	${COMPILER} ${PRE_CFLAGS} ${DSAT_CPP_FILE} -o ${DSAT_TARGET}

${MAPLE_OBJS}:
	${MAKE} -C Maple/simp rs

${TARGET}: ${MAIN_SOURCE_FILE} ${TARGET_FILES} ${MAPLE_OBJS}
	${COMPILER} ${CFLAGS} ${MAIN_SOURCE_FILE} ${TARGET_FILES} -o ${TARGET} -lz

${UPDATE}:
	@chmod +x ${BIN_DIR}/*

${CLEAN}:
	@rm -f *~
	@rm -f ${SRC_DIR}/*.o
	@rm -f ${SRC_DIR}/*~
	${MAKE} -C Maple/simp clean

${CLEANUP}: ${CLEAN}
	@rm -f ${TARGET}
