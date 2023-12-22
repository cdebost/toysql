CC=gcc
CFLAGS=-g -pedantic -Wall -Wpedantic -Werror -fsanitize=address
SRC=${wildcard ./src/*.c}
OBJ=${patsubst %.c,build/%.o,${SRC}}

all: toysqld

toysqld: ${OBJ}
	${CC} ${CFLAGS} ${OBJ} -o toysqld ${LDFLAGS}

build/%.o: %.c
	mkdir -p ${dir $@}
	${CC} ${CFLAGS} -o $@ $< -c
