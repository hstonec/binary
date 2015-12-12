CC=cc
PROG=sish
CFLAGS=-g -Wall

all: ${PROG}

${PROG}: main.c sish.o func.o jstring.o arraylist.o macros.h sish.h func.h
	$(CC) ${CFLAGS} -o ${PROG} main.c sish.o func.o jstring.o arraylist.o \
	-lbsd

sish.o: sish.c macros.h sish.h func.h
	$(CC) ${CFLAGS} -c sish.c
	
func.o: func.c func.h
	$(CC) ${CFLAGS} -c func.c
	
jstring.o: jstring.c jstring.h
	$(CC) ${CFLAGS} -c jstring.c

arraylist.o: arraylist.c arraylist.h
	$(CC) ${CFLAGS} -c arraylist.c

.PHONY: clean
clean:
	-rm sish sish.o func.o jstring.o arraylist.o
