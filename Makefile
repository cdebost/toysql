CC=gcc
CFLAGS=-g -Og -pedantic -Wall -Wpedantic -Werror -fsanitize=address
SRC=${wildcard ./src/*.c ./src/**/*.c}
OBJ=${patsubst %.c,build/%.o,${SRC}}
INC=-Isrc
TEST_SRC=${wildcard ./test/*.c}
TEST_OBJ=${patsubst %.c,build/%.o,${TEST_SRC}}

all: build/bin/toysqld build/bin/toysqltest

build/bin/toysqld: ${OBJ}
	mkdir -p build/bin
	${CC} ${CFLAGS} ${OBJ} -o build/bin/toysqld ${LDFLAGS}

build/./src/%.o: src/%.c
	mkdir -p ${dir $@}
	${CC} ${CFLAGS} ${INC} -o $@ $< -c

build/bin/toysqltest: ${OBJ} ${TEST_OBJ}
	mkdir -p build/bin
	${CC} ${CFLAGS} ${TEST_OBJ} ${filter-out %/toysqld.o,${OBJ}} -o build/bin/toysqltest ${LDFLAGS}

build/./test/%.o: test/%.c
	mkdir -p ${dir $@}
	${CC} ${CFLAGS} ${INC} -o $@ $< -c

.PHONY: clean test

clean:
	rm -rf build

check: build/bin/toysqltest
	build/bin/toysqltest
