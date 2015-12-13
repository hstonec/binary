CC=cc
PROG=sish
CFLAGS=-g -Wall

all: ${PROG}

${PROG}: main.c sish.o parse.o builtin.o func.o jstring.o arraylist.o
	$(CC) ${CFLAGS} -o ${PROG} main.c sish.o parse.o builtin.o func.o jstring.o arraylist.o \
	-lbsd

sish.o: sish.c parse.o builtin.o func.o jstring.o arraylist.o sish.h macros.h
	$(CC) ${CFLAGS} -c sish.c
	
parse.o: parse.c jstring.o arraylist.o parse.h	
	$(CC) ${CFLAGS} -c parse.c
	
builtin.o: builtin.c jstring.o arraylist.o builtin.h sish.h macros.h
	$(CC) ${CFLAGS} -c builtin.c
	
func.o: func.c func.h
	$(CC) ${CFLAGS} -c func.c
	
jstring.o: jstring.c jstring.h
	$(CC) ${CFLAGS} -c jstring.c

arraylist.o: arraylist.c arraylist.h
	$(CC) ${CFLAGS} -c arraylist.c

.PHONY: clean
clean:
	-rm sish sish.o parse.o builtin.o func.o jstring.o arraylist.o
