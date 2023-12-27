CC=gcc
CFLAGS=-g -Og -pedantic -Wall -Wpedantic -Werror -fsanitize=address
SRC=${wildcard ./src/*.c ./src/**/*.c}
OBJ=${patsubst %.c,build/%.o,${SRC}}
INC=-Isrc
UNIT_TEST_SRC=${wildcard ./test/unit/*.c}
UNIT_TEST_OBJ=${patsubst %.c,build/%.o,${UNIT_TEST_SRC}}
FUNC_TEST_SRC=${wildcard ./test/func/*.c}
FUNC_TEST_OBJ=${patsubst %.c,build/%.o,${FUNC_TEST_SRC}}
FUNC_TESTS=${wildcard ./test/func/test/*.sql}
FUNC_RESULTS=${wildcard ./test/func/result/*.out}

all: build/bin/toysqld build/bin/unittest build/bin/functest

build/bin/toysqld: ${OBJ}
	mkdir -p build/bin
	${CC} ${CFLAGS} ${OBJ} -o build/bin/toysqld ${LDFLAGS}

build/./src/%.o: src/%.c
	mkdir -p ${dir $@}
	${CC} ${CFLAGS} ${INC} -o $@ $< -c

build/bin/unittest: ${OBJ} ${UNIT_TEST_OBJ}
	mkdir -p build/bin
	${CC} ${CFLAGS} ${UNIT_TEST_OBJ} ${filter-out %/toysqld.o,${OBJ}} -o build/bin/unittest ${LDFLAGS}

build/bin/functest: ${OBJ} ${FUNC_TEST_OBJ} ${FUNC_TESTS} ${FUNC_RESULTS} build/bin/toysqld
	mkdir -p build/bin
	${CC} ${CFLAGS} ${FUNC_TEST_OBJ} ${filter-out %/toysqld.o,${OBJ}} -o build/bin/functest ${LDFLAGS}
	mkdir -p build/test/func/test
	mkdir -p build/test/func/result
	cp -r test/func/test/ build/test/func/test/
	cp -r test/func/result/ build/test/func/result/

build/./test/%.o: test/%.c
	mkdir -p ${dir $@}
	${CC} ${CFLAGS} ${INC} -o $@ $< -c

.PHONY: clean test

clean:
	rm -rf build

test: build/bin/unittest build/bin/functest
	build/bin/unittest
	build/bin/functest
