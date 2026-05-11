COMPILER = g++
CFLAGS = -O3 -mcmodel=large -std=c++17 -Wall -Wno-sign-compare -Idbg-macro -Iclipp/include
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

NUWLS_TARGET = nuwls/build.o nuwls/deci.o nuwls/heuristic.o nuwls/pms.o
NUWLS_HEADERS = $(wildcard nuwls/*h)
NUWLS_SOURCE_FILES = $(wildcard nuwls/*.cc) ${NUWLS_HEADERS}

CADICAL = cadical
LIB_CADICAL = ${LIB_DIR}/${CADICAL}

TARGET_FILES = 	${DSAT_TARGET} ${NUWLS_TARGET}

UPDATE = update
CLEAN = clean
CLEANUP = cleanup
.PHONY: all ${UPDATE} ${CLEAN} ${CLEANUP}

MAIN_SOURCE_FILE = ${SRC_DIR}/main.cpp

all: ${TARGET} ${UPDATE}

${DSAT_TARGET}: ${DSAT_SOURCE_FILES} ${EXT_MINISAT_HEADERS} ${NUWLS_HEADERS}
	${COMPILER} ${PRE_CFLAGS} ${DSAT_CPP_FILE} -o ${DSAT_TARGET}

${NUWLS_TARGET}: ${NUWLS_SOURCE_FILES}
	${MAKE} -C nuwls

${LIB_CADICAL}: ${CADICAL}/build/lib${CADICAL}.a
	@mkdir -p ${LIB_DIR}
	@cp ${CADICAL}/build/lib${CADICAL}.a lib/

${CADICAL}/build/lib${CADICAL}.a: cadical.diff
	@cd ${CADICAL} && git restore . && git apply ../cadical.diff && ./configure && cd ..
	${MAKE} -C ${CADICAL}
	@rm -rf ${CADICAL}/build/*.o

${TARGET}: ${MAIN_SOURCE_FILE} ${TARGET_FILES} ${LIB_CADICAL}
	${COMPILER} ${CFLAGS} ${MAIN_SOURCE_FILE} ${TARGET_FILES} -o ${TARGET} -L${LIB_DIR} -l${CADICAL}

${UPDATE}:
	@chmod +x ${BIN_DIR}/*

${CLEAN}:
	@rm -f *~
	@rm -f ${SRC_DIR}/*.o
	@rm -f ${SRC_DIR}/*~
	@rm -f ${NUWLS_TARGET}
	@rm -rf ${LIB_DIR}
	@rm -rf ${CADICAL}/build

${CLEANUP}: ${CLEAN}
	@rm -f ${TARGET}
