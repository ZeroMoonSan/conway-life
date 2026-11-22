CC := cc
SRC = main.c libsl/x11/drw.c libsl/util.c
CFLAGS = -std=c99 -pedantic -Wall -Wno-deprecated-declarations -Os -Woverflow
LIBS = -Ilibsl -lX11 -lXft -lfontconfig

all: life
life: config.h
	${CC} ${CFLAGS} ${LIBS} -o $@ ${SRC}
clean:
	rm life
